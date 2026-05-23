#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <memory>

#include "task_processor.hpp"
#include "prepare_renderer_resources.hpp"
#include "tileset.hpp"

static std::shared_ptr<SDLPrepareRendererResources> prepareRendererResources;
static std::shared_ptr<SDLTileset> tileset;

int main(int argc, char** argv)
{
    return 0;
}