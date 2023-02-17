#include "pch.h"
#include "RTPRender.h"

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
    chainDesc.BufferCount = FRAME_COUNT_DBL_BUFFER;  // double buffer
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
            rtvHandle.ptr += m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
        }
    }
    ThrowIfFailed(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandAllocator)));
}

void RTPRender::LoadAsstes()
{
}

void RTPRender::Init()
{
    LoadPipeline();
}

void RTPRender::Update()
{
}

void RTPRender::Frame()
{
}
