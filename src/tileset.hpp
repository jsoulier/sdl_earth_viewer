#pragma once

#include <Cesium3DTilesSelection/Tileset.h>
#include <Cesium3DTilesSelection/TilesetExternals.h>
#include <CesiumCurl/CurlAssetAccessor.h>
#include <CesiumRasterOverlays/IonRasterOverlay.h>
#include <CesiumUtility/CreditSystem.h>

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>

class SDLCamera;
class SDLPrepareRendererResources;
class SDLTaskProcessor;

class SDLTilesetConfig
{
public:
    SDLTilesetConfig();

    Cesium3DTilesSelection::TilesetOptions TilesetOptions;
    CesiumRasterOverlays::RasterOverlayOptions RasterOverlayOptions;
    std::shared_ptr<SDLPrepareRendererResources> PrepareRendererResources;
    std::filesystem::path IonTokenPath;
    int64_t IonAssetID;
    int64_t IonImageryID;
};

class SDLTileset
{
public:
    SDLTileset();
    const Cesium3DTilesSelection::ViewUpdateResult& Update(const SDLCamera& camera);
    static std::shared_ptr<SDLTileset> Create(const SDLTilesetConfig& config);

private:
    std::unique_ptr<Cesium3DTilesSelection::Tileset> Tileset;
    CesiumAsync::AsyncSystem AsyncSystem;
};
