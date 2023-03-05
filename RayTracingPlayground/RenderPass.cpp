#include "pch.h"
#include "RenderPass.h"

#include "DeviceResources.h"

void RenderPass::PackRootSignature(D3D12_ROOT_SIGNATURE_DESC& desc, ComPtr<ID3D12RootSignature>* rootSig)
{
    ComPtr<ID3DBlob> blob;
    ComPtr<ID3DBlob> error;

    ThrowIfFailed(D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &error));
    CheckError(error);
    ThrowIfFailed(DEVICE->CreateRootSignature(1, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&(*rootSig))));
}

ComPtr<ID3DBlob> RenderPass::CompileShader(LPCWSTR hlslFilename, const char* entryName, ShaderType stype, UINT compileFlags)
{
    ComPtr<ID3DBlob> shaderblob;
    ComPtr<ID3DBlob> error;
    D3DCompileFromFile(GetAssetFullPath(hlslFilename).c_str(), nullptr, nullptr, entryName, TargetNameShaderType(stype),
                                     compileFlags, 0, &shaderblob, &error);
    CheckError(error);
    return shaderblob;
}

const char* RenderPass::TargetNameShaderType(ShaderType stype)
{
    switch (stype)
    {
    case PS : return "ps_5_0";
    case VS : return "vs_5_0";
    }
    return "";
}

void RenderPass::ShaderBlob2Bytecode(ComPtr<ID3DBlob> shaderblob, D3D12_SHADER_BYTECODE* bytecode)
{
    bytecode->BytecodeLength = shaderblob->GetBufferSize();
    bytecode->pShaderBytecode = shaderblob->GetBufferPointer();
}
