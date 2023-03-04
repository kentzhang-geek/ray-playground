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
    ComPtr<ID3DBlob> signature;
    ComPtr<ID3DBlob> error;
    ThrowIfFailed(D3D12SerializeRootSignature(&rsDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error));
    ThrowIfFailed(GetDevice()->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(),
                                                IID_PPV_ARGS(&m_rootSignature)));

    // shaders
    ComPtr<ID3DBlob> vertexShader;
    ComPtr<ID3DBlob> pixelShader;
    UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
    TCHAR buf[1024];
    GetCurrentDirectory(1024, buf);
    RTP_LOG("%s\n", buf);
    ThrowIfFailed(D3DCompileFromFile(GetAssetFullPath(L"tri_shader.hlsl").c_str(), nullptr, nullptr, "VSMain", "vs_5_0",
                                     compileFlags, 0, &vertexShader, nullptr));
    ThrowIfFailed(D3DCompileFromFile(GetAssetFullPath(L"tri_shader.hlsl").c_str(), nullptr, nullptr, "PSMain", "ps_5_0",
                                     compileFlags, 0, &pixelShader, nullptr));

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
    psoDesc.VS = CD3DX12_SHADER_BYTECODE(vertexShader.Get());
    psoDesc.PS = CD3DX12_SHADER_BYTECODE(pixelShader.Get());
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
