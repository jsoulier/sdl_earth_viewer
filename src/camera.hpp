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
    bool IsValid() const;
    Cesium3DTilesSelection::ViewState GetViewState() const;
    glm::dmat4 GetViewStateProjMatrix() const;
    glm::dmat4 GetProjMatrix() const;
    glm::dmat4 GetViewMatrix() const;
    glm::dvec3 GetPosition() const;
    double GetDistance() const;
    uint32_t GetWidth() const;
    uint32_t GetHeight() const;
    double GetAspectRatio() const;
    double GetFovX() const;

private:
    glm::dvec3 Target;
    glm::uvec2 Viewport;
    double Distance;
    double Pitch;
    double Yaw;
};
