#pragma once

#include <CesiumAsync/ITaskProcessor.h>

#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

class SDLTaskProcessor : public CesiumAsync::ITaskProcessor
{
public:
    SDLTaskProcessor();
    ~SDLTaskProcessor();
    void startTask(std::function<void()> function) override;

private:
    std::vector<std::thread> Workers;
    std::queue<std::function<void()>> Queue;
    std::mutex Mutex;
    std::condition_variable Condition;
    bool Stop;
};
