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

#include <any>
#include <cstdint>
#include <cstring>
#include <utility>
#include <variant>
#include <vector>

#include "prepare_renderer_resources.hpp"

static constexpr int kOverlayID = 0;

SDLPrepareRendererResources::SDLPrepareRendererResources(SDL_GPUDevice* device)
    : Device{device}
{
}

CesiumAsync::Future<Cesium3DTilesSelection::TileLoadResultAndRenderResources> SDLPrepareRendererResources::prepareInLoadThread(
    const CesiumAsync::AsyncSystem& asyncSystem,
    Cesium3DTilesSelection::TileLoadResult&& tileLoadResult,
    const glm::dmat4& transform,
    const std::any& rendererOptions)
{
    auto nullFuture = asyncSystem.createResolvedFuture(Cesium3DTilesSelection::TileLoadResultAndRenderResources{std::move(tileLoadResult), nullptr});
    CesiumGltf::Model* rootModel = std::get_if<CesiumGltf::Model>(&tileLoadResult.contentKind);
    if (!rootModel)
    {
        return nullFuture;
    }
    SDL_GPUCommandBuffer* commandBuffer = SDL_AcquireGPUCommandBuffer(Device);
    if (!commandBuffer)
    {
        SDL_Log("Failed to acquire command buffer: %s", SDL_GetError());
        return nullFuture;
    }
    SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(commandBuffer);
    if (!copyPass)
    {
        SDL_Log("Failed to begin copy pass: %s", SDL_GetError());
        SDL_CancelGPUCommandBuffer(commandBuffer);
        return nullFuture;
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
            auto overlay0It = primitive.attributes.find("_CESIUMOVERLAY_0");
            CesiumGltf::AccessorView<glm::vec2> overlay0View;
            if (overlay0It != primitive.attributes.end())
            {
                overlay0View = CesiumGltf::AccessorView<glm::vec2>(model, overlay0It->second);
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
                if (overlay0View.size() == numVertices)
                {
                    vertexData[i].Overlay0 = overlay0View[i];
                }
                else
                {
                    vertexData[i].Overlay0 = glm::vec2(0.0f);
                }
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
                    indexData = static_cast<uint32_t*>(SDL_MapGPUTransferBuffer(Device, indexTransferBuffer, false));
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
                indexElementSize,
                glm::mat4(transform),
            });
        });
    SDL_EndGPUCopyPass(copyPass);
    SDL_SubmitGPUCommandBuffer(commandBuffer);
    if (resources->Primitives.empty())
    {
        SDL_Log("Tile has no primitives");
    }
    return asyncSystem.createResolvedFuture(Cesium3DTilesSelection::TileLoadResultAndRenderResources{std::move(tileLoadResult), resources});
}

void* SDLPrepareRendererResources::prepareInMainThread(
    Cesium3DTilesSelection::Tile& tile,
    void* pLoadThreadResult)
{
    return pLoadThreadResult;
}

void SDLPrepareRendererResources::free(
    Cesium3DTilesSelection::Tile& tile,
    void* pLoadThreadResult,
    void* pMainThreadResult) noexcept
{
    auto freeTile = [this](void* tile)
    {
        if (!tile)
        {
            return;
        }
        SDLPrepareRendererResourcesTile* resources = static_cast<SDLPrepareRendererResourcesTile*>(tile);
        for (SDLPrepareRendererResourcesTileMesh& primitive : resources->Primitives)
        {
            SDL_ReleaseGPUBuffer(Device, primitive.VertexBuffer);
            SDL_ReleaseGPUBuffer(Device, primitive.IndexBuffer);
        }
        delete resources;
    };
    freeTile(pLoadThreadResult);
    if (pLoadThreadResult != pMainThreadResult)
    {
        freeTile(pMainThreadResult);
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
    if (overlayTextureCoordinateID != kOverlayID)
    {
        return;
    }
    auto tileRenderContent = tile.getContent().getRenderContent();
    if (!tileRenderContent)
    {
        return;
    }
    SDLPrepareRendererResourcesTile* resources = static_cast<SDLPrepareRendererResourcesTile*>(tileRenderContent->getRenderResources());
    if (!resources)
    {
        return;
    }
    SDLPrepareRendererResourcesRasterOverlayTile* rasterResources = static_cast<SDLPrepareRendererResourcesRasterOverlayTile*>(pMainThreadRendererResources);
    if (!rasterResources)
    {
        return;
    }
    resources->RasterOverlays.push_back({rasterResources, translation, scale});
}

void SDLPrepareRendererResources::detachRasterInMainThread(
    const Cesium3DTilesSelection::Tile& tile,
    int32_t overlayTextureCoordinateID,
    const CesiumRasterOverlays::RasterOverlayTile& rasterTile,
    void* pMainThreadRendererResources) noexcept
{
    auto tileRenderContent = tile.getContent().getRenderContent();
    if (!tileRenderContent)
    {
        return;
    }
    SDLPrepareRendererResourcesTile* resources = static_cast<SDLPrepareRendererResourcesTile*>(tileRenderContent->getRenderResources());
    if (!resources)
    {
        return;
    }
    SDLPrepareRendererResourcesRasterOverlayTile* rasterResources = static_cast<SDLPrepareRendererResourcesRasterOverlayTile*>(pMainThreadRendererResources);
    if (!rasterResources)
    {
        return;
    }
    for (auto it = resources->RasterOverlays.begin(); it != resources->RasterOverlays.end(); it++)
    {
        if (it->RasterTile == rasterResources && overlayTextureCoordinateID == kOverlayID)
        {
            resources->RasterOverlays.erase(it);
            break;
        }
    }
}

void* SDLPrepareRendererResources::prepareRasterInLoadThread(
    CesiumGltf::ImageAsset& image,
    const std::any& rendererOptions)
{
    if (image.width == 0 || image.height == 0 || image.bytesPerChannel != 1)
    {
        return nullptr;
    }
    if (image.channels != 4)
    {
        image.changeNumberOfChannels(4, std::byte{255});
    }
    SDL_GPUTexture* texture = nullptr;
    {
        SDL_GPUTextureCreateInfo info{};
        info.type = SDL_GPU_TEXTURETYPE_2D;
        info.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
        info.width = image.width;
        info.height = image.height;
        info.layer_count_or_depth = 1;
        info.num_levels = 1;
        info.sample_count = SDL_GPU_SAMPLECOUNT_1;
        info.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
        texture = SDL_CreateGPUTexture(Device, &info);
    }
    if (!texture)
    {
        SDL_Log("Failed to create raster overlay texture: %s", SDL_GetError());
        return nullptr;
    }
    SDL_GPUTransferBuffer* transferBuffer = nullptr;
    {
        SDL_GPUTransferBufferCreateInfo info{};
        info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
        info.size = image.pixelData.size();
        transferBuffer = SDL_CreateGPUTransferBuffer(Device, &info);
    }
    if (!transferBuffer)
    {
        SDL_Log("Failed to create transfer buffer: %s", SDL_GetError());
        SDL_ReleaseGPUTexture(Device, texture);
        return nullptr;
    }
    void* textureData = SDL_MapGPUTransferBuffer(Device, transferBuffer, false);
    if (!textureData)
    {
        SDL_Log("Failed to map transfer buffer: %s", SDL_GetError());
        SDL_ReleaseGPUTransferBuffer(Device, transferBuffer);
        SDL_ReleaseGPUTexture(Device, texture);
        return nullptr;
    }
    std::memcpy(textureData, image.pixelData.data(), image.pixelData.size());
    SDL_UnmapGPUTransferBuffer(Device, transferBuffer);
    SDL_GPUCommandBuffer* commandBuffer = SDL_AcquireGPUCommandBuffer(Device);
    if (!commandBuffer)
    {
        SDL_Log("Failed to acquire command buffer: %s", SDL_GetError());
        SDL_ReleaseGPUTransferBuffer(Device, transferBuffer);
        SDL_ReleaseGPUTexture(Device, texture);
        return nullptr;
    }
    SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(commandBuffer);
    if (!copyPass)
    {
        SDL_Log("Failed to begin copy pass: %s", SDL_GetError());
        SDL_CancelGPUCommandBuffer(commandBuffer);
        SDL_ReleaseGPUTransferBuffer(Device, transferBuffer);
        SDL_ReleaseGPUTexture(Device, texture);
    }
    {
        SDL_GPUTextureTransferInfo info{};
        info.transfer_buffer = transferBuffer;
        SDL_GPUTextureRegion region{};
        region.texture = texture;
        region.w = image.width;
        region.h = image.height;
        region.d = 1;
        SDL_UploadToGPUTexture(copyPass, &info, &region, false);
        SDL_EndGPUCopyPass(copyPass);
        SDL_ReleaseGPUTransferBuffer(Device, transferBuffer);
    }
    SDL_SubmitGPUCommandBuffer(commandBuffer);
    SDLPrepareRendererResourcesRasterOverlayTile* rasterResources = new SDLPrepareRendererResourcesRasterOverlayTile();
    rasterResources->Texture = texture;
    return rasterResources;
}

void* SDLPrepareRendererResources::prepareRasterInMainThread(
    CesiumRasterOverlays::RasterOverlayTile& rasterTile,
    void* pLoadThreadResult)
{
    return pLoadThreadResult;
}

void SDLPrepareRendererResources::freeRaster(
    const CesiumRasterOverlays::RasterOverlayTile& rasterTile,
    void* pLoadThreadResult,
    void* pMainThreadResult) noexcept
{
    auto freeOverlayTile = [this](void* tile)
    {
        if (!tile)
        {
            return;
        }
        SDLPrepareRendererResourcesRasterOverlayTile* rasterResources = static_cast<SDLPrepareRendererResourcesRasterOverlayTile*>(tile);
        SDL_ReleaseGPUTexture(Device, rasterResources->Texture);
        delete rasterResources;
    };
    freeOverlayTile(pLoadThreadResult);
    if (pLoadThreadResult != pMainThreadResult)
    {
        freeOverlayTile(pMainThreadResult);
    }
}
