#pragma once

#include "Rsrc.h"

#include "detail/RsrcMngr.inl"

#include <unordered_set>

namespace Ubpa::DX12::FG {
	class RsrcMngr {
	public:
		void Init(GraphicsCommandList uGCmdList, Device uDevice) {
			this->uGCmdList = uGCmdList;
			this->uDevice = uDevice;
		}

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

}