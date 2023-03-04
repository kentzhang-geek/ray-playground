#pragma once


class RTPRender
{
public:
	RTPRender();

	void Init();
	void Update();
	void Frame();
	
	/**
	 * \brief Populate the main command list
	 */
	void PopulateCommandList();
	
	int m_width;
	int m_height;
	float m_aspectRatio;
    D3D12_VIEWPORT m_viewport;
	
    // Synchronization objects.
    UINT m_frameIndex;
};

