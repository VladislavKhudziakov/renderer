#pragma once

#include <errors/error_handler.hpp>

#include <cinttypes>
#include <functional>

namespace app
{

    class base_app
    {
    public:
        virtual int32_t run(int32_t argn, const char** argv);

    protected:
        virtual errors::error init_window() = 0;
        virtual errors::error run_main_loop() = 0;
        virtual errors::error cleanup() = 0;
    };
} // namespace renderer
