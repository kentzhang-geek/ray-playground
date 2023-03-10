#include "pch.h"
#include "TrianglePass.h"

#include "d3dx12.h"
#include "DeviceResources.h"
#include "Helper.h"

void TrianglePass::Prepare()
{
    // Empty root signature
    D3D12_ROOT_SIGNATURE_DESC rsDesc;
    rsDesc.NumParameters = 0;
    rsDesc.pParameters = nullptr;
    rsDesc.NumStaticSamplers = 0;
    rsDesc.pStaticSamplers = 0;
    rsDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
    PackRootSignature(rsDesc, &m_rootSignature);

    // Define the vertex input layout.
    D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
    {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}
    };

    // PSO
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.InputLayout = {inputElementDescs, _countof(inputElementDescs)};
    psoDesc.pRootSignature = m_rootSignature.Get();
    auto vsblob = CompileShader(L"tri_shader.hlsl", "VSMain", VS);
    auto psblob = CompileShader(L"tri_shader.hlsl", "PSMain", PS);
    ShaderBlob2Bytecode(vsblob, &psoDesc.VS);
    ShaderBlob2Bytecode(psblob, &psoDesc.PS);
    psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    psoDesc.DepthStencilState.DepthEnable = false; // disable depth testing for now
    psoDesc.DepthStencilState.StencilEnable = false;
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    psoDesc.SampleDesc.Count = 1;
    ThrowIfFailed(DEVICE->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pipelineState)));

    // vertex buffer
    typedef struct
    {
        DirectX::XMFLOAT3 position;
        DirectX::XMFLOAT4 color;
    } Vertex;

    {
        Vertex triVtx[] = {
            {{0.0f, 0.25f * RESOURCES->m_aspectRatio, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},
            {{0.25f, -0.25f * RESOURCES->m_aspectRatio, 0.0f}, {0.0f, 1.0f, 0.0f, 1.0f}},
            {{-0.25f, -0.25f * RESOURCES->m_aspectRatio, 0.0f}, {0.0f, 0.0f, 1.0f, 1.0f}}
        };
        D3D12_HEAP_PROPERTIES hpp = {};
        hpp.Type = D3D12_HEAP_TYPE_UPLOAD;
        hpp.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        hpp.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
        hpp.CreationNodeMask = 1;
        hpp.VisibleNodeMask = 1;
        D3D12_RESOURCE_DESC rDesc = {};
        rDesc.Alignment = 0;
        rDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        rDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
        rDesc.Format = DXGI_FORMAT_UNKNOWN;
        rDesc.Height = 1;
        rDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        rDesc.Width = sizeof(triVtx);
        rDesc.MipLevels = 1;
        rDesc.SampleDesc.Count = 1;
        rDesc.DepthOrArraySize = 1;
        ThrowIfFailed(DEVICE->CreateCommittedResource(
            &hpp,
            D3D12_HEAP_FLAG_NONE,
            &rDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&m_vertexBuffer)));

        // Copy the triangle data to the vertex buffer.
        UINT8* pVertexDataBegin;
        CD3DX12_RANGE readRange(0, 0); // We do not intend to read from this resource on the CPU.
        ThrowIfFailed(m_vertexBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pVertexDataBegin)));
        memcpy(pVertexDataBegin, triVtx, sizeof(triVtx));
        m_vertexBuffer->Unmap(0, nullptr);

        // Initialize the vertex buffer view.
        m_vertexBufferView.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();
        m_vertexBufferView.StrideInBytes = sizeof(Vertex);
        m_vertexBufferView.SizeInBytes = sizeof(triVtx);
    }

    // index buffer
    {
        UINT32 idx[3] = {0, 1, 2};
        D3D12_HEAP_PROPERTIES hpp = {};
        hpp.Type = D3D12_HEAP_TYPE_UPLOAD;
        hpp.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        hpp.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
        hpp.CreationNodeMask = 1;
        hpp.VisibleNodeMask = 1;
        D3D12_RESOURCE_DESC rDesc = {};
        rDesc.Alignment = 0;
        rDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        rDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
        rDesc.Format = DXGI_FORMAT_UNKNOWN;
        rDesc.Height = 1;
        rDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        rDesc.Width = sizeof(idx);
        rDesc.MipLevels = 1;
        rDesc.SampleDesc.Count = 1;
        rDesc.DepthOrArraySize = 1;
        ThrowIfFailed(DEVICE->CreateCommittedResource(
            &hpp,
            D3D12_HEAP_FLAG_NONE,
            &rDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&m_indexBuffer)));
        
        // Map the index buffer.
        UINT8* pIndexBufPtr;
        CD3DX12_RANGE readRange(0, 0); // We do not intend to read from this resource on the CPU.
        ThrowIfFailed(m_indexBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pIndexBufPtr)));
        memcpy(pIndexBufPtr, idx, sizeof(idx));
        m_indexBuffer->Unmap(0, nullptr);

        // Initialize the vertex buffer view.
        m_indexBufferView.BufferLocation = m_indexBuffer->GetGPUVirtualAddress();
        m_indexBufferView.Format = DXGI_FORMAT_R32_UINT;
        m_indexBufferView.SizeInBytes = sizeof(idx);
    }
}

void TrianglePass::PopulateCommandList()
{
    ThrowIfFailed(RESOURCES->m_commandAllocator->Reset());
    ThrowIfFailed(RESOURCES->m_commandList->Reset(RESOURCES->m_commandAllocator.Get(), nullptr));
    RESOURCES->m_commandList->SetPipelineState(m_pipelineState.Get());
    // necessary states
    RESOURCES->m_commandList->SetGraphicsRootSignature(m_rootSignature.Get());
    RESOURCES->m_commandList->RSSetViewports(1, &RESOURCES->m_viewport);
    D3D12_RECT sciRect = {};
    sciRect.top = 0;
    sciRect.left = 0;
    sciRect.right = RESOURCES->m_width;
    sciRect.bottom = RESOURCES->m_height;
    RESOURCES->m_commandList->RSSetScissorRects(1, &sciRect);

    // Indicate that the back buffer will be used as a render target.
    D3D12_RESOURCE_BARRIER dbr;
    dbr.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    dbr.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    dbr.Transition.pResource = RESOURCES->m_renderTargets[RESOURCES->m_frameIndex].Get();
    dbr.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    dbr.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    dbr.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    RESOURCES->m_commandList->ResourceBarrier(1, &dbr);

    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle;
    rtvHandle.ptr = RESOURCES->m_rtvHeap->GetCPUDescriptorHandleForHeapStart().ptr + RESOURCES->m_frameIndex * RESOURCES->m_rtvDescriptorSize;
    RESOURCES->m_commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

    // record commands
    const float clearColor[] = {0.0f, 0.2f, 0.4f, 1.0f};
    RESOURCES->m_commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
    RESOURCES->m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    RESOURCES->m_commandList->IASetVertexBuffers(0, 1, &m_vertexBufferView);
    RESOURCES->m_commandList->IASetIndexBuffer(&m_indexBufferView);
    RESOURCES->m_commandList->DrawIndexedInstanced(3, 1, 0, 0, 0);

    // Indicate that the back buffer will now be used to present.
    dbr.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    dbr.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    dbr.Transition.pResource = RESOURCES->m_renderTargets[RESOURCES->m_frameIndex].Get();
    dbr.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    dbr.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    dbr.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
    RESOURCES->m_commandList->ResourceBarrier(1, &dbr);

    ThrowIfFailed(RESOURCES->m_commandList->Close());
}
