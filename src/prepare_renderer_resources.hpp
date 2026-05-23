#pragma once

#include <Cesium3DTilesSelection/IPrepareRendererResources.h>
#include <SDL3/SDL.h>
#include <glm/glm.hpp>

#include <cstdint>

class SDLPrepareRendererResources : public Cesium3DTilesSelection::IPrepareRendererResources
{
public:
    SDLPrepareRendererResources();
    ~SDLPrepareRendererResources() override = default;

    CesiumAsync::Future<Cesium3DTilesSelection::TileLoadResultAndRenderResources> prepareInLoadThread(
        const CesiumAsync::AsyncSystem& asyncSystem,
        Cesium3DTilesSelection::TileLoadResult&& tileLoadResult,
        const glm::dmat4& transform,
        const std::any& rendererOptions) override;
    void* prepareInMainThread(
        Cesium3DTilesSelection::Tile& tile,
        void* pLoadThreadResult) override;
    void free(
        Cesium3DTilesSelection::Tile& tile,
        void* pLoadThreadResult,
        void* pMainThreadResult) noexcept override;
    void attachRasterInMainThread(
        const Cesium3DTilesSelection::Tile& tile,
        int32_t overlayTextureCoordinateID,
        const CesiumRasterOverlays::RasterOverlayTile& rasterTile,
        void* pMainThreadRendererResources,
        const glm::dvec2& translation,
        const glm::dvec2& scale) override;
    void detachRasterInMainThread(
        const Cesium3DTilesSelection::Tile& tile,
        int32_t overlayTextureCoordinateID,
        const CesiumRasterOverlays::RasterOverlayTile& rasterTile,
        void* pMainThreadRendererResources) noexcept override;
    void* prepareRasterInLoadThread(
        CesiumGltf::ImageAsset& image,
        const std::any& rendererOptions) override;
    void* prepareRasterInMainThread(
        CesiumRasterOverlays::RasterOverlayTile& rasterTile,
        void* pLoadThreadResult) override;
    void freeRaster(
        const CesiumRasterOverlays::RasterOverlayTile& rasterTile,
        void* pLoadThreadResult,
        void* pMainThreadResult) noexcept override;
};
