#pragma once

#include "../core/core.h"

#include <UFG/UFG.h>

#include <unordered_set>
#include <variant>
#include <functional>

namespace Ubpa::detail::DX12::FG::ResourceMngr_ {
	template <class T>
	inline void hash_combine(std::size_t& s, const T& v)
	{
		std::hash<T> h;
		s ^= h(v) + 0x9e3779b9 + (s << 6) + (s >> 2);
	}
}

namespace std {
	template<>
	struct hash<D3D12_RESOURCE_DESC> {
		size_t operator()(const D3D12_RESOURCE_DESC& desc) const noexcept {
			size_t rst = 0;
			for (size_t i = 0; i < sizeof(D3D12_RESOURCE_DESC); i++)
				Ubpa::detail::DX12::FG::ResourceMngr_::hash_combine(rst, reinterpret_cast<const char*>(&desc)[i]);
			return rst;
		}
	};
	template<>
	struct hash<D3D12_SHADER_RESOURCE_VIEW_DESC> {
		size_t operator()(const D3D12_SHADER_RESOURCE_VIEW_DESC& desc) const noexcept {
			size_t rst = 0;
			for (size_t i = 0; i < sizeof(D3D12_SHADER_RESOURCE_VIEW_DESC); i++)
				Ubpa::detail::DX12::FG::ResourceMngr_::hash_combine(rst, reinterpret_cast<const char*>(&desc)[i]);
			return rst;
		}
	};
	template<>
	struct hash<D3D12_RENDER_TARGET_VIEW_DESC> {
		size_t operator()(const D3D12_RENDER_TARGET_VIEW_DESC& desc) const noexcept {
			size_t rst = 0;
			for (size_t i = 0; i < sizeof(D3D12_RENDER_TARGET_VIEW_DESC); i++)
				Ubpa::detail::DX12::FG::ResourceMngr_::hash_combine(rst, reinterpret_cast<const char*>(&desc)[i]);
			return rst;
		}
	};
}

namespace Ubpa::DX12::FG {
	class ResourceMngr {
		template<class>
		static constexpr bool always_false_v = false;
	public:
		using Rsrc = ID3D12Resource;
		using RsrcPtr = ComPtr<Rsrc>;
		using RsrcState = D3D12_RESOURCE_STATES;
		using RsrcType = D3D12_RESOURCE_DESC;
		struct RsrcImplDesc_SRV_NULL {};
		struct RsrcImplDesc_RTV_Null {};
		struct RsrcImplDesc_DSV_Null {};
		using RsrcImplDesc = std::variant<
			D3D12_SHADER_RESOURCE_VIEW_DESC,
			D3D12_RENDER_TARGET_VIEW_DESC,
			D3D12_DEPTH_STENCIL_VIEW_DESC,
			RsrcImplDesc_SRV_NULL,
			RsrcImplDesc_RTV_Null,
			RsrcImplDesc_DSV_Null>;
		struct RsrcImplHandle {
			D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle;
			D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle;
		};
		using PassRsrcs = std::unordered_map<size_t, RsrcImplHandle>;
		struct SRsrcView {
			Rsrc* pRsrc;
			RsrcState state;
		};
		struct SRsrc {
			RsrcPtr pRsrc;
			RsrcState state;
		};

		void Init(GraphicsCommandList uGCmdList, Device uDevice) {
			this->uGCmdList = uGCmdList;
			this->uDevice = uDevice;
		}

		void Clear() {
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

		void Construct(size_t rsrcNodeIdx) {
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

		void Destruct(size_t rsrcNodeIdx) {
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

		PassRsrcs RequestPassRsrcs(size_t passNodeIdx) {
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

		ResourceMngr& RegisterImportedRsrc(size_t rsrcNodeIdx, SRsrcView view) {
			importeds[rsrcNodeIdx] = view;
			return *this;
		}

		ResourceMngr& RegisterTemporalRsrc(size_t rsrcNodeIdx, RsrcType type) {
			temporals[rsrcNodeIdx] = type;
			return *this;
		}

		ResourceMngr& RegisterPassRsrcs(size_t passNodeIdx, size_t rsrcNodeIdx, RsrcState state, RsrcImplDesc desc) {
			passNodeIdx2rsrcs[passNodeIdx].emplace_back(rsrcNodeIdx, state, desc);
			return *this;
		}

		bool IsImported(size_t rsrcNodeIdx) const noexcept {
			return importeds.find(rsrcNodeIdx) != importeds.end();
		}

	private:
		GraphicsCommandList uGCmdList;
		Device uDevice;

		// type -> vector<view>
		std::vector<RsrcPtr> rsrcKeeper;
		std::unordered_map<RsrcType, std::vector<SRsrcView>> pool;

		// rsrcNodeIdx -> view
		std::unordered_map<size_t, SRsrcView> importeds;
		// rsrcNodeIdx -> type
		std::unordered_map<size_t, RsrcType> temporals;
		// passNodeIdx -> vector<(rsrcNodeIdx, RsrcState, RsrcImplDesc)>
		std::unordered_map<size_t, std::vector<std::tuple<size_t, RsrcState, RsrcImplDesc>>> passNodeIdx2rsrcs;
		// rsrcNodeIdx -> view
		std::unordered_map<size_t, SRsrcView> actives;


		DescriptorHeap srvDH;
		std::vector<UINT> srvDHfree;
		std::unordered_set<UINT> srvDHused;
		// rsrc ptr -> index in srvDH
		std::unordered_map<Rsrc*, UINT> rsrc2idxSrvDH;

		DescriptorHeap rtvDH;
		std::vector<UINT> rtvDHfree;
		std::unordered_set<UINT> rtvDHused;
		// rsrc ptr -> index in rtvDH
		std::unordered_map<Rsrc*, UINT> rsrc2idxRtvDH;

		DescriptorHeap dsvDH;
		std::vector<UINT> dsvDHfree;
		std::unordered_set<UINT> dsvDHused;
		// rsrc ptr -> index in dsvDH
		std::unordered_map<Rsrc*, UINT> rsrc2idxDsvDH;
	};

	class Executor {
	public:
		using PassFuncSig = void(const ResourceMngr::PassRsrcs&);

		Executor& RegisterPassFunc(size_t passNodeIdx, std::function<PassFuncSig> func) {
			passFuncs[passNodeIdx] = std::move(func);
			return *this;
		}

		void Clear() {
			passFuncs.clear();
		}

		void Execute(
			const Ubpa::FG::FrameGraph& fg,
			const Ubpa::FG::Compiler::Result& crst,
			ResourceMngr& rsrcMngr)
		{
			const auto& passnodes = fg.GetPassNodes();
			for (auto passNodeIdx : crst.sortedPasses) {
				const auto& passinfo = crst.idx2info.find(passNodeIdx)->second;

				for (const auto& rsrc : passinfo.constructRsrcs)
					rsrcMngr.Construct(rsrc);

				auto passRsrcs = rsrcMngr.RequestPassRsrcs(passNodeIdx);
				passFuncs[passNodeIdx](passRsrcs);

				//cout << "[Execute] " << passnodes[i].Name() << endl;

				for (const auto& rsrc : passinfo.destructRsrcs)
					rsrcMngr.Destruct(rsrc);
			}
		}

	private:
		std::unordered_map<size_t, std::function<PassFuncSig>> passFuncs;
	};
}

