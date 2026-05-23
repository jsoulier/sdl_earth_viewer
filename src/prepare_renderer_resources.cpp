#include <Cesium3DTilesSelection/Tile.h>
#include <Cesium3DTilesSelection/TileLoadResult.h>
#include <CesiumAsync/AsyncSystem.h>
#include <CesiumAsync/Future.h>
#include <CesiumGltf/AccessorView.h>
#include <CesiumGltf/ImageAsset.h>
#include <CesiumGltf/Model.h>
#include <CesiumRasterOverlays/RasterOverlayTile.h>
#include <SDL3/SDL.h>
#include <glm/glm.hpp>

#include <cstdint>
#include <any>
#include <vector>

#include "prepare_renderer_resources.hpp"

SDLPrepareRendererResources::SDLPrepareRendererResources(SDL_GPUDevice* device)
    : Device{device}
{
}

SDLPrepareRendererResources::~SDLPrepareRendererResources()
{
    Device = nullptr;
}

CesiumAsync::Future<Cesium3DTilesSelection::TileLoadResultAndRenderResources> SDLPrepareRendererResources::prepareInLoadThread(
    const CesiumAsync::AsyncSystem& asyncSystem,
    Cesium3DTilesSelection::TileLoadResult&& tileLoadResult,
    const glm::dmat4& transform,
    const std::any& rendererOptions)
{
    CesiumGltf::Model* rootModel = std::get_if<CesiumGltf::Model>(&tileLoadResult.contentKind);
    if (!rootModel)
    {
        return asyncSystem.createResolvedFuture(Cesium3DTilesSelection::TileLoadResultAndRenderResources{std::move(tileLoadResult), nullptr});
    }
    SDL_GPUCommandBuffer* commandBuffer = SDL_AcquireGPUCommandBuffer(Device);
    if (!commandBuffer)
    {
        SDL_Log("Failed to acquire command buffer: %s", SDL_GetError());
        return asyncSystem.createResolvedFuture(Cesium3DTilesSelection::TileLoadResultAndRenderResources{std::move(tileLoadResult), nullptr});
    }
    SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(commandBuffer);
    if (!copyPass)
    {
        SDL_Log("Failed to begin copy pass: %s", SDL_GetError());
        SDL_CancelGPUCommandBuffer(commandBuffer);
        return asyncSystem.createResolvedFuture(Cesium3DTilesSelection::TileLoadResultAndRenderResources{std::move(tileLoadResult), nullptr});
    }
    SDLPrepareRendererResourcesTile* resources = new SDLPrepareRendererResourcesTile();
    rootModel->forEachPrimitiveInScene(-1, [&](
        const CesiumGltf::Model& model,
        const CesiumGltf::Node& node,
        const CesiumGltf::Mesh& mesh,
        const CesiumGltf::MeshPrimitive& primitive,
        const glm::dmat4& transform)
        {
            auto positionIt = primitive.attributes.find("POSITION");
            if (positionIt == primitive.attributes.end())
            {
                return;
            }
            CesiumGltf::AccessorView<glm::vec3> positionView(model, positionIt->second);
            if (positionView.size() <= 0)
            {
                return;
            }
            SDL_GPUTransferBuffer* vertexTransferBuffer = nullptr;
            SDL_GPUBuffer* vertexBuffer = nullptr;
            uint32_t numVertices = positionView.size();
            SDLPrepareRendererResourcesTileMeshVertex* vertexData = nullptr;
            {
                SDL_GPUTransferBufferCreateInfo info{};
                info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
                info.size = numVertices * sizeof(SDLPrepareRendererResourcesTileMeshVertex);
                vertexTransferBuffer = SDL_CreateGPUTransferBuffer(Device, &info);
            }
            {
                SDL_GPUBufferCreateInfo info{};
                info.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
                info.size = numVertices * sizeof(SDLPrepareRendererResourcesTileMeshVertex);
                vertexBuffer = SDL_CreateGPUBuffer(Device, &info);
            }
            if (!vertexTransferBuffer || !vertexBuffer)
            {
                SDL_Log("Failed to create vertex buffer(s): %s", SDL_GetError());
                SDL_ReleaseGPUTransferBuffer(Device, vertexTransferBuffer);
                SDL_ReleaseGPUBuffer(Device, vertexBuffer);
                return;
            }
            vertexData = static_cast<SDLPrepareRendererResourcesTileMeshVertex*>(SDL_MapGPUTransferBuffer(Device, vertexTransferBuffer, false));
            if (!vertexData)
            {
                SDL_Log("Failed to map vertex transfer buffer: %s", SDL_GetError());
                SDL_ReleaseGPUTransferBuffer(Device, vertexTransferBuffer);
                SDL_ReleaseGPUBuffer(Device, vertexBuffer);
                return;
            }
            for (uint32_t i = 0; i < numVertices; i++)
            {
                vertexData[i].Position = positionView[i];
            }
            SDL_UnmapGPUTransferBuffer(Device, vertexTransferBuffer);
            {
                SDL_GPUTransferBufferLocation location{};
                location.transfer_buffer = vertexTransferBuffer;
                SDL_GPUBufferRegion region{};
                region.buffer = vertexBuffer;
                region.size = numVertices * sizeof(SDLPrepareRendererResourcesTileMeshVertex);
                SDL_UploadToGPUBuffer(copyPass, &location, &region, false);
                SDL_ReleaseGPUTransferBuffer(Device, vertexTransferBuffer);
            }
            SDL_GPUTransferBuffer* indexTransferBuffer = nullptr;
            SDL_GPUBuffer* indexBuffer = nullptr;
            uint32_t numIndices = 0;
            uint32_t* indexData = nullptr;
            SDL_GPUIndexElementSize indexElementSize = SDL_GPU_INDEXELEMENTSIZE_16BIT;
            CesiumGltf::AccessorView<uint32_t> indexView(model, primitive.indices);
            if (indexView.size() > 0)
            {
                numIndices = static_cast<uint32_t>(indexView.size());
                indexElementSize = SDL_GPU_INDEXELEMENTSIZE_32BIT;
                {
                    SDL_GPUTransferBufferCreateInfo info{};
                    info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
                    info.size = numIndices * sizeof(uint32_t);
                    indexTransferBuffer = SDL_CreateGPUTransferBuffer(Device, &info);
                }
                {
                    SDL_GPUBufferCreateInfo info{};
                    info.usage = SDL_GPU_BUFFERUSAGE_INDEX;
                    info.size = numIndices * sizeof(uint32_t);
                    indexBuffer = SDL_CreateGPUBuffer(Device, &info);
                }
                if (indexTransferBuffer && indexBuffer)
                {
                    uint32_t* indexData = static_cast<uint32_t*>(SDL_MapGPUTransferBuffer(Device, indexTransferBuffer, false));
                    if (indexData)
                    {
                        for (uint32_t i = 0; i < numIndices; i++)
                        {
                            indexData[i] = indexView[i];
                        }
                        SDL_UnmapGPUTransferBuffer(Device, indexTransferBuffer);
                        {
                            SDL_GPUTransferBufferLocation location{};
                            location.transfer_buffer = indexTransferBuffer;
                            SDL_GPUBufferRegion region{};
                            region.buffer = indexBuffer;
                            region.size = numIndices * sizeof(uint32_t);
                            SDL_UploadToGPUBuffer(copyPass, &location, &region, false);
                            SDL_ReleaseGPUTransferBuffer(Device, indexTransferBuffer);
                        }
                    }
                    else
                    {
                        SDL_Log("Failed to map index transfer buffer: %s", SDL_GetError());
                    }
                }
                else
                {
                    SDL_Log("Failed to create index buffer(s): %s", SDL_GetError());
                }
                if (!indexTransferBuffer || !indexBuffer || !indexData)
                {
                    SDL_ReleaseGPUTransferBuffer(Device, vertexTransferBuffer);
                    SDL_ReleaseGPUBuffer(Device, vertexBuffer);
                    SDL_ReleaseGPUTransferBuffer(Device, indexTransferBuffer);
                    SDL_ReleaseGPUBuffer(Device, indexBuffer);
                    return;
                }
            }
            resources->Primitives.push_back({
                vertexBuffer,
                indexBuffer,
                numVertices,
                numIndices,
                indexElementSize
            });
        });
    SDL_EndGPUCopyPass(copyPass);
    SDL_SubmitGPUCommandBuffer(commandBuffer);
    return asyncSystem.createResolvedFuture(Cesium3DTilesSelection::TileLoadResultAndRenderResources{std::move(tileLoadResult), resources});
}

void* SDLPrepareRendererResources::prepareInMainThread(
    Cesium3DTilesSelection::Tile& tile,
    void* pLoadThreadResult)
{
    // Noop
    return pLoadThreadResult;
}

void SDLPrepareRendererResources::free(
    Cesium3DTilesSelection::Tile& tile,
    void* pLoadThreadResult,
    void* pMainThreadResult) noexcept
{
    void* pResult = pMainThreadResult ? pMainThreadResult : pLoadThreadResult;
    if (pResult)
    {
        SDLPrepareRendererResourcesTile* pResources = static_cast<SDLPrepareRendererResourcesTile*>(pResult);
        for (auto& primitive : pResources->Primitives)
        {
            if (primitive.VertexBuffer)
            {
                SDL_ReleaseGPUBuffer(Device, primitive.VertexBuffer);
            }
            if (primitive.IndexBuffer)
            {
                SDL_ReleaseGPUBuffer(Device, primitive.IndexBuffer);
            }
        }
        delete pResources;
    }
}

void SDLPrepareRendererResources::attachRasterInMainThread(
    const Cesium3DTilesSelection::Tile& tile,
    int32_t overlayTextureCoordinateID,
    const CesiumRasterOverlays::RasterOverlayTile& rasterTile,
    void* pMainThreadRendererResources,
    const glm::dvec2& translation,
    const glm::dvec2& scale)
{
}

void SDLPrepareRendererResources::detachRasterInMainThread(
    const Cesium3DTilesSelection::Tile& tile,
    int32_t overlayTextureCoordinateID,
    const CesiumRasterOverlays::RasterOverlayTile& rasterTile,
    void* pMainThreadRendererResources) noexcept
{
}

void* SDLPrepareRendererResources::prepareRasterInLoadThread(
    CesiumGltf::ImageAsset& image,
    const std::any& rendererOptions)
{
    return new SDLPrepareRendererResourcesRasterOverlayTile();
}

void* SDLPrepareRendererResources::prepareRasterInMainThread(
    CesiumRasterOverlays::RasterOverlayTile& rasterTile,
    void* pLoadThreadResult)
{
    // Noop
    return pLoadThreadResult;
}

void SDLPrepareRendererResources::freeRaster(
    const CesiumRasterOverlays::RasterOverlayTile& rasterTile,
    void* pLoadThreadResult,
    void* pMainThreadResult) noexcept
{
    void* pResult = pMainThreadResult ? pMainThreadResult : pLoadThreadResult;
    if (pResult)
    {
        delete static_cast<SDLPrepareRendererResourcesRasterOverlayTile*>(pResult);
    }
}
