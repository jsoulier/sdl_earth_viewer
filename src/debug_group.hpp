#pragma once

#include <SDL3/SDL.h>

// Useful for D3D12 where debug groups are broken
#define USE_DEBUG_GROUPS 0

#if USE_DEBUG_GROUPS
#define DebugGroup(commandBuffer) DebugGroupClass debugGroup(commandBuffer, SDL_FUNCTION)
#define DebugGroupBlock(commandBuffer, name) DebugGroupClass debugGroup(commandBuffer, name)
#else
#define DebugGroup(commandBuffer) 
#define DebugGroupBlock(commandBuffer, name) 
#endif

class DebugGroupClass
{
public:
    DebugGroupClass(SDL_GPUCommandBuffer* commandBuffer, const char* name);
    ~DebugGroupClass();

private:
    SDL_GPUCommandBuffer* CommandBuffer;
};
