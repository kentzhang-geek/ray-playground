#pragma once

#define FRAME_COUNT_DBL_BUFFER 2

class DeviceResources
{
public:
	static DeviceResources * Get();
	static bool Init(int m_width, int m_height, HWND m_hwnd);
	
	void WaitForPreviousFrame();
	
	Microsoft::WRL::ComPtr<ID3D12Device8> m_device;
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> m_commandQueue;
    Microsoft::WRL::ComPtr<IDXGISwapChain4> m_swapChain;
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> m_commandAllocator;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_renderTargets[FRAME_COUNT_DBL_BUFFER];
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList4> m_commandList;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
	UINT32 m_rtvDescriptorSize;

    HANDLE m_fenceEvent;
    Microsoft::WRL::ComPtr<ID3D12Fence> m_fence;
    UINT64 m_fenceValue;
	int m_width;
	int m_height;
	float m_aspectRatio;
    D3D12_VIEWPORT m_viewport;
    UINT m_frameIndex;
	HWND m_hwnd;
	
private:
	DeviceResources();
	DeviceResources(int m_width, int m_height, HWND m_hwnd);
	void Init();
	
	static DeviceResources * instance;
};

ID3D12Device8 * GetDevice();
#define DEVICE GetDevice()
#define RESOURCES DeviceResources::Get()