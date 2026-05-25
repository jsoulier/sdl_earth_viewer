#include <Cesium3DTilesSelection/ViewState.h>
#include <CesiumGeometry/Transforms.h>
#include <SDL3/SDL.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>
#include <limits>

#include "camera.hpp"

static constexpr glm::dvec3 kUp = glm::dvec3(0.0, 1.0, 0.0);
static constexpr double kMaxPitch = glm::pi<double>() / 2.0 - 0.001;
static constexpr double kNear = 1.0;
static constexpr double kFar = kNear * 1e8;
static constexpr double kEarthRadius = 6378.137e3;
static constexpr double kMinDistanceToEarth = 100.0;
static constexpr double kArcSpeed = 0.005;
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
        break;
    }
    case SDL_EVENT_MOUSE_WHEEL:
    {
        Distance -= event.wheel.y * Distance * kZoomSpeed;
        double minDistance = std::max(0.0, kEarthRadius - glm::length(Target)) + kMinDistanceToEarth;
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
    return Cesium3DTilesSelection::ViewState(GetViewMatrix(), GetProjMatrix(), glm::dvec2(Viewport));
}

glm::dmat4 SDLCamera::GetProjMatrix() const
{
    return CesiumGeometry::Transforms::createPerspectiveMatrix(GetFovX(), kFovY, kNear, kFar);
}

glm::dmat4 SDLCamera::GetViewMatrix() const
{
    return CesiumGeometry::Transforms::createViewMatrix(GetPosition(), glm::normalize(Target - GetPosition()), kUp);
}

glm::dvec3 SDLCamera::GetPosition() const
{
    double x = Distance * std::cos(Pitch) * std::sin(Yaw);
    double y = Distance * std::sin(Pitch);
    double z = Distance * std::cos(Pitch) * std::cos(Yaw);
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
