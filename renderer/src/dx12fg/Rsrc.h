#pragma once

#include "../core/core.h"

#include <variant>
#include <unordered_map>

namespace Ubpa::DX12::FG {
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
}

