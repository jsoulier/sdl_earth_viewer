#include <Cesium3DTilesSelection/ViewState.h>
#include <CesiumGeometry/Transforms.h>
#include <SDL3/SDL.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>
#include <limits>

#include "camera.hpp"

static constexpr glm::dvec3 kUp = glm::dvec3(0.0, 0.0, 1.0); // Z is up in ECEF
static constexpr double kMaxPitch = glm::pi<double>() / 2.0 - 0.001;
static constexpr double kNear = 1.0;
static constexpr double kEarthRadius = 6378.137e3;
static constexpr double kMinDistance = 100.0;
static constexpr double kArcSpeed = 0.005;
static constexpr double kPanSpeed = 0.001;
static constexpr double kZoomSpeed = 0.1;
static constexpr double kFovY = glm::radians(45.0);

SDLCamera::SDLCamera()
    : Target{0.0, 0.0, 0.0}
    , Viewport{0, 0}
    , Distance{20000.0e3}
    , Pitch{0.0}
    , Yaw{0.0}
{
}

void SDLCamera::Handle(const SDL_Event& event)
{
    const glm::dvec3 position = GetPosition();
    const glm::dvec3 forward = glm::normalize(Target - position);
    const glm::dvec3 right = glm::normalize(glm::cross(forward, kUp));
    const glm::dvec3 up = glm::normalize(glm::cross(right, forward));
    switch (event.type)
    {
    case SDL_EVENT_MOUSE_MOTION:
    {
        if (event.motion.state & SDL_BUTTON_MASK(SDL_BUTTON_LEFT))
        {
            Yaw -= event.motion.xrel * kArcSpeed;
            Pitch += event.motion.yrel * kArcSpeed;
            Pitch = std::clamp(Pitch, -kMaxPitch, kMaxPitch);
        }
        else if (event.motion.state & SDL_BUTTON_MASK(SDL_BUTTON_RIGHT))
        {
            double panSpeed = Distance * kPanSpeed;
            Target -= right * double(event.motion.xrel) * panSpeed;
            Target += up * double(event.motion.yrel) * panSpeed;
        }
        break;
    }
    case SDL_EVENT_MOUSE_WHEEL:
    {
        Distance -= event.wheel.y * Distance * kZoomSpeed;
        double minDistance = std::max(0.0, kEarthRadius - glm::length(Target)) + kMinDistance;
        Distance = std::max(Distance, minDistance);
        break;
    }
    }
}

void SDLCamera::Resize(uint32_t width, uint32_t height)
{
    Viewport = {width, height};
}

bool SDLCamera::IsValid() const
{
    return Viewport.x != 0 && Viewport.y != 0;
}

Cesium3DTilesSelection::ViewState SDLCamera::GetViewState() const
{
    return Cesium3DTilesSelection::ViewState(GetViewMatrix(), GetViewStateProjMatrix(), glm::dvec2(Viewport));
}

glm::dmat4 SDLCamera::GetViewMatrix() const
{
    return CesiumGeometry::Transforms::createViewMatrix(GetPosition(), glm::normalize(Target - GetPosition()), kUp);
}

glm::dmat4 SDLCamera::GetProjMatrix() const
{
    glm::dmat4 proj = GetViewStateProjMatrix();
    proj[1][1] *= -1.0; // Cesium uses Vulkan Y down, SDL expects Y up
    return proj;
}

glm::dvec3 SDLCamera::GetPosition() const
{
    double x = Distance * std::cos(Pitch) * std::cos(Yaw);
    double y = Distance * std::cos(Pitch) * std::sin(Yaw);
    double z = Distance * std::sin(Pitch);
    return Target + glm::dvec3{x, y, z};
}

double SDLCamera::GetDistance() const
{
    return Distance;
}

uint32_t SDLCamera::GetWidth() const
{
    return Viewport.x;
}

uint32_t SDLCamera::GetHeight() const
{
    return Viewport.y;
}

double SDLCamera::GetAspectRatio() const
{
    return double(Viewport.x) / double(Viewport.y);
}

double SDLCamera::GetFovX() const
{
    return 2.0 * std::atan(std::tan(kFovY / 2.0) * GetAspectRatio());
}

glm::dmat4 SDLCamera::GetViewStateProjMatrix() const
{
    return CesiumGeometry::Transforms::createPerspectiveMatrix(GetFovX(), kFovY, kNear, kNear * 1e8);
}
