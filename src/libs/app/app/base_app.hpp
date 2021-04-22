#pragma once

#include <errors/error_handler.hpp>

#include <cinttypes>
#include <functional>

namespace app
{
    class base_app
    {
    public:
        virtual ERROR_TYPE run();

    protected:
        virtual ERROR_TYPE init_window() = 0;
        virtual ERROR_TYPE run_main_loop() = 0;
        virtual ERROR_TYPE cleanup() = 0;
    };
} // namespace app
