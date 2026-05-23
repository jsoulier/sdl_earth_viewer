#pragma once

#include <CesiumAsync/ITaskProcessor.h>

#include <functional>

class SDLTaskProcessor : public CesiumAsync::ITaskProcessor
{
public:
    void startTask(std::function<void()> function) override;
};
