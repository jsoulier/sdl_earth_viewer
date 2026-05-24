#include <Cesium3DTilesSelection/Tileset.h>
#include <Cesium3DTilesSelection/TilesetExternals.h>
#include <CesiumCurl/CurlAssetAccessor.h>
#include <CesiumUtility/CreditSystem.h>
#include <SDL3/SDL.h>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <string>
    
#include "camera.hpp"
#include "log_sink.hpp"
#include "prepare_renderer_resources.hpp"
#include "task_processor.hpp"
#include "tileset.hpp"

static constexpr int kDefaultIonAssetID = 1;
static constexpr int kDefaultIonImageryID = 2;
static constexpr const char* kDefaultIonTokenFileName = "cesium_ion_token.txt";

static std::string GetIonToken(const std::filesystem::path& path) 
{
    std::ifstream file(path);
    if (!file.is_open())
    {
        SDL_Log("Failed to open token file: %s", path.string().data());
        return "";
    }
    std::string token;
    std::getline(file, token);
    size_t end = token.find_last_not_of(" \n\r\t");
    if (end != std::string::npos)
    {
        token = token.substr(0, end + 1);
    }
    else
    {
        token.clear();
    }
    return token;
}

SDLTilesetConfig::SDLTilesetConfig()
    : IonAssetID{kDefaultIonAssetID}
    , IonImageryID{kDefaultIonImageryID}
{
    TilesetOptions.forbidHoles = true;
    TilesetOptions.enableFrustumCulling = false; // TODO: figure out why their frustum culling is over-culling

    const char* home = SDL_GetUserFolder(SDL_FOLDER_HOME);
    if (home)
    {
        IonTokenPath = std::filesystem::path(home) / kDefaultIonTokenFileName;
    }
}

SDLTileset::SDLTileset()
: AsyncSystem{nullptr}
{
}

const Cesium3DTilesSelection::ViewUpdateResult& SDLTileset::Update(const SDLCamera& camera)
{
    if (camera.IsValid())
    {
        Tileset->updateViewGroup(Tileset->getDefaultViewGroup(), {camera.GetViewState()});
    }
    Tileset->loadTiles();
    AsyncSystem.dispatchMainThreadTasks();
    return Tileset->getDefaultViewGroup().getViewUpdateResult();
}

std::shared_ptr<SDLTileset> SDLTileset::Create(const SDLTilesetConfig& config)
{
    if (config.IonAssetID == -1)
    {
        SDL_Log("Ion asset ID is invalid");
        return nullptr;
    }
    const std::string ionToken = GetIonToken(config.IonTokenPath);
    if (ionToken.empty())
    {
        SDL_Log("Ion token is empty");
        return nullptr;
    }
    std::shared_ptr<SDLTileset> tileset = std::make_shared<SDLTileset>();
    std::shared_ptr<SDLLogSink<std::mutex>> logSink = std::make_shared<SDLLogSink<std::mutex>>();
    std::shared_ptr<spdlog::logger> logger = std::make_shared<spdlog::logger>("cesium", logSink);
    std::shared_ptr<SDLTaskProcessor> taskProcessor = std::make_shared<SDLTaskProcessor>();
    std::shared_ptr<CesiumAsync::IAssetAccessor> assetAccessor = std::make_shared<CesiumCurl::CurlAssetAccessor>();
    std::shared_ptr<CesiumUtility::CreditSystem> creditSystem = std::make_shared<CesiumUtility::CreditSystem>();
    tileset->AsyncSystem = CesiumAsync::AsyncSystem(taskProcessor);
    Cesium3DTilesSelection::TilesetExternals externals{assetAccessor, config.PrepareRendererResources, tileset->AsyncSystem, creditSystem, logger};
    tileset->Tileset = std::make_unique<Cesium3DTilesSelection::Tileset>(externals, config.IonAssetID, ionToken, config.TilesetOptions);
    if (config.IonImageryID != -1)
    {
        tileset->Tileset->getOverlays().add(new CesiumRasterOverlays::IonRasterOverlay("overlay", config.IonImageryID, ionToken, config.RasterOverlayOptions));
    }
    return tileset;
}
