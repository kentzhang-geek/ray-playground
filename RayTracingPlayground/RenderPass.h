#pragma once

class RenderPass
{
public:
    virtual void Prepare() = 0;
    virtual void PopulateCommandList() = 0;
};
