#pragma once
#include <stdexcept>
#include <string>
#include "Log.h"

using Microsoft::WRL::ComPtr;

inline void ThrowIfFailed(HRESULT hr)
{
    if (FAILED(hr))
    {
        RTP_LOG("HRESULT of 0x%08X", static_cast<UINT>(hr));
        throw std::runtime_error("error");
    }
}

inline void CheckError(ComPtr<ID3DBlob> err)
{
    if (err)
    {
        RTP_LOG("Get Error : %s", static_cast<char *>(err->GetBufferPointer()));
    }
}

inline std::wstring GetAssetFullPath(LPCWSTR assetName)
{
    return std::wstring(L"./") + assetName;
}

#define SizeOfInUint32(obj) ((sizeof(obj) - 1) / sizeof(UINT32) + 1)
