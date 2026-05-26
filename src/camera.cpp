#include <Cesium3DTilesSelection/ViewState.h>
#include <CesiumGeometry/Transforms.h>
#include <SDL3/SDL.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>
#include <limits>

#include "camera.hpp"

static constexpr glm::dvec3 kUp = glm::dvec3(0.0, 0.0, 1.0);
static constexpr double kMaxPitch = glm::pi<double>() / 2.0 - 0.001;
static constexpr double kNear = 1.0;
static constexpr double kFar = kNear * 1e8;
static constexpr double kEarthRadius = 6378.137e3;
static constexpr double kArcSpeed = 0.1e-9;
static constexpr double kPanSpeed = 0.001;
static constexpr double kZoomSpeed = 0.1;
static constexpr double kMinSpeed = 100.0;
static constexpr double kFovY = glm::radians(45.0);

SDLCamera::SDLCamera()
    : Target{0.0, 0.0, 0.0}
    , Viewport{0, 0}
    , Distance{20000.0e3}
    , Pitch{0.0}
    , Yaw{glm::half_pi<double>()}
{
}

void SDLCamera::Handle(const SDL_Event& event)
{
    const glm::dvec3 position = GetPosition();
    const glm::dvec3 forward = glm::normalize(Target - position);
    const glm::dvec3 right = glm::normalize(glm::cross(forward, kUp));
    const glm::dvec3 up = glm::normalize(glm::cross(right, forward));
    const double altitude = glm::length(GetPosition()) - kEarthRadius;
    const double speed = std::max(kMinSpeed, altitude);
    switch (event.type)
    {
    case SDL_EVENT_MOUSE_MOTION:
    {
        if (event.motion.state & SDL_BUTTON_MASK(SDL_BUTTON_LEFT))
        {
            Yaw -= event.motion.xrel * speed * kArcSpeed;
            Pitch = std::clamp(Pitch + event.motion.yrel * speed * kArcSpeed, -kMaxPitch, kMaxPitch);
        }
        else if (event.motion.state & SDL_BUTTON_MASK(SDL_BUTTON_RIGHT))
        {
            Target -= right * double(event.motion.xrel) * speed * kPanSpeed;
            Target += up * double(event.motion.yrel) * speed * kPanSpeed;
        }
        break;
    }
    case SDL_EVENT_MOUSE_WHEEL:
    {
        Distance -= event.wheel.y * speed * kZoomSpeed;
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
    return Viewport.x > 0 && Viewport.y > 0;
}

Cesium3DTilesSelection::ViewState SDLCamera::GetViewState() const
{
    return Cesium3DTilesSelection::ViewState(GetViewMatrix(), GetProjMatrix(), glm::dvec2(Viewport));
}

glm::dmat4 SDLCamera::GetProjMatrix() const
{
    return CesiumGeometry::Transforms::createPerspectiveMatrix(GetFovX(), kFovY, kNear, glm::length(GetPosition()));
}

glm::dmat4 SDLCamera::GetViewMatrix() const
{
    return CesiumGeometry::Transforms::createViewMatrix(GetPosition(), glm::normalize(Target - GetPosition()), kUp);
}

glm::dvec3 SDLCamera::GetPosition() const
{
    double x = Distance * std::cos(Pitch) * std::cos(Yaw);
    double y = Distance * std::cos(Pitch) * std::sin(Yaw);
    double z = Distance * std::sin(Pitch);
    return Target + glm::dvec3{x, y, z};
}

glm::dvec3 SDLCamera::GetTarget() const
{
    return Target;
}

double SDLCamera::GetDistance() const
{
    return Distance;
}

double SDLCamera::GetPitch() const
{
    return Pitch;
}

double SDLCamera::GetYaw() const
{
    return Yaw;
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
