#pragma once
#include "RayTracingConstants.h"
#include "RenderPass.h"

class RayTracingPass : public RenderPass
{
public:
    void Prepare() override;
    void PopulateCommandList() override;

    RayGenConstantBuffer m_rayGenConstants;
    ComPtr<ID3D12RootSignature> m_globalRootSig;
    ComPtr<ID3D12RootSignature> m_localRootSig;
};
