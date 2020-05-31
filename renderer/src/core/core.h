#pragma once

#include "../common/d3dUtil.h"

namespace Ubpa::DX12 {
    using namespace Microsoft::WRL;

    // simple API
    struct Device {
        ID3D12Device* device;

        ID3D12Device* operator->() noexcept { return device; }

        void CreateCommittedResource(
            D3D12_HEAP_TYPE heap_type,
            size_t size,
            ID3D12Resource** resources);
    };

    // raw : raw ID3D12GraphicsCommandList pointer
    // .   : simple API
    // ->  : raw API
    struct GraphicsCommandList {
        ID3D12GraphicsCommandList* raw;

        ID3D12GraphicsCommandList* operator->() noexcept { return raw; }

        void Reset(ID3D12CommandAllocator* pAllocator,
            ID3D12PipelineState* pInitialState = nullptr);
        void Execute(ID3D12CommandQueue*);

        void ResourceBarrier(ID3D12Resource* resource, D3D12_RESOURCE_STATES from, D3D12_RESOURCE_STATES to);

        void ClearRenderTargetView(D3D12_CPU_DESCRIPTOR_HANDLE RenderTargetView, const FLOAT color[4]);
        void ClearDepthStencilView(D3D12_CPU_DESCRIPTOR_HANDLE DepthStencilView);

        void RSSetViewport(D3D12_VIEWPORT viewport);
        void RSSetScissorRect(D3D12_RECT rect);

        // simple version
        // pro: OMSetRenderTargets
        void OMSetRenderTarget(D3D12_CPU_DESCRIPTOR_HANDLE rendertarget,
            D3D12_CPU_DESCRIPTOR_HANDLE depthstencil);
    };
}
