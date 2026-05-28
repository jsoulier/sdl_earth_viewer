#include <functional>
#include <mutex>
#include <thread>
#include <utility>

#include "task_processor.hpp"

SDLTaskProcessor::SDLTaskProcessor()
    : Stop{false}
{
    static const int kNumThreads = std::max(1u, std::thread::hardware_concurrency());
    for (int i = 0; i < kNumThreads; i++)
    {
        Workers.emplace_back([this]()
        {
            while (true)
            {
                std::function<void()> task;
                {
                    std::unique_lock<std::mutex> lock(Mutex);
                    Condition.wait(lock, [this]()
                    {
                        return Stop || !Queue.empty();
                    });
                    if (Stop && Queue.empty())
                    {
                        return;
                    }
                    task = std::move(Queue.front());
                    Queue.pop();
                }
                task();
            }
        });
    }
}

SDLTaskProcessor::~SDLTaskProcessor()
{
    {
        std::unique_lock<std::mutex> lock(Mutex);
        Stop = true;
    }
    Condition.notify_all();
    for (std::thread& worker : Workers)
    {
        worker.join();
    }
}

void SDLTaskProcessor::startTask(std::function<void()> function)
{
    {
        std::unique_lock<std::mutex> lock(Mutex);
        Queue.emplace(std::move(function));
    }
    Condition.notify_one();
}
