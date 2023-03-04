#include "pch.h"
#include "RTPRender.h"

#include "d3dx12.h"
#include "DeviceResources.h"
#include "Helper.h"
#include "TrianglePass.h"

RTPRender::RTPRender()
{
    m_passes.push_back(std::make_shared<TrianglePass>());
}

void RTPRender::Init()
{
    for (auto pass : m_passes)
    {
        pass->Prepare();
    }
}

void RTPRender::Update()
{
}

void RTPRender::Frame()
{
    for (auto pass : m_passes)
    {
        pass->PopulateCommandList();
    }
    ID3D12CommandList* lists[] = {
        RESOURCES->m_commandList.Get()
    };
    RESOURCES->m_commandQueue->ExecuteCommandLists(1, lists);
    ThrowIfFailed(RESOURCES->m_swapChain->Present(1, 0));
    RESOURCES->WaitForPreviousFrame();
}

