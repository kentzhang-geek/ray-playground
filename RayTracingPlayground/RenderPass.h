#pragma once

#include "Helper.h"

class RenderPass
{
public:
    virtual void Prepare() = 0;
    virtual void PopulateCommandList() = 0;
    void PackRootSignature(D3D12_ROOT_SIGNATURE_DESC& desc, ComPtr<ID3D12RootSignature>* rootSig);

    
};
