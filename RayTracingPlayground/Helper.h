#include "Log.h"

using Microsoft::WRL::ComPtr;

inline void ThrowIfFailed(HRESULT hr)
{
    if (FAILED(hr))
    {
        RTP_LOG("HRESULT of 0x%08X", static_cast<UINT>(hr));
        exit(0);
    }
}
