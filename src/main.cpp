#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlgpu3.h>

#include <cstdint>
#include <filesystem>
#include <memory>

#include "camera.hpp"
#include "debug_group.hpp"
#include "prepare_renderer_resources.hpp"
#include "shader.hpp"
#include "task_processor.hpp"
#include "tileset.hpp"

static constexpr const char* kDefaultIonTokenFileName = "cesium_ion_token.txt";

static SDL_Window* window;
static SDL_GPUDevice* device;
static SDL_GPUTexture* depthTexture;
static SDL_GPUGraphicsPipeline* tilesetPipeline;
static std::shared_ptr<SDLPrepareRendererResources> prepareRendererResources;
static std::shared_ptr<SDLTileset> tileset;
static SDLCamera camera;
static uint64_t time1;
static uint64_t time2;
static float dt;

static bool CreateTilesetPipeline()
{
    SDL_GPUColorTargetDescription colorTargets[1]{};
    colorTargets[0].format = SDL_GetGPUSwapchainTextureFormat(device, window);
    SDL_GPUVertexAttribute vertexAttributes[2]{};
    vertexAttributes[0].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
    vertexAttributes[0].location = 0;
    vertexAttributes[0].offset = offsetof(SDLPrepareRendererResourcesTileMeshVertex, Position);
    vertexAttributes[1].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
    vertexAttributes[1].location = 1;
    vertexAttributes[1].offset = offsetof(SDLPrepareRendererResourcesTileMeshVertex, Overlay0);
    SDL_GPUVertexBufferDescription vertexBuffers[1]{};
    vertexBuffers[0].pitch = sizeof(SDLPrepareRendererResourcesTileMeshVertex);
    vertexBuffers[0].slot = 0;
    SDL_GPUGraphicsPipelineCreateInfo info{};
    info.vertex_shader = LoadShader(device, "tileset.vert");
    info.fragment_shader = LoadShader(device, "tileset.frag");
    info.target_info.num_color_targets = 1;
    info.target_info.color_target_descriptions = colorTargets;
    info.target_info.has_depth_stencil_target = true;
    info.target_info.depth_stencil_format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
    info.vertex_input_state.num_vertex_attributes = 2;
    info.vertex_input_state.vertex_attributes = vertexAttributes;
    info.vertex_input_state.num_vertex_buffers = 1;
    info.vertex_input_state.vertex_buffer_descriptions = vertexBuffers;
    info.depth_stencil_state.enable_depth_test = true;
    info.depth_stencil_state.enable_depth_write = true;
    info.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_LESS;
    info.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_BACK;
    info.rasterizer_state.front_face = SDL_GPU_FRONTFACE_CLOCKWISE;
    if (info.vertex_shader && info.fragment_shader)
    {
        tilesetPipeline = SDL_CreateGPUGraphicsPipeline(device, &info);
    }
    SDL_ReleaseGPUShader(device, info.vertex_shader);
    SDL_ReleaseGPUShader(device, info.fragment_shader);
    return tilesetPipeline != nullptr;
}

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
        int w, h;
        SDL_GetWindowSize(window, &w, &h);
        camera.Resize((uint32_t)w, (uint32_t)h);
    }
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
    if (!CreateTilesetPipeline())
    {
        SDL_Log("Failed to create tileset pipeline");
        return false;
    }
    {
        SDLTilesetConfig config;
        config.IonTokenPath = SDL_GetUserFolder(SDL_FOLDER_HOME);
        config.IonTokenPath /= kDefaultIonTokenFileName;
        config.IonAssetID = 1;
        config.IonImageryID = 2;
        config.PrepareRendererResources = prepareRendererResources;
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
    SDL_ReleaseGPUGraphicsPipeline(device, tilesetPipeline);
    SDL_ReleaseGPUTexture(device, depthTexture);
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
        if (!ImGui::GetIO().WantCaptureMouse && !ImGui::GetIO().WantCaptureKeyboard)
        {
            camera.Handle(event);
        }
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
    if (tileset)
    {
        tileset->Update(camera);
    }
}

static bool Resize(uint32_t width, uint32_t height)
{
    camera.Resize(width, height);
    if (depthTexture)
    {
        SDL_ReleaseGPUTexture(device, depthTexture);
    }
    SDL_GPUTextureCreateInfo info{};
    info.type = SDL_GPU_TEXTURETYPE_2D;
    info.format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
    info.width = width;
    info.height = height;
    info.layer_count_or_depth = 1;
    info.num_levels = 1;
    info.usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;
    depthTexture = SDL_CreateGPUTexture(device, &info);
    if (!depthTexture)
    {
        SDL_Log("Failed to create depth texture: %s", SDL_GetError());
        return false;
    }
    return true;
}

static void Render()
{
    SDL_GPUCommandBuffer* commandBuffer = SDL_AcquireGPUCommandBuffer(device);
    if (!commandBuffer)
    {
        SDL_Log("Failed to acquire command buffer: %s", SDL_GetError());
        return;
    }
    SDL_GPUTexture* swapchainTexture;
    uint32_t width;
    uint32_t height;
    if (!SDL_WaitAndAcquireGPUSwapchainTexture(commandBuffer, window, &swapchainTexture, &width, &height))
    {
        SDL_Log("Failed to acquire command buffer: %s", SDL_GetError());
        SDL_CancelGPUCommandBuffer(commandBuffer);
        return;
    }
    if (!swapchainTexture || !width || !height)
    {
        // not an error
        SDL_SubmitGPUCommandBuffer(commandBuffer);
        return;
    }
    if ((camera.GetWidth() != width || camera.GetHeight() != height) && !Resize(width, height))
    {
        SDL_Log("Failed to create depth texture: %s", SDL_GetError());
        SDL_SubmitGPUCommandBuffer(commandBuffer);
        return;
    }
    {
        DebugGroupBlock(commandBuffer, "Render::Tileset");
        SDL_GPUColorTargetInfo colorInfo{};
        colorInfo.texture = swapchainTexture;
        colorInfo.load_op = SDL_GPU_LOADOP_CLEAR;
        colorInfo.store_op = SDL_GPU_STOREOP_STORE;
        SDL_GPUDepthStencilTargetInfo depthInfo{};
        depthInfo.texture = depthTexture;
        depthInfo.load_op = SDL_GPU_LOADOP_CLEAR;
        depthInfo.stencil_load_op = SDL_GPU_LOADOP_CLEAR;
        depthInfo.store_op = SDL_GPU_STOREOP_STORE;
        depthInfo.clear_depth = 1.0f;
        SDL_GPURenderPass* renderPass = SDL_BeginGPURenderPass(commandBuffer, &colorInfo, 1, &depthInfo);
        if (!renderPass)
        {
            SDL_Log("Failed to begin render pass: %s", SDL_GetError());
            SDL_SubmitGPUCommandBuffer(commandBuffer);
            return;
        }
        if (tileset)
        {
            // TODO:
        }
        SDL_EndGPURenderPass(renderPass);
    }
    {
        DebugGroupBlock(commandBuffer, "Render::PrepareImGui");
        ImGuiIO& io = ImGui::GetIO();
        io.DisplaySize.x = width;
        io.DisplaySize.y = height;
        ImGui_ImplSDLGPU3_NewFrame();
        ImGui::NewFrame();
        ImGui::Begin("Debug");
        ImGui::Text("FPS: %.1f", io.Framerate);
        ImGui::Text("Camera Distance: %.1f km", camera.GetDistance() / 1e3);
        ImGui::End();
        ImGui::Render();
        ImGui_ImplSDLGPU3_PrepareDrawData(ImGui::GetDrawData(), commandBuffer);
    }
    {
        DebugGroupBlock(commandBuffer, "Render::RenderImGui");
        SDL_GPUColorTargetInfo info{};
        info.texture = swapchainTexture;
        info.load_op = SDL_GPU_LOADOP_LOAD;
        info.store_op = SDL_GPU_STOREOP_STORE;
        SDL_GPURenderPass* renderPass = SDL_BeginGPURenderPass(commandBuffer, &info, 1, nullptr);
        if (!renderPass)
        {
            SDL_Log("Failed to begin render pass: %s", SDL_GetError());
            SDL_SubmitGPUCommandBuffer(commandBuffer);
            return;
        }
        ImGui_ImplSDLGPU3_RenderDrawData(ImGui::GetDrawData(), commandBuffer, renderPass);
        SDL_EndGPURenderPass(renderPass);
    }
    SDL_SubmitGPUCommandBuffer(commandBuffer);
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
