#include "pch.h"
#include "DeviceResources.h"

#include "Helper.h"

DeviceResources* DeviceResources::instance = nullptr;

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


DeviceResources* DeviceResources::Get()
{
    return instance;
}

bool DeviceResources::Init(int m_width, int m_height, HWND m_hwnd)
{
    if (nullptr != instance)
    {
        instance = new DeviceResources(m_width, m_height, m_hwnd);
        return true;
    }
}

DeviceResources::DeviceResources()
{
}

DeviceResources::DeviceResources(int m_width, int m_height, HWND m_hwnd)
    : m_width(m_width),
      m_height(m_height),
      m_hwnd(m_hwnd)
{
    m_aspectRatio = float(m_width) / float(m_height);
    m_viewport = {};
    m_viewport.Height = static_cast<float>(m_height);
    m_viewport.Width = static_cast<float>(m_width);
    m_viewport.TopLeftX = 0.0f;
    m_viewport.TopLeftY = 0.0f;
    m_viewport.MaxDepth = D3D12_MAX_DEPTH;
    m_viewport.MinDepth = D3D12_MIN_DEPTH;
}

void DeviceResources::Init()
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

    // create command list
    ThrowIfFailed(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, DeviceResources::Get()->m_commandAllocator.Get(),
                                              nullptr, IID_PPV_ARGS(&m_commandList)));
    // close it for preparing main loop
    ThrowIfFailed(m_commandList->Close());

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

ID3D12Device8* GetDevice()
{
    if (nullptr != DeviceResources::Get())
    {
        return DeviceResources::Get()->m_device.Get();
    }
}

void DeviceResources::WaitForPreviousFrame()
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
