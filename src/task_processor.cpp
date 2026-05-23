#include <functional>
#include <thread>

#include "task_processor.hpp"

void SDLTaskProcessor::startTask(std::function<void()> function)
{
    std::thread(function).detach();
}
