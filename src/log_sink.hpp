#pragma once

#include <SDL3/SDL.h>
#include <spdlog/sinks/base_sink.h>
#include <spdlog/spdlog.h>

template<typename Mutex>
class SDLLogSink : public spdlog::sinks::base_sink<Mutex>
{
protected:
    void sink_it_(const spdlog::details::log_msg& msg) override
    {
        SDL_Log("%s", msg.payload.data());
    }

    void flush_() override
    {
    }
};
