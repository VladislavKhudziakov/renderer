

#include "base_app.hpp"

#include <errors/error_handler.hpp>

int32_t app::base_app::run(int32_t argn, const char** argv)
{
    HANDLE_ERROR(init_window());
    HANDLE_ERROR(run_main_loop());
    HANDLE_ERROR(cleanup());

    return 0;
}
