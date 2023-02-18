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
	}

	void Init();
	void Update();
	void Frame();

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
	int m_width;
	int m_height;
	HWND m_hwnd;
	
    // Synchronization objects.
    UINT m_frameIndex;
};

