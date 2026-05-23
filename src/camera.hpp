#pragma once

#include <Cesium3DTilesSelection/ViewState.h>
#include <SDL3/SDL.h>
#include <glm/glm.hpp>

class SDLCamera
{
public:
    SDLCamera();
    void Handle(const SDL_Event& event);
    void Resize(uint32_t width, uint32_t height);
    Cesium3DTilesSelection::ViewState GetViewState() const;
    glm::dmat4 GetViewMatrix() const;
    glm::dmat4 GetProjectionMatrix() const;
    glm::dvec3 GetPosition() const;
    uint32_t GetWidth() const;
    uint32_t GetHeight() const;
    double GetAspectRatio() const;

private:
    glm::dvec3 Target;
    glm::uvec2 Viewport;
    double Distance;
    double Pitch;
    double Yaw;
};
