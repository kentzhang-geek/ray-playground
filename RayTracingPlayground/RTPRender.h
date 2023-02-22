#pragma once

#define FRAME_COUNT_DBL_BUFFER 2

class RTPRender
{
public:
	RTPRender(int m_width, int m_height, HWND m_hwnd)
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

	void Init();
	void Update();
	void Frame();
	void WaitForPreviousFrame();
	/**
	 * \brief Populate the main command list
	 */
	void PopulateCommandList();

	/**
	 * \brief Initiate the pipeline
	 */
	void LoadPipeline();
	/**
	 * \brief Initiate shaders and other resources
	 */
	void LoadAsstes();
	Microsoft::WRL::ComPtr<ID3D12Device8> m_device;
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> m_commandQueue;
    Microsoft::WRL::ComPtr<IDXGISwapChain4> m_swapChain;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_renderTargets[FRAME_COUNT_DBL_BUFFER];
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> m_commandAllocator;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> m_rootSignature;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_pipelineState;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> m_commandList;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_vertexBuffer;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_indexBuffer;
    D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView;
	D3D12_INDEX_BUFFER_VIEW m_indexBufferView;
    HANDLE m_fenceEvent;
    Microsoft::WRL::ComPtr<ID3D12Fence> m_fence;
    UINT64 m_fenceValue;
	int m_width;
	int m_height;
	float m_aspectRatio;
	UINT32 m_rtvDescriptorSize;
    D3D12_VIEWPORT m_viewport;
	HWND m_hwnd;
	
    // Synchronization objects.
    UINT m_frameIndex;
};

