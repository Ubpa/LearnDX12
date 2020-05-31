#include "core.h"

using namespace Ubpa;

void DX12::Device::CreateCommittedResource(
    D3D12_HEAP_TYPE heap_type,
    SIZE_T size,
    ID3D12Resource** resources)
{
    ThrowIfFailed(raw->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(size),
        D3D12_RESOURCE_STATE_COMMON,
        nullptr,
        IID_PPV_ARGS(resources)));
}

void DX12::Device::CreateDescriptorHeap(UINT size, D3D12_DESCRIPTOR_HEAP_TYPE type,
    ID3D12DescriptorHeap** pHeap)
{
    D3D12_DESCRIPTOR_HEAP_DESC cbvHeapDesc;
    cbvHeapDesc.NumDescriptors = size;
    cbvHeapDesc.Type = type;
    cbvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    cbvHeapDesc.NodeMask = 0;
    ThrowIfFailed(raw->CreateDescriptorHeap(&cbvHeapDesc,
        IID_PPV_ARGS(pHeap)));
}

void DX12::GraphicsCommandList::Reset(ID3D12CommandAllocator* pAllocator,
    ID3D12PipelineState* pInitialState) {
    ThrowIfFailed(raw->Reset(pAllocator, pInitialState));
}

void DX12::GraphicsCommandList::Execute(ID3D12CommandQueue* queue) {
    queue->ExecuteCommandLists(1, reinterpret_cast<ID3D12CommandList* const*>(raw.GetAddressOf()));
}

void DX12::GraphicsCommandList::ResourceBarrier(ID3D12Resource* resource, D3D12_RESOURCE_STATES from, D3D12_RESOURCE_STATES to) {
    raw->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(resource,
        from, to));
}

void DX12::GraphicsCommandList::RSSetViewport(D3D12_VIEWPORT viewport) {
    raw->RSSetViewports(1, &viewport);
}

void DX12::GraphicsCommandList::RSSetScissorRect(D3D12_RECT rect) {
    raw->RSSetScissorRects(1, &rect);
}

void DX12::GraphicsCommandList::ClearRenderTargetView(D3D12_CPU_DESCRIPTOR_HANDLE RenderTargetView, const FLOAT color[4]) {
    raw->ClearRenderTargetView(RenderTargetView, color, 0, nullptr);
}

void DX12::GraphicsCommandList::ClearDepthStencilView(D3D12_CPU_DESCRIPTOR_HANDLE DepthStencilView) {
    raw->ClearDepthStencilView(DepthStencilView, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.f, 0, 0, nullptr);
}

void DX12::GraphicsCommandList::OMSetRenderTarget(
    D3D12_CPU_DESCRIPTOR_HANDLE rendertarget,
    D3D12_CPU_DESCRIPTOR_HANDLE depthstencil)
{
    raw->OMSetRenderTargets(1, &rendertarget, true, &depthstencil);
}

void DX12::GraphicsCommandList::DrawIndexed(
    UINT IndexCount,
    UINT StartIndexLocation,
    INT BaseVertexLocation)
{
    raw->DrawIndexedInstanced(IndexCount, 1, StartIndexLocation, BaseVertexLocation, 0);
}

void DX12::Blob::Create(SIZE_T size) {
    ThrowIfFailed(D3DCreateBlob(size, &raw));
}

void DX12::Blob::Copy(const void* data, SIZE_T size) {
    CopyMemory(raw->GetBufferPointer(), data, size);
}

void DX12::Blob::Create(const void* data, SIZE_T size) {
    Create(size);
    Copy(data, size);
}
