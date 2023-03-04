#ifndef __RAY_TRACING_CONSTANTS__
#define __RAY_TRACING_CONSTANTS__

struct Viewport
{
    float left;
    float top;
    float right;
    float bottom;
};

struct RayGenConstantBuffer
{
    Viewport viewport;
    Viewport stencil;
};

#endif