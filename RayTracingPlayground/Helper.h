#pragma once
#include <stdexcept>
#include <string>
#include "Log.h"

using Microsoft::WRL::ComPtr;

/**
 * \brief check the result of a D3D12 call, if failed then crash instantly
 * \param hr result of a typical D3D12 call
 */
inline void ThrowIfFailed(HRESULT hr)
{
    if (FAILED(hr))
    {
        RTP_LOG("HRESULT of 0x%08X", static_cast<UINT>(hr));
        throw std::runtime_error("error");
    }
}

/**
 * \brief print error information described by a D3DBlob
 * \param err error information described by a D3DBlob
 */
inline void CheckError(ComPtr<ID3DBlob> err)
{
    if (err)
    {
        RTP_LOG("Get Error : %s", static_cast<char *>(err->GetBufferPointer()));
    }
}

/**
 * \brief depends on which file path will the debugger running at
 * \param assetName name of a asset need to load
 * \return relative file path
 */
inline std::wstring GetAssetFullPath(LPCWSTR assetName)
{
    return std::wstring(L"./") + assetName;
}

#define SizeOfInUint32(obj) ((sizeof(obj) - 1) / sizeof(UINT32) + 1)
