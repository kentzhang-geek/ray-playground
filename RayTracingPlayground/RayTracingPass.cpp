#include "pch.h"
#include "RayTracingPass.h"

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

    // TODO: create ray tracing pipeline state object
}

void RayTracingPass::PopulateCommandList()
{
}
