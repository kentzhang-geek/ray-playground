#include "pch.h"
#include "RayTracingPass.h"

#include "d3dx12.h"
#include "DeviceResources.h"
#include "DXRHelper.h"
#include "nv_helpers_dx12/BottomLevelASGenerator.h"
#include "nv_helpers_dx12/RaytracingPipelineGenerator.h"
#include "nv_helpers_dx12/RootSignatureGenerator.h"

void RayTracingPass::Prepare()
{
    // Global root signature
    {
        D3D12_DESCRIPTOR_RANGE uavDesc = {};
        uavDesc.NumDescriptors = 1;
        uavDesc.BaseShaderRegister = 0;
        uavDesc.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
        uavDesc.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
        D3D12_ROOT_PARAMETER rootParms[2] = {};
        // output 
        rootParms[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        rootParms[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        rootParms[0].DescriptorTable.NumDescriptorRanges = 1;
        rootParms[0].DescriptorTable.pDescriptorRanges = &uavDesc;
        // Acc Structure
        rootParms[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        rootParms[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
        rootParms[1].Descriptor.ShaderRegister = 0;
        rootParms[1].Descriptor.RegisterSpace = 0;
        D3D12_ROOT_SIGNATURE_DESC rootSigDesc = {};
        rootSigDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;
        rootSigDesc.NumStaticSamplers = 0;
        rootSigDesc.NumParameters = 2;
        rootSigDesc.pParameters = rootParms;
        PackRootSignature(rootSigDesc, &m_globalRootSig);
    }

    // local root signature
    {
        D3D12_ROOT_PARAMETER rootParam = {};
        rootParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
        rootParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        rootParam.Descriptor.RegisterSpace = 0;
        rootParam.Descriptor.ShaderRegister = 0;
        rootParam.Constants.Num32BitValues = SizeOfInUint32(m_rayGenConstants);
        D3D12_ROOT_SIGNATURE_DESC rootDesc = {};
        rootDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE;
        rootDesc.NumParameters = 1;
        rootDesc.pParameters = &rootParam;
        PackRootSignature(rootDesc, &m_localRootSig);
    }

    // create ray tracing pipeline state object
    {
        nv_helpers_dx12::RayTracingPipelineGenerator pipe(DEVICE);

        m_rayGenLib = nv_helpers_dx12::CompileShaderLibrary(GetAssetFullPath(L"shaders/RayGen.hlsl").c_str());
        m_hitLib = nv_helpers_dx12::CompileShaderLibrary(GetAssetFullPath(L"shaders/Hit.hlsl").c_str());
        m_missLib = nv_helpers_dx12::CompileShaderLibrary(GetAssetFullPath(L"shaders/Miss.hlsl").c_str());

        pipe.AddLibrary(m_rayGenLib.Get(), {L"RayGen"});
        pipe.AddLibrary(m_hitLib.Get(), {L"ClosestHit"});
        pipe.AddLibrary(m_missLib.Get(), {L"Miss"});

        // ray gen sig
        {
            nv_helpers_dx12::RootSignatureGenerator rsc;
            rsc.AddHeapRangesParameter({
                {
                    0, 1, 0, D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 0
                },
                {
                    0, 1, 0, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1
                }
            });
            m_rayGenSig = rsc.Generate(DEVICE, true);
        }
        // miss sig
        nv_helpers_dx12::RootSignatureGenerator missG;
        m_missSig = missG.Generate(DEVICE, true);
        // Hig sig
        nv_helpers_dx12::RootSignatureGenerator hitG;
        hitG.AddRootParameter(D3D12_ROOT_PARAMETER_TYPE_SRV);
        m_hitSig = hitG.Generate(DEVICE, true);
        // hit group
        pipe.AddHitGroup(L"HitGroup", L"ClosestHit");
        // parameters
        pipe.AddRootSignatureAssociation(m_rayGenSig.Get(), {L"RayGen"});
        pipe.AddRootSignatureAssociation(m_hitSig.Get(), {L"HitGroup"});
        pipe.AddRootSignatureAssociation(m_missSig.Get(), {L"Miss"});
        // configs
        pipe.SetMaxPayloadSize(4 * sizeof(float));
        pipe.SetMaxAttributeSize(2 * sizeof(float));
        pipe.SetMaxRecursionDepth(2);

        m_rtStateObject = pipe.Generate();
        ThrowIfFailed(m_rtStateObject->QueryInterface(IID_PPV_ARGS(&m_rtStateObjectProps)));
    }

    // ray tracing output resources
    {
        D3D12_RESOURCE_DESC resDesc = {};
        resDesc.DepthOrArraySize = 1;
        resDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        resDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        resDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
        resDesc.Width = RESOURCES->m_width;
        resDesc.Height = RESOURCES->m_height;
        resDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        resDesc.MipLevels = 1;
        resDesc.SampleDesc.Count = 1;
        ThrowIfFailed(DEVICE->CreateCommittedResource(&nv_helpers_dx12::kDefaultHeapProps, D3D12_HEAP_FLAG_NONE,
                                                      &resDesc, D3D12_RESOURCE_STATE_COPY_SOURCE, nullptr,
                                                      IID_PPV_ARGS(&m_outputResource)));
    }

    // shader resource heap
    {
        m_srvUavHeap = nv_helpers_dx12::CreateDescriptorHeap(DEVICE, 2, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, true);
        D3D12_CPU_DESCRIPTOR_HANDLE srvHandle = m_srvUavHeap->GetCPUDescriptorHandleForHeapStart();
        D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
        uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
        DEVICE->CreateUnorderedAccessView(m_outputResource.Get(), nullptr, &uavDesc, srvHandle);

        srvHandle.ptr += DEVICE->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

        // TLAS
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc;
        srvDesc.Format = DXGI_FORMAT_UNKNOWN;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.RaytracingAccelerationStructure.Location = m_topLevelASBuffers.pResult->GetGPUVirtualAddress();
        DEVICE->CreateShaderResourceView(nullptr, &srvDesc, srvHandle);
    }

    LoadAssets();

    CreateAS();

    // TODO : shader binding table
}

void RayTracingPass::PopulateCommandList()
{
}

void RayTracingPass::CreateAS()
{
    // TODO : create acc structures
    AccelerationStructureBuffers blBuffers = CreateBtmAS(m_vertexBuffer, 3);
}

AccelerationStructureBuffers RayTracingPass::CreateBtmAS(ComPtr<ID3D12Resource> buf, uint32_t vtxCount)
{
    nv_helpers_dx12::BottomLevelASGenerator btmAsG;
    btmAsG.AddVertexBuffer(buf.Get(), 0, vtxCount, sizeof(Vertex), nullptr, 0);
    // The AS build requires some scratch space to store temporary information.
    // The amount of scratch memory is dependent on the scene complexity.
    UINT64 scratchSizeInBytes = 0;
    // The final AS also needs to be stored in addition to the existing vertex
    // buffers. It size is also dependent on the scene complexity.
    UINT64 resultSizeInBytes = 0;
    btmAsG.ComputeASBufferSizes(DEVICE, false, &scratchSizeInBytes,
                                &resultSizeInBytes);

    AccelerationStructureBuffers asBuf;
    asBuf.pScratch = nv_helpers_dx12::CreateBuffer(DEVICE, scratchSizeInBytes,
                                                   D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
                                                   D3D12_RESOURCE_STATE_COMMON, nv_helpers_dx12::kDefaultHeapProps);
    asBuf.pResult = nv_helpers_dx12::CreateBuffer(DEVICE, resultSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
                                                  D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE,
                                                  nv_helpers_dx12::kDefaultHeapProps);
    btmAsG.Generate(RESOURCES->m_commandList.Get(), asBuf.pScratch.Get(), asBuf.pResult.Get(), false, nullptr);
    return asBuf;
}

void RayTracingPass::LoadAssets()
{
    // Create an empty root signature.
    {
        D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = {0};
        rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
        PackRootSignature(rootSignatureDesc, &m_globalRootSig);
    }

    // Create the pipeline state, which includes compiling and loading shaders.
    {
        ComPtr<ID3DBlob> vertexShader;
        ComPtr<ID3DBlob> pixelShader;

        // Define the vertex input layout.
        D3D12_INPUT_ELEMENT_DESC inputElementDescs[] = {
            {
                "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
                D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0
            },
            {
                "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12,
                D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0
            }
        };
    }


    // Create the vertex buffer.
    {
        // Define the geometry for a triangle.
        Vertex triVtx[] = {
            {{0.0f, 0.25f * RESOURCES->m_aspectRatio, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},
            {{0.25f, -0.25f * RESOURCES->m_aspectRatio, 0.0f}, {0.0f, 1.0f, 0.0f, 1.0f}},
            {{-0.25f, -0.25f * RESOURCES->m_aspectRatio, 0.0f}, {0.0f, 0.0f, 1.0f, 1.0f}}
        };

        D3D12_HEAP_PROPERTIES hpp = {};
        hpp.Type = D3D12_HEAP_TYPE_UPLOAD;
        hpp.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        hpp.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
        hpp.CreationNodeMask = 1;
        hpp.VisibleNodeMask = 1;
        D3D12_RESOURCE_DESC rDesc = {};
        rDesc.Alignment = 0;
        rDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        rDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
        rDesc.Format = DXGI_FORMAT_UNKNOWN;
        rDesc.Height = 1;
        rDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        rDesc.Width = sizeof(triVtx);
        rDesc.MipLevels = 1;
        rDesc.SampleDesc.Count = 1;
        rDesc.DepthOrArraySize = 1;
        ThrowIfFailed(DEVICE->CreateCommittedResource(
            &hpp,
            D3D12_HEAP_FLAG_NONE,
            &rDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&m_vertexBuffer)));

        // Copy the triangle data to the vertex buffer.
        UINT8* pVertexDataBegin;
        CD3DX12_RANGE readRange(0, 0); // We do not intend to read from this resource on the CPU.
        ThrowIfFailed(m_vertexBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pVertexDataBegin)));
        memcpy(pVertexDataBegin, triVtx, sizeof(triVtx));
        m_vertexBuffer->Unmap(0, nullptr);

        // Initialize the vertex buffer view.
        m_vertexBufferView.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();
        m_vertexBufferView.StrideInBytes = sizeof(Vertex);
        m_vertexBufferView.SizeInBytes = sizeof(triVtx);
    }
}
