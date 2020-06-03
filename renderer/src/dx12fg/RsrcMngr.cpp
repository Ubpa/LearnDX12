#include "RsrcMngr.h"

using namespace Ubpa::DX12;

void FG::RsrcMngr::NewFrame() {
	importeds.clear();
	temporals.clear();
	passNodeIdx2rsrcs.clear();
	actives.clear();

	assert(srvDHfree.size() == srvDH.Size());
	assert(rtvDHfree.size() == rtvDH.Size());
	assert(dsvDHfree.size() == dsvDH.Size());
	assert(srvDHused.empty());
	assert(rtvDHused.empty());
	assert(dsvDHused.empty());

	assert(impls.empty());
}

void FG::RsrcMngr::Clear() {
	NewFrame();
	rsrcKeeper.clear();
	pool.clear();
}

void FG::RsrcMngr::SrvDHReserve(UINT num) {
	assert(srvDHused.empty());

	UINT origSize = srvDH.Size();
	if (origSize >= num)
		return;

	srvDH.Create(uDevice.raw.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, num, true);

	for (UINT i = origSize; i < num; i++)
		srvDHfree.push_back(i);
}

void FG::RsrcMngr::RtvDHReserve(UINT num) {
	assert(rtvDHused.empty());

	UINT origSize = rtvDH.Size();
	if (origSize >= num)
		return;

	rtvDH.Create(uDevice.raw.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_RTV, num, false);

	for (UINT i = origSize; i < num; i++)
		rtvDHfree.push_back(i);
}

void FG::RsrcMngr::DsvDHReserve(UINT num) {
	assert(dsvDHused.empty());

	UINT origSize = dsvDH.Size();
	if (dsvDH.Size() >= num)
		return;

	dsvDH.Create(uDevice.raw.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_DSV, num, false);

	for (UINT i = origSize; i < num; i++)
		dsvDHfree.push_back(i);
}

void FG::RsrcMngr::DHReserve(const Ubpa::FG::Compiler::Result& crst) {
	UINT maxSRV = 0;
	UINT maxRTV = 0;
	UINT maxDSV = 0;
	UINT curSRV = 0;
	UINT curRTV = 0;
	UINT curDSV = 0;
	
	struct DHRecord {
		std::unordered_set<D3D12_DEPTH_STENCIL_VIEW_DESC>   descs_dsv;
		std::unordered_set<D3D12_RENDER_TARGET_VIEW_DESC>   descs_rtv;
		std::unordered_set<D3D12_SHADER_RESOURCE_VIEW_DESC> descs_srv;

		bool null_dsv{ false };
		bool null_rtv{ false };
		bool null_srv{ false };
	};
	std::unordered_map<size_t, DHRecord> rsrc2record;
	for (auto passNodeIdx : crst.sortedPasses) {
		for (const auto& [rsrcNodeIdx, state, desc] : passNodeIdx2rsrcs[passNodeIdx]) {
			auto& record = rsrc2record[rsrcNodeIdx];
			std::visit([&](const auto& desc) {
				using T = std::decay_t<decltype(desc)>;
				if constexpr (std::is_same_v<T, D3D12_SHADER_RESOURCE_VIEW_DESC>)
				{
					if (record.descs_srv.find(desc) != record.descs_srv.end())
						return;
					record.descs_srv.insert(desc);
					curSRV++;
					if (curSRV > maxSRV)
						maxSRV = curSRV;
				}
				else if constexpr (std::is_same_v<T, D3D12_RENDER_TARGET_VIEW_DESC>)
				{
					if (record.descs_rtv.find(desc) != record.descs_rtv.end())
						return;
					record.descs_rtv.insert(desc);
					curRTV++;
					if (curRTV > maxRTV)
						maxRTV = curRTV;
				}
				else if constexpr (std::is_same_v<T, D3D12_DEPTH_STENCIL_VIEW_DESC>)
				{
					if (record.descs_dsv.find(desc) != record.descs_dsv.end())
						return;
					record.descs_dsv.insert(desc);
					curDSV++;
					if (curDSV > maxDSV)
						maxDSV = curDSV;
				}
				else if constexpr (std::is_same_v<T, RsrcImplDesc_SRV_NULL>) {
					if (record.null_srv)
						return;
					record.null_srv = true;
					curSRV++;
					if (curSRV > maxSRV)
						maxSRV = curSRV;
				}
				else if constexpr (std::is_same_v<T, RsrcImplDesc_RTV_Null>) {
					if (record.null_rtv)
						return;
					record.null_rtv = true;
					curRTV++;
					if (curRTV > maxRTV)
						maxRTV = curRTV;
				}
				else if constexpr (std::is_same_v<T, RsrcImplDesc_DSV_Null>) {
					if (record.null_dsv)
						return;
					record.null_dsv = true;
					curDSV++;
					if (curDSV > maxDSV)
						maxDSV = curDSV;
				}
				else
					static_assert(always_false_v<T>, "non-exhaustive visitor!");
				}, desc);
		}
		const auto& info = crst.idx2info.find(passNodeIdx)->second;
		for (auto rsrcNodeIdx : info.destructRsrcs) {
			const auto& record = rsrc2record[rsrcNodeIdx];

			curSRV -= static_cast<UINT>(record.descs_srv.size());
			curRTV -= static_cast<UINT>(record.descs_rtv.size());
			curDSV -= static_cast<UINT>(record.descs_dsv.size());

			curSRV -= record.null_srv;
			curRTV -= record.null_rtv;
			curDSV -= record.null_dsv;

			rsrc2record.erase(rsrcNodeIdx);
		}
	}

	SrvDHReserve(maxSRV);
	RtvDHReserve(maxRTV);
	DsvDHReserve(maxDSV);
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

	const auto& indices = impls[rsrcNodeIdx];
	for (auto [desv, idx] : indices.desc2idx_srv) {
		srvDHused.erase(idx);
		srvDHfree.push_back(idx);
	}
	for (auto [desv, idx] : indices.desc2idx_rtv) {
		rtvDHused.erase(idx);
		rtvDHfree.push_back(idx);
	}
	for (auto [desv, idx] : indices.desc2idx_dsv) {
		dsvDHused.erase(idx);
		dsvDHfree.push_back(idx);
	}
	if (indices.nullidx_srv != static_cast<UINT>(-1)) {
		srvDHused.erase(indices.nullidx_srv);
		srvDHfree.push_back(indices.nullidx_srv);
	}
	if (indices.nullidx_rtv != static_cast<UINT>(-1)) {
		rtvDHused.erase(indices.nullidx_rtv);
		rtvDHfree.push_back(indices.nullidx_rtv);
	}
	if (indices.nullidx_dsv != static_cast<UINT>(-1)) {
		dsvDHused.erase(indices.nullidx_dsv);
		dsvDHfree.push_back(indices.nullidx_dsv);
	}

	impls.erase(rsrcNodeIdx);
}

FG::PassRsrcs FG::RsrcMngr::RequestPassRsrcs(size_t passNodeIdx) {
	PassRsrcs passRsrc;
	const auto& rsrcStates = passNodeIdx2rsrcs[passNodeIdx];
	for (const auto& [rsrcNodeIdx, state, desc] : rsrcStates) {
		auto& view = actives[rsrcNodeIdx];
		auto& implMaps = impls[rsrcNodeIdx];

		if (view.state != state) {
			uGCmdList.ResourceBarrier(
				view.pRsrc,
				view.state,
				state);
			view.state = state;
		}

		passRsrc[rsrcNodeIdx] = std::visit([&](const auto& desc) -> RsrcImplHandle {
			using T = std::decay_t<decltype(desc)>;

			UINT* pIdx;

			if constexpr (std::is_same_v<T, D3D12_SHADER_RESOURCE_VIEW_DESC>
				|| std::is_same_v<T, RsrcImplDesc_SRV_NULL>)
			{
				if constexpr (std::is_same_v<T, D3D12_SHADER_RESOURCE_VIEW_DESC>) {
					auto target_idx = implMaps.desc2idx_srv.find(desc);
					if (target_idx != implMaps.desc2idx_srv.end())
						pIdx = &target_idx->second;
					else {
						pIdx = &implMaps.desc2idx_srv[desc];
						*pIdx = static_cast<UINT>(-1);
					}
				}
				else // std::is_same_v<T, RsrcImplDesc_SRV_NULL>
					pIdx = &implMaps.nullidx_srv;

				UINT& idx = *pIdx;
				if (idx == static_cast<UINT>(-1)) {
					idx = srvDHfree.back();
					srvDHfree.pop_back();
					srvDHused.insert(idx);

					const D3D12_SHADER_RESOURCE_VIEW_DESC* pdesc;
					if constexpr (std::is_same_v<T, D3D12_SHADER_RESOURCE_VIEW_DESC>)
						pdesc = &desc;
					else
						pdesc = nullptr;

					uDevice->CreateShaderResourceView(view.pRsrc, pdesc, rtvDH.hCPU(idx));
				}

				return { srvDH.hCPU(idx), srvDH.hGPU(idx) };
			}
			else if constexpr (std::is_same_v<T, D3D12_RENDER_TARGET_VIEW_DESC>
				|| std::is_same_v<T, RsrcImplDesc_RTV_Null>)
			{
				if constexpr (std::is_same_v<T, D3D12_RENDER_TARGET_VIEW_DESC>) {
					auto target_idx = implMaps.desc2idx_rtv.find(desc);
					if (target_idx != implMaps.desc2idx_rtv.end())
						pIdx = &target_idx->second;
					else {
						pIdx = &implMaps.desc2idx_rtv[desc];
						*pIdx = static_cast<UINT>(-1);
					}
				}
				else // std::is_same_v<T, RsrcImplDesc_SRV_NULL>
					pIdx = &implMaps.nullidx_rtv;

				UINT& idx = *pIdx;
				if (idx == static_cast<UINT>(-1)) {
					idx = rtvDHfree.back();
					rtvDHfree.pop_back();
					rtvDHused.insert(idx);

					const D3D12_RENDER_TARGET_VIEW_DESC* pdesc;
					if constexpr (std::is_same_v<T, D3D12_RENDER_TARGET_VIEW_DESC>)
						pdesc = &desc;
					else
						pdesc = nullptr;

					uDevice->CreateRenderTargetView(view.pRsrc, pdesc, rtvDH.hCPU(idx));
				}

				return { rtvDH.hCPU(idx), {0} };
			}
			else if constexpr (std::is_same_v<T, D3D12_DEPTH_STENCIL_VIEW_DESC>
				|| std::is_same_v<T, RsrcImplDesc_DSV_Null>)
			{
				if constexpr (std::is_same_v<T, D3D12_DEPTH_STENCIL_VIEW_DESC>) {
					auto target_idx = implMaps.desc2idx_dsv.find(desc);
					if (target_idx != implMaps.desc2idx_dsv.end())
						pIdx = &target_idx->second;
					else {
						pIdx = &implMaps.desc2idx_dsv[desc];
						*pIdx = static_cast<UINT>(-1);
					}
				}
				else // std::is_same_v<T, RsrcImplDesc_SRV_NULL>
					pIdx = &implMaps.nullidx_dsv;

				UINT& idx = *pIdx;
				if (idx == static_cast<UINT>(-1)) {
					idx = dsvDHfree.back();
					dsvDHfree.pop_back();
					dsvDHused.insert(idx);

					const D3D12_DEPTH_STENCIL_VIEW_DESC* pdesc;
					if constexpr (std::is_same_v<T, D3D12_DEPTH_STENCIL_VIEW_DESC>)
						pdesc = &desc;
					else
						pdesc = nullptr;

					uDevice->CreateDepthStencilView(view.pRsrc, pdesc, dsvDH.hCPU(idx));
				}

				return { dsvDH.hCPU(idx), {0} };
			}
			else
				static_assert(always_false_v<T>, "non-exhaustive visitor!");
			}, desc);
	}
	return passRsrc;
}
