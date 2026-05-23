#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlgpu3.h>

#include <cstdint>
#include <filesystem>
#include <memory>

#include "task_processor.hpp"
#include "prepare_renderer_resources.hpp"
#include "tileset.hpp"

static constexpr const char* kDefaultIonTokenFileName = "cesium_ion_token.txt";

static SDL_Window* window;
static SDL_GPUDevice* device;
static std::shared_ptr<SDLPrepareRendererResources> prepareRendererResources;
static std::shared_ptr<SDLTileset> tileset;
static uint64_t time1;
static uint64_t time2;
static float dt;

static bool Init()
{
#ifndef NDEBUG
    SDL_SetLogPriorities(SDL_LOG_PRIORITY_VERBOSE);
#endif
    SDL_SetAppMetadata("SDL Earth", nullptr, nullptr);
    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        SDL_Log("Failed to initialize SDL: %s", SDL_GetError());
        return false;
    }
    window = SDL_CreateWindow("SDL Earth", 960, 720, SDL_WINDOW_HIDDEN);
    if (!window)
    {
        SDL_Log("Failed to create window: %s", SDL_GetError());
        return false;
    }
#ifndef NDEBUG
    device = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_MSL, true, nullptr);
#else
    device = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_DXIL | SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_MSL, false, nullptr);
#endif
    if (!device)
    {
        SDL_Log("Failed to create device: %s", SDL_GetError());
        return false;
    }
    if (!SDL_ClaimWindowForGPUDevice(device, window))
    {
        SDL_Log("Failed to claim window: %s", SDL_GetError());
        return false;
    }
    if (SDL_WindowSupportsGPUPresentMode(device, window, SDL_GPU_PRESENTMODE_MAILBOX))
    {
        SDL_SetGPUSwapchainParameters(device, window,
            SDL_GPU_SWAPCHAINCOMPOSITION_SDR, SDL_GPU_PRESENTMODE_MAILBOX);
    }
    SDL_ShowWindow(window);
    SDL_SetWindowResizable(window, true);
    SDL_FlashWindow(window, SDL_FLASH_BRIEFLY);
    {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGui_ImplSDL3_InitForSDLGPU(window);
        ImGui_ImplSDLGPU3_InitInfo info{};
        info.Device = device;
        info.ColorTargetFormat = SDL_GetGPUSwapchainTextureFormat(device, window);
        ImGui_ImplSDLGPU3_Init(&info);
    }
    prepareRendererResources = std::make_shared<SDLPrepareRendererResources>(device);
    {
        SDLTilesetConfig config;
        config.IonTokenPath = SDL_GetUserFolder(SDL_FOLDER_HOME);
        config.IonTokenPath /= kDefaultIonTokenFileName;
        config.IonAssetID = 1;
        config.IonImageryID = 2;
        tileset = SDLTileset::Create(config);
        if (!tileset)
        {
            SDL_Log("Failed to create tileset");
            // Noop
        }
    }
    return true;
}

static void Quit()
{
    SDL_HideWindow(window);
    tileset.reset();
    prepareRendererResources.reset();
    ImGui_ImplSDLGPU3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
    SDL_ReleaseWindowFromGPUDevice(device, window);
    SDL_DestroyGPUDevice(device);
    SDL_DestroyWindow(window);
    SDL_Quit();
}

static bool Poll()
{
    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
        ImGui_ImplSDL3_ProcessEvent(&event);
        switch (event.type)
        {
        case SDL_EVENT_QUIT:
            return false;
        }
    }
    return true;
}

static void Update()
{
}

static void Render()
{
}

int main(int argc, char** argv)
{
    if (!Init())
    {
        return 1;
    }
    time2 = SDL_GetTicksNS();
    time1 = time2;
    while (true)
    {
        time2 = SDL_GetTicksNS();
        dt = time2 - time1;
        time1 = time2;
        if (!Poll())
        {
            break;
        }
        Update();
        Render();
    }
    Quit();
    return 0;
}