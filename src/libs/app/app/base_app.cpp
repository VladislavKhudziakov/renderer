

#include "base_app.hpp"

#include <errors/error_handler.hpp>

int32_t app::base_app::run(int32_t argn, const char** argv)
{
    errors::handle_error(init_window());
    errors::handle_error(run_main_loop());
    errors::handle_error(cleanup());
    return 0;
}
