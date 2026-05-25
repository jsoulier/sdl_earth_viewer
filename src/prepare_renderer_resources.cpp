#include <Cesium3DTilesSelection/Tile.h>
#include <Cesium3DTilesSelection/TileLoadResult.h>
#include <CesiumAsync/AsyncSystem.h>
#include <CesiumAsync/Future.h>
#include <CesiumGltf/AccessorView.h>
#include <CesiumGltf/ImageAsset.h>
#include <CesiumGltf/Model.h>
#include <CesiumGltfContent/GltfUtilities.h>
#include <CesiumRasterOverlays/RasterOverlayTile.h>
#include <SDL3/SDL.h>
#include <glm/glm.hpp>

#include <algorithm>
#include <any>
#include <cstdint>
#include <cstring>
#include <utility>
#include <variant>
#include <vector>

#include "config.hpp"
#include "prepare_renderer_resources.hpp"

SDLPrepareRendererResources::SDLPrepareRendererResources(SDL_GPUDevice* device)
    : Device{device}
{
}

CesiumAsync::Future<Cesium3DTilesSelection::TileLoadResultAndRenderResources> SDLPrepareRendererResources::prepareInLoadThread(
    const CesiumAsync::AsyncSystem& asyncSystem,
    Cesium3DTilesSelection::TileLoadResult&& tileLoadResult,
    const glm::dmat4& tileTransform,
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
    glm::dmat4 rootTransform = CesiumGltfContent::GltfUtilities::applyRtcCenter(*rootModel, tileTransform);
    rootTransform = CesiumGltfContent::GltfUtilities::applyGltfUpAxisTransform(*rootModel, rootTransform);
    rootModel->forEachPrimitiveInScene(-1, [&](
        const CesiumGltf::Model& model,
        const CesiumGltf::Node& node,
        const CesiumGltf::Mesh& mesh,
        const CesiumGltf::MeshPrimitive& primitive,
        const glm::dmat4& nodeTransform)
        {
            auto positionIt = primitive.attributes.find("POSITION");
            if (positionIt == primitive.attributes.end())
            {
                SDL_Log("Failed to find position attribute");
                return;
            }
            const CesiumGltf::Accessor* positionAccessor = CesiumGltf::Model::getSafe(&model.accessors, positionIt->second);
            if (!positionAccessor)
            {
                SDL_Log("Failed to get position accessor");
                return;
            }
            auto overlay0It = primitive.attributes.find("_CESIUMOVERLAY_0");
            SDL_GPUTransferBuffer* vertexTransferBuffer = nullptr;
            SDL_GPUBuffer* vertexBuffer = nullptr;
            uint32_t numVertices = static_cast<uint32_t>(positionAccessor->count);
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
            CesiumGltf::createAccessorView(model, *positionAccessor, [&](auto&& positionView)
            {
                if (positionView.status() != CesiumGltf::AccessorViewStatus::Valid)
                {
                    SDL_Log("Position view was invalid");
                    return;
                }
                for (uint32_t i = 0; i < numVertices; i++)
                {
                    auto position = positionView[i];
                    vertexData[i].Position = glm::vec3(position.value[0], position.value[1], position.value[2]);
                    vertexData[i].Overlay0 = glm::vec2(0.0f);
                }
            });
            if (overlay0It != primitive.attributes.end())
            {
                const CesiumGltf::Accessor* overlay0Accessor = CesiumGltf::Model::getSafe(&model.accessors, overlay0It->second);
                if (overlay0Accessor)
                {
                    CesiumGltf::createAccessorView(model, *overlay0Accessor, [&](auto&& overlay0View)
                    {
                        if (overlay0View.status() != CesiumGltf::AccessorViewStatus::Valid)
                        {
                            SDL_Log("Overlay0 view was invalid");
                            return;
                        }
                        for (uint32_t i = 0; i < std::min(numVertices, uint32_t(overlay0View.size())); i++)
                        {
                            auto overlay0 = overlay0View[i];
                            vertexData[i].Overlay0 = glm::vec2(overlay0.value[0], overlay0.value[1]);
                        }
                    });
                }
            }
            SDL_UnmapGPUTransferBuffer(Device, vertexTransferBuffer);
            {
                SDL_GPUTransferBufferLocation location{};
                SDL_GPUBufferRegion region{};
                location.transfer_buffer = vertexTransferBuffer;
                region.buffer = vertexBuffer;
                region.size = numVertices * sizeof(SDLPrepareRendererResourcesTileMeshVertex);
                SDL_UploadToGPUBuffer(copyPass, &location, &region, false);
            }
            SDL_ReleaseGPUTransferBuffer(Device, vertexTransferBuffer);
            vertexTransferBuffer = nullptr;
            SDL_GPUTransferBuffer* indexTransferBuffer = nullptr;
            SDL_GPUBuffer* indexBuffer = nullptr;
            uint32_t numIndices = 0;
            void* indexData = nullptr;
            SDL_GPUIndexElementSize indexElementSize = SDL_GPU_INDEXELEMENTSIZE_16BIT;
            const CesiumGltf::Accessor* indexAccessor = CesiumGltf::Model::getSafe(&model.accessors, primitive.indices);
            if (indexAccessor)
            {
                numIndices = static_cast<uint32_t>(indexAccessor->count);
                uint32_t stride = 0;
                if (indexAccessor->componentType == CesiumGltf::Accessor::ComponentType::UNSIGNED_INT)
                {
                    indexElementSize = SDL_GPU_INDEXELEMENTSIZE_32BIT;
                    stride = sizeof(uint32_t);
                }
                else
                {
                    indexElementSize = SDL_GPU_INDEXELEMENTSIZE_16BIT;
                    stride = sizeof(uint16_t);
                }
                {
                    SDL_GPUTransferBufferCreateInfo info{};
                    info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
                    info.size = numIndices * stride;
                    indexTransferBuffer = SDL_CreateGPUTransferBuffer(Device, &info);
                }
                {
                    SDL_GPUBufferCreateInfo info{};
                    info.usage = SDL_GPU_BUFFERUSAGE_INDEX;
                    info.size = numIndices * stride;
                    indexBuffer = SDL_CreateGPUBuffer(Device, &info);
                }
                if (indexTransferBuffer && indexBuffer)
                {
                    indexData = SDL_MapGPUTransferBuffer(Device, indexTransferBuffer, false);
                    if (indexData)
                    {
                        uint32_t* index32Data = static_cast<uint32_t*>(indexData);
                        uint16_t* index16Data = static_cast<uint16_t*>(indexData);
                        CesiumGltf::createAccessorView(model, *indexAccessor, [&](auto&& indexView)
                        {
                            if (indexView.status() != CesiumGltf::AccessorViewStatus::Valid)
                            {
                                return;
                            }
                            for (uint32_t i = 0; i < numIndices; i++)
                            {
                                if (stride == 4)
                                {
                                    index32Data[i] = indexView[i].value[0];
                                }
                                else
                                {
                                    index16Data[i] = indexView[i].value[0];
                                }
                            }
                        });
                        SDL_UnmapGPUTransferBuffer(Device, indexTransferBuffer);
                        {
                            SDL_GPUTransferBufferLocation location{};
                            SDL_GPUBufferRegion region{};
                            location.transfer_buffer = indexTransferBuffer;
                            region.buffer = indexBuffer;
                            region.size = numIndices * stride;
                            SDL_UploadToGPUBuffer(copyPass, &location, &region, false);
                        }
                        SDL_ReleaseGPUTransferBuffer(Device, indexTransferBuffer);
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
                rootTransform * nodeTransform,
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
    if (overlayTextureCoordinateID != kRasterOverlayID)
    {
        SDL_Log("Tried to attach an unsupported raster overlay");
        return;
    }
    const Cesium3DTilesSelection::TileRenderContent* tileRenderContent = tile.getContent().getRenderContent();
    if (!tileRenderContent)
    {
        return;
    }
    SDLPrepareRendererResourcesTile* resources = static_cast<SDLPrepareRendererResourcesTile*>(tileRenderContent->getRenderResources());
    if (!resources)
    {
        return;
    }
    SDL_GPUTexture* texture = static_cast<SDL_GPUTexture*>(pMainThreadRendererResources);
    if (!texture)
    {
        return;
    }
    resources->RasterOverlays.emplace_back(texture, translation, scale);
}

void SDLPrepareRendererResources::detachRasterInMainThread(
    const Cesium3DTilesSelection::Tile& tile,
    int32_t overlayTextureCoordinateID,
    const CesiumRasterOverlays::RasterOverlayTile& rasterTile,
    void* pMainThreadRendererResources) noexcept
{
    if (overlayTextureCoordinateID != kRasterOverlayID)
    {
        SDL_Log("Tried to detach an unsupported raster overlay");
        return;
    }
    const Cesium3DTilesSelection::TileRenderContent* tileRenderContent = tile.getContent().getRenderContent();
    if (!tileRenderContent)
    {
        return;
    }
    SDLPrepareRendererResourcesTile* resources = static_cast<SDLPrepareRendererResourcesTile*>(tileRenderContent->getRenderResources());
    if (!resources)
    {
        return;
    }
    SDL_GPUTexture* texture = static_cast<SDL_GPUTexture*>(pMainThreadRendererResources);
    if (!texture)
    {
        return;
    }
    for (auto it = resources->RasterOverlays.begin(); it != resources->RasterOverlays.end(); it++)
    {
        if (it->Texture == texture)
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
    if (image.width == 0 || image.height == 0 || image.bytesPerChannel != 1 || image.pixelData.empty())
    {
        SDL_Log("Tried to prepare an invalid image");
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
        return nullptr;
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
    }
    SDL_EndGPUCopyPass(copyPass);
    SDL_ReleaseGPUTransferBuffer(Device, transferBuffer);
    SDL_SubmitGPUCommandBuffer(commandBuffer);
    return texture;
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
        SDL_GPUTexture* texture = static_cast<SDL_GPUTexture*>(tile);
        SDL_ReleaseGPUTexture(Device, texture);
    };
    freeOverlayTile(pLoadThreadResult);
    if (pLoadThreadResult != pMainThreadResult)
    {
        freeOverlayTile(pMainThreadResult);
    }
}
