#pragma once

#include "../common/d3dUtil.h"

namespace Ubpa::DX12 {
    using namespace Microsoft::WRL;

    template<typename T>
    struct ComPtrHolder {
        Microsoft::WRL::ComPtr<T> raw;
        T* operator->() noexcept { return raw.Get(); }
        const T* operator->() const noexcept { return raw.Get(); }
    };

    // simple API
    struct Device : ComPtrHolder<ID3D12Device> {
        void CreateCommittedResource(
            D3D12_HEAP_TYPE heap_type,
            SIZE_T size,
            ID3D12Resource** resources);

        void CreateDescriptorHeap(UINT size, D3D12_DESCRIPTOR_HEAP_TYPE type,
            ID3D12DescriptorHeap** pHeap);
    };

    // raw : Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>
    // .   : simple API
    // ->  : raw API
    struct GraphicsCommandList : ComPtrHolder<ID3D12GraphicsCommandList> {
        void Reset(
            ID3D12CommandAllocator* pAllocator,
            ID3D12PipelineState* pInitialState = nullptr);
        void Execute(ID3D12CommandQueue*);

        void ResourceBarrier(ID3D12Resource* resource, D3D12_RESOURCE_STATES from, D3D12_RESOURCE_STATES to);
        
        // different type
        template<typename... Heaps,
            typename = std::enable_if_t<
            sizeof...(Heaps) < static_cast<SIZE_T>(D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES) &&
            (std::is_same_v<Heaps, ID3D12DescriptorHeap>&&...) >>
        void SetDescriptorHeaps(Heaps*... heaps);

        void ClearRenderTargetView(D3D12_CPU_DESCRIPTOR_HANDLE RenderTargetView, const FLOAT color[4]);
        void ClearDepthStencilView(D3D12_CPU_DESCRIPTOR_HANDLE DepthStencilView);

        void RSSetViewport(D3D12_VIEWPORT viewport);
        void RSSetScissorRect(D3D12_RECT rect);

        // simple version
        // pro: OMSetRenderTargets
        void OMSetRenderTarget(
            D3D12_CPU_DESCRIPTOR_HANDLE rendertarget,
            D3D12_CPU_DESCRIPTOR_HANDLE depthstencil);

        // one instance
        void DrawIndexed(
            UINT IndexCount,
            UINT StartIndexLocation,
            INT BaseVertexLocation);
    };

    // CPU buffer
    // raw : Microsoft::WRL::ComPtr<ID3DBlob>
    // .   : simple API
    // ->  : raw API
    struct Blob : ComPtrHolder<ID3DBlob> {
        void Create(SIZE_T size);
        void Copy(const void* data, SIZE_T size);
        void Create(const void* data, SIZE_T size);
    };
}

#include "core.inl"
