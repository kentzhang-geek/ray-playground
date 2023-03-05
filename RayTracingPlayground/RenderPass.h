#pragma once

#include "Helper.h"

class RenderPass
{
public:
    /**
     * \brief prepare render resrouces
     */
    virtual void Prepare() = 0;
    /**
     * \brief push commands to command list
     */
    virtual void PopulateCommandList() = 0;

    /**
     * \brief serialize a root signature
     * \param desc root signature description
     * \param rootSig root signature object
     */
    void PackRootSignature(D3D12_ROOT_SIGNATURE_DESC& desc, ComPtr<ID3D12RootSignature>* rootSig);

    typedef enum
    {
        Invalid = 0,
        VS,
        PS
    } ShaderType;

    /**
     * \brief compile a shader into bytecode
     * \param hlslFilename file name of target hlsl file
     * \param entryName entry of the shader
     * \param stype type of the shader
     * \param compileFlags flags of the shader
     */
    ComPtr<ID3DBlob> CompileShader(LPCWSTR hlslFilename, const char* entryName, ShaderType stype,
                       UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION);

    /**
     * \brief convert shader type into the string which compile call needs
     * \param stype input shader type
     * \return return target string
     */
    inline const char* TargetNameShaderType(ShaderType stype);

    /**
     * \brief fill parameters of bytecode object
     * \param shaderblob 
     * \param bytecode 
     */
    void ShaderBlob2Bytecode(ComPtr<ID3DBlob> shaderblob, D3D12_SHADER_BYTECODE * bytecode);
};
