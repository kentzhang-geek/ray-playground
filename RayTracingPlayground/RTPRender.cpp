#include "pch.h"
#include "RTPRender.h"

#include "d3dx12.h"
#include "Helper.h"


static void GetHardwareAdapter(
    IDXGIFactory7* pFactory,
    IDXGIAdapter4** ppAdapter)
{
    *ppAdapter = nullptr;
    ComPtr<IDXGIAdapter4> adapter;

    ComPtr<IDXGIFactory7> factory;
    ThrowIfFailed(pFactory->QueryInterface(IID_PPV_ARGS(&factory)));
    for (UINT idx = 0; SUCCEEDED(factory->EnumAdapterByGpuPreference(idx, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
             IID_PPV_ARGS(&adapter))); idx++)
    {
        DXGI_ADAPTER_DESC3 desc;
        adapter->GetDesc3(&desc);
        if (desc.Flags & DXGI_ADAPTER_FLAG3_SOFTWARE) continue;
        if (desc.Flags & DXGI_ADAPTER_FLAG3_REMOTE) continue;
        if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_12_1, __uuidof(ID3D12Device), nullptr)))
        {
            break;
        }
    }
    if (adapter.Get() != nullptr)
    {
        *ppAdapter = adapter.Detach();
    }
}

void RTPRender::LoadPipeline()
{
    using namespace Microsoft::WRL;
    UINT dxgiFactoryFlags = 0;
    // debug layer
    Microsoft::WRL::ComPtr<ID3D12Debug> debugController;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
    {
        debugController->EnableDebugLayer();
        dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
    }
    // create device
    ComPtr<IDXGIFactory7> factory;
    ThrowIfFailed(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&factory)));
    ComPtr<IDXGIAdapter4> hardwareAdapter;
    GetHardwareAdapter(factory.Get(), &hardwareAdapter);
    ThrowIfFailed(D3D12CreateDevice(hardwareAdapter.Get(), D3D_FEATURE_LEVEL_12_1, IID_PPV_ARGS(&m_device)));
    // create command queue
    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    ThrowIfFailed(m_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_commandQueue)));
    // swap chain
    DXGI_SWAP_CHAIN_DESC1 chainDesc = {};
    chainDesc.BufferCount = FRAME_COUNT_DBL_BUFFER; // double buffer
    chainDesc.Width = m_width;
    chainDesc.Height = m_height;
    chainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    chainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    chainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    chainDesc.SampleDesc.Count = 1;
    ComPtr<IDXGISwapChain1> swapChain;
    ThrowIfFailed(factory->CreateSwapChainForHwnd(m_commandQueue.Get(),
                                                  m_hwnd, &chainDesc, nullptr, nullptr, &swapChain));
    ThrowIfFailed(factory->MakeWindowAssociation(m_hwnd, DXGI_MWA_NO_ALT_ENTER | DXGI_MWA_NO_WINDOW_CHANGES));
    ThrowIfFailed(swapChain.As(&m_swapChain));
    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();
    RTP_LOG("cidx %d\n", m_frameIndex);
    // create descriptor heap
    {
        // Describe and create a render target view (RTV) descriptor heap.
        D3D12_DESCRIPTOR_HEAP_DESC hDesc = {};
        hDesc.NumDescriptors = FRAME_COUNT_DBL_BUFFER;
        hDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        hDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        ThrowIfFailed(m_device->CreateDescriptorHeap(&hDesc, IID_PPV_ARGS(&m_rtvHeap)));
    }
    // frame resource
    {
        auto rtvHandle = m_rtvHeap->GetCPUDescriptorHandleForHeapStart();
        for (int n = 0; n < FRAME_COUNT_DBL_BUFFER; n++)
        {
            ThrowIfFailed(m_swapChain->GetBuffer(n, IID_PPV_ARGS(&m_renderTargets[n])));
            m_device->CreateRenderTargetView(m_renderTargets[n].Get(), nullptr, rtvHandle);
            m_rtvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
            rtvHandle.ptr += m_rtvDescriptorSize;
        }
    }
    ThrowIfFailed(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandAllocator)));
}

void RTPRender::LoadAsstes()
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
    ThrowIfFailed(m_device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(),
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
    ThrowIfFailed(m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pipelineState)));

    // create command list
    ThrowIfFailed(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocator.Get(),
                                              m_pipelineState.Get(), IID_PPV_ARGS(&m_commandList)));
    // close it for preparing main loop
    ThrowIfFailed(m_commandList->Close());

    // vertex buffer
    typedef struct
    {
        DirectX::XMFLOAT3 position;
        DirectX::XMFLOAT4 color;
    } Vertex;

    {
        Vertex triVtx[] = {
            {{0.0f, 0.25f * m_aspectRatio, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},
            {{0.25f, -0.25f * m_aspectRatio, 0.0f}, {0.0f, 1.0f, 0.0f, 1.0f}},
            {{-0.25f, -0.25f * m_aspectRatio, 0.0f}, {0.0f, 0.0f, 1.0f, 1.0f}}
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
        ThrowIfFailed(m_device->CreateCommittedResource(
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
        ThrowIfFailed(m_device->CreateCommittedResource(
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

    // Create synchronization objects and wait until assets have been uploaded to the GPU.
    {
        ThrowIfFailed(m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));
        m_fenceValue = 1;

        // Create an event handle to use for frame synchronization.
        m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        if (m_fenceEvent == nullptr)
        {
            ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
        }

        // Wait for the command list to execute; we are reusing the same command 
        // list in our main loop but for now, we just want to wait for setup to 
        // complete before continuing.
        WaitForPreviousFrame();
    }
}

void RTPRender::Init()
{
    LoadPipeline();
    LoadAsstes();
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
    m_commandQueue->ExecuteCommandLists(1, lists);
    ThrowIfFailed(m_swapChain->Present(1, 0));
    WaitForPreviousFrame();
}

void RTPRender::WaitForPreviousFrame()
{
    // WAITING FOR THE FRAME TO COMPLETE BEFORE CONTINUING IS NOT BEST PRACTICE.
    // This is code implemented as such for simplicity. The D3D12HelloFrameBuffering
    // sample illustrates how to use fences for efficient resource usage and to
    // maximize GPU utilization.

    // Signal and increment the fence value.
    const UINT64 fence = m_fenceValue;
    ThrowIfFailed(m_commandQueue->Signal(m_fence.Get(), fence));
    m_fenceValue++;

    // Wait until the previous frame is finished.
    if (m_fence->GetCompletedValue() < fence)
    {
        ThrowIfFailed(m_fence->SetEventOnCompletion(fence, m_fenceEvent));
        WaitForSingleObject(m_fenceEvent, INFINITE);
    }

    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();
}

void RTPRender::PopulateCommandList()
{
    ThrowIfFailed(m_commandAllocator->Reset());
    ThrowIfFailed(m_commandList->Reset(m_commandAllocator.Get(), m_pipelineState.Get()));
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
