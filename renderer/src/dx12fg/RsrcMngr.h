#pragma once

#include "Rsrc.h"

#include <UFG/UFG.h>

#include <unordered_set>

namespace Ubpa::DX12::FG {
	class RsrcMngr {
	public:
		void Init(GCmdList uGCmdList, Device uDevice) {
			this->uGCmdList = uGCmdList;
			this->uDevice = uDevice;
		}

		void NewFrame();
		void DHReserve(const Ubpa::FG::Compiler::Result& crst);
		void Clear();

		void Construct(size_t rsrcNodeIdx);
		void Destruct(size_t rsrcNodeIdx);

		PassRsrcs RequestPassRsrcs(size_t passNodeIdx);

		RsrcMngr& RegisterImportedRsrc(size_t rsrcNodeIdx, SRsrcView view) {
			importeds[rsrcNodeIdx] = view;
			return *this;
		}

		RsrcMngr& RegisterTemporalRsrc(size_t rsrcNodeIdx, RsrcType type) {
			temporals[rsrcNodeIdx] = type;
			return *this;
		}

		RsrcMngr& RegisterPassRsrcs(size_t passNodeIdx, size_t rsrcNodeIdx, RsrcState state, RsrcImplDesc desc) {
			passNodeIdx2rsrcs[passNodeIdx].emplace_back(rsrcNodeIdx, state, desc);
			return *this;
		}

		bool IsImported(size_t rsrcNodeIdx) const noexcept {
			return importeds.find(rsrcNodeIdx) != importeds.end();
		}

	private:
		void SrvDHReserve(UINT num);
		void DsvDHReserve(UINT num);
		void RtvDHReserve(UINT num);

		GCmdList uGCmdList;
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

		struct DHIndices {
			std::unordered_map<D3D12_DEPTH_STENCIL_VIEW_DESC, UINT>   desc2idx_dsv;
			std::unordered_map<D3D12_RENDER_TARGET_VIEW_DESC, UINT>   desc2idx_rtv;
			std::unordered_map<D3D12_SHADER_RESOURCE_VIEW_DESC, UINT> desc2idx_srv;

			UINT nullidx_dsv{ static_cast<UINT>(-1) };
			UINT nullidx_rtv{ static_cast<UINT>(-1) };
			UINT nullidx_srv{ static_cast<UINT>(-1) };
		};
		// rsrcNodeIdx -> type
		std::unordered_map<size_t, DHIndices> impls;

		DescriptorHeap srvDH;
		std::vector<UINT> srvDHfree;
		std::unordered_set<UINT> srvDHused;

		DescriptorHeap rtvDH;
		std::vector<UINT> rtvDHfree;
		std::unordered_set<UINT> rtvDHused;

		DescriptorHeap dsvDH;
		std::vector<UINT> dsvDHfree;
		std::unordered_set<UINT> dsvDHused;
	};

}