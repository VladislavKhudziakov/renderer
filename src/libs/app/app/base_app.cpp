

#include "base_app.hpp"

#include <errors/error_handler.hpp>

ERROR_TYPE app::base_app::run(int argc, const char** argv)
{
    HANDLE_ERROR(init_window());
    HANDLE_ERROR(run_main_loop());
    HANDLE_ERROR(cleanup());

    RAISE_ERROR_OK();
}
