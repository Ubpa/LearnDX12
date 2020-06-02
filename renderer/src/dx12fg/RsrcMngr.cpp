#include "RsrcMngr.h"

using namespace Ubpa::DX12;

void FG::RsrcMngr::Clear() {
	// pool.clear();

	importeds.clear();
	temporals.clear();
	passNodeIdx2rsrcs.clear();
	actives.clear();

	// ================

	UINT srvDH_target_size = 128;
	UINT rtvDH_target_size = 128;
	UINT dsvDH_target_size = 128;

	if (srvDH.pDH.Get() == nullptr || srvDH.pDH->GetDesc().NumDescriptors < srvDH_target_size) {
		srvDH.Create(uDevice.raw.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, srvDH_target_size, true);
		srvDHfree.clear();
		for (UINT i = 0; i < srvDH_target_size; i++)
			srvDHfree.push_back(i);
	}
	else {
		for (auto i : srvDHused)
			srvDHfree.push_back(i);
	}

	if (rtvDH.pDH.Get() == nullptr || rtvDH.pDH->GetDesc().NumDescriptors < rtvDH_target_size) {
		rtvDH.Create(uDevice.raw.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_RTV, rtvDH_target_size, false);
		rtvDHfree.clear();
		for (UINT i = 0; i < rtvDH_target_size; i++)
			rtvDHfree.push_back(i);
	}
	else {
		for (auto i : rtvDHused)
			rtvDHfree.push_back(i);
	}

	if (dsvDH.pDH.Get() == nullptr || dsvDH.pDH->GetDesc().NumDescriptors < dsvDH_target_size) {
		dsvDH.Create(uDevice.raw.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_DSV, dsvDH_target_size, false);
		dsvDHfree.clear();
		for (UINT i = 0; i < dsvDH_target_size; i++)
			dsvDHfree.push_back(i);
	}
	else {
		for (auto i : dsvDHused)
			dsvDHfree.push_back(i);
	}

	srvDHused.clear();
	rtvDHused.clear();
	dsvDHused.clear();

	rsrc2idxSrvDH.clear();
	rsrc2idxRtvDH.clear();
	rsrc2idxDsvDH.clear();
}

void FG::RsrcMngr::Construct(size_t rsrcNodeIdx) {
	SRsrcView view;

	if (IsImported(rsrcNodeIdx)) {
		view = importeds[rsrcNodeIdx];
		//cout << "[Construct] Import | " << name << " @" << rsrc.buffer << endl;
	}
	else {
		auto type = temporals[rsrcNodeIdx];
		auto& typefrees = pool[type];
		if (typefrees.empty()) {
			view.state = D3D12_RESOURCE_STATE_COMMON;
			RsrcPtr ptr;
			ThrowIfFailed(uDevice->CreateCommittedResource(
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
				D3D12_HEAP_FLAG_NONE,
				&type,
				view.state,
				nullptr,
				IID_PPV_ARGS(ptr.GetAddressOf())));
			rsrcKeeper.push_back(ptr);
			view.pRsrc = ptr.Get();
			//cout << "[Construct] Create | " << name << " @" << rsrc.buffer << endl;
		}
		else {
			view = typefrees.back();
			typefrees.pop_back();
			//cout << "[Construct] Init | " << name << " @" << rsrc.buffer << endl;
		}
	}
	actives[rsrcNodeIdx] = view;
}

void FG::RsrcMngr::Destruct(size_t rsrcNodeIdx) {
	auto view = actives[rsrcNodeIdx];
	if (!IsImported(rsrcNodeIdx)) {
		pool[temporals[rsrcNodeIdx]].push_back(view);
		//cout << "[Destruct] Recycle | " << name << " @" << rsrc.buffer << endl;
	}
	else {
		auto orig_state = importeds[rsrcNodeIdx].state;
		if (view.state != orig_state) {
			uGCmdList.ResourceBarrier(
				view.pRsrc,
				view.state,
				orig_state);
		}
		//cout << "[Destruct] Import | " << name << " @" << rsrc.buffer << endl;
	}

	actives.erase(rsrcNodeIdx);
}

FG::PassRsrcs FG::RsrcMngr::RequestPassRsrcs(size_t passNodeIdx) {
	PassRsrcs passRsrc;
	const auto& rsrcStates = passNodeIdx2rsrcs[passNodeIdx];
	for (const auto& [rsrcNodeIdx, state, desc] : rsrcStates) {
		auto& view = actives[rsrcNodeIdx];

		if (view.state != state) {
			uGCmdList.ResourceBarrier(
				view.pRsrc,
				view.state,
				state);
			view.state = state;
		}

		passRsrc[rsrcNodeIdx] = std::visit([&](const auto& desc) -> RsrcImplHandle {
			using T = std::decay_t<decltype(desc)>;

			if constexpr (std::is_same_v<T, D3D12_SHADER_RESOURCE_VIEW_DESC>
				|| std::is_same_v<T, RsrcImplDesc_SRV_NULL>)
			{
				UINT idx;

				const D3D12_SHADER_RESOURCE_VIEW_DESC* pdesc;
				if constexpr (std::is_same_v<T, D3D12_SHADER_RESOURCE_VIEW_DESC>)
					pdesc = &desc;
				else
					pdesc = nullptr;

				auto target = rsrc2idxSrvDH.find(view.pRsrc);
				if (target != rsrc2idxSrvDH.end())
					idx = target->second;
				else {
					idx = rtvDHfree.back();
					rtvDHfree.pop_back();
					rtvDHused.insert(idx);
					uDevice->CreateShaderResourceView(view.pRsrc, pdesc, rtvDH.hCPU(idx));
					rsrc2idxSrvDH[view.pRsrc] = idx;
				}

				return { srvDH.hCPU(idx), srvDH.hGPU(idx) };
			}
			else if constexpr (std::is_same_v<T, D3D12_RENDER_TARGET_VIEW_DESC>
				|| std::is_same_v<T, RsrcImplDesc_RTV_Null>)
			{
				UINT idx;

				const D3D12_RENDER_TARGET_VIEW_DESC* pdesc;
				if constexpr (std::is_same_v<T, D3D12_RENDER_TARGET_VIEW_DESC>)
					pdesc = &desc;
				else
					pdesc = nullptr;

				auto target = rsrc2idxRtvDH.find(view.pRsrc);
				if (target != rsrc2idxRtvDH.end())
					idx = target->second;
				else {
					idx = rtvDHfree.back();
					rtvDHfree.pop_back();
					rtvDHused.insert(idx);
					uDevice->CreateRenderTargetView(view.pRsrc, pdesc, rtvDH.hCPU(idx));
					rsrc2idxRtvDH[view.pRsrc] = idx;
				}

				return { rtvDH.hCPU(idx), {0} };
			}
			else if constexpr (std::is_same_v<T, D3D12_DEPTH_STENCIL_VIEW_DESC>
				|| std::is_same_v<T, RsrcImplDesc_DSV_Null>)
			{
				UINT idx;

				const D3D12_DEPTH_STENCIL_VIEW_DESC* pdesc;
				if constexpr (std::is_same_v<T, D3D12_DEPTH_STENCIL_VIEW_DESC>)
					pdesc = &desc;
				else
					pdesc = nullptr;

				auto target = rsrc2idxDsvDH.find(view.pRsrc);
				if (target != rsrc2idxDsvDH.end())
					idx = target->second;
				else {
					idx = dsvDHfree.back();
					dsvDHfree.pop_back();
					dsvDHused.insert(idx);
					uDevice->CreateDepthStencilView(view.pRsrc, pdesc, dsvDH.hCPU(idx));
					rsrc2idxDsvDH[view.pRsrc] = idx;
				}

				return { dsvDH.hCPU(idx), {0} };
			}
			else
				static_assert(always_false_v<T>, "non-exhaustive visitor!");
			}, desc);
	}
	return passRsrc;
}
