#include "pch.h"
#include "RTPRender.h"

#include "d3dx12.h"
#include "DeviceResources.h"
#include "Helper.h"

RTPRender::RTPRender()
{
}

void RTPRender::Init()
{
}

void RTPRender::Update()
{
}

void RTPRender::Frame()
{
    PopulateCommandList();
    ID3D12CommandList* lists[] = {
        m_commandList.Get()
    };
    RESOURCES->m_commandQueue->ExecuteCommandLists(1, lists);
    ThrowIfFailed(RESOURCES->m_swapChain->Present(1, 0));
    RESOURCES->WaitForPreviousFrame();
}

void RTPRender::PopulateCommandList()
{
    ThrowIfFailed(m_commandAllocator->Reset());
    ThrowIfFailed(m_commandList->Reset(m_commandAllocator.Get(), nullptr));
    m_commandList->SetPipelineState(m_pipelineState.Get());
    // necessary states
    m_commandList->SetGraphicsRootSignature(m_rootSignature.Get());
    m_commandList->RSSetViewports(1, &m_viewport);
    D3D12_RECT sciRect = {};
    sciRect.top = 0;
    sciRect.left = 0;
    sciRect.right = m_width;
    sciRect.bottom = m_height;
    m_commandList->RSSetScissorRects(1, &sciRect);

    // Indicate that the back buffer will be used as a render target.
    D3D12_RESOURCE_BARRIER dbr;
    dbr.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    dbr.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    dbr.Transition.pResource = m_renderTargets[m_frameIndex].Get();
    dbr.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    dbr.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    dbr.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    m_commandList->ResourceBarrier(1, &dbr);

    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle;
    rtvHandle.ptr = m_rtvHeap->GetCPUDescriptorHandleForHeapStart().ptr + m_frameIndex * m_rtvDescriptorSize;
    m_commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

    // record commands
    const float clearColor[] = {0.0f, 0.2f, 0.4f, 1.0f};
    m_commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
    m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_commandList->IASetVertexBuffers(0, 1, &m_vertexBufferView);
    m_commandList->IASetIndexBuffer(&m_indexBufferView);
    m_commandList->DrawIndexedInstanced(3, 1, 0, 0, 0);

    // Indicate that the back buffer will now be used to present.
    dbr.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    dbr.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    dbr.Transition.pResource = m_renderTargets[m_frameIndex].Get();
    dbr.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    dbr.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    dbr.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
    m_commandList->ResourceBarrier(1, &dbr);

    ThrowIfFailed(m_commandList->Close());
}
