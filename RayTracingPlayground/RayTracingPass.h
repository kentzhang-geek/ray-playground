#pragma once
#include <dxcapi.h>

#include "RayTracingConstants.h"
#include "RenderPass.h"
#include "nv_helpers_dx12/TopLevelASGenerator.h"

struct AccelerationStructureBuffers
{
    ComPtr<ID3D12Resource> pScratch; // Scratch memory for AS builder
    ComPtr<ID3D12Resource> pResult; // Where the AS is
    ComPtr<ID3D12Resource> pInstanceDesc; // Hold the matrices of the instances
};

class RayTracingPass : public RenderPass
{
public:
    void Prepare() override;
    void PopulateCommandList() override;
	
	struct Vertex
	{
		DirectX::XMFLOAT3 position;
		DirectX::XMFLOAT4 color;
	};

    RayGenConstantBuffer m_rayGenConstants;
    ComPtr<ID3D12RootSignature> m_globalRootSig;
    ComPtr<ID3D12RootSignature> m_localRootSig;
    ComPtr<IDxcBlob> m_rayGenLib;
    ComPtr<IDxcBlob> m_hitLib;
    ComPtr<IDxcBlob> m_missLib;
    ComPtr<ID3D12RootSignature> m_rayGenSig;
    ComPtr<ID3D12RootSignature> m_hitSig;
    ComPtr<ID3D12RootSignature> m_missSig;
    ComPtr<ID3D12StateObject> m_rtStateObject;
    ComPtr<ID3D12StateObjectProperties> m_rtStateObjectProps;
    ComPtr<ID3D12Resource> m_outputResource;
    ComPtr<ID3D12DescriptorHeap> m_srvUavHeap;

    nv_helpers_dx12::TopLevelASGenerator m_topLevelASGenerator;
    AccelerationStructureBuffers m_topLevelASBuffers;
	
	// App resources.
	ComPtr<ID3D12Resource> m_vertexBuffer;
	D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView;

    void CreateAS();
    AccelerationStructureBuffers CreateBtmAS(ComPtr<ID3D12Resource> buf, uint32_t vtxCount);
	
	void LoadAssets();
};
