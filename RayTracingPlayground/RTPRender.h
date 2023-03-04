#pragma once
#include <memory>
#include <vector>

#include "RenderPass.h"


class RTPRender
{
public:
	RTPRender();

	void Init();
	void Update();
	void Frame();
	
	int m_width;
	int m_height;
	float m_aspectRatio;

	std::vector<std::shared_ptr<RenderPass>> m_passes;
};

