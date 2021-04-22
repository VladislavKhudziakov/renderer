

#include "error_handler.hpp"

#include <logger/log.hpp>
#include <cassert>

#ifndef NDEBUG
errors::error::error(state err_state, int32_t code, const std::string& msg)
    : std::exception()
    , err_state(err_state)
    , code(code)
    , m_message(msg + " Code " + std::to_string(code))
{
}

const char* errors::error::what() const noexcept
{
    return m_message.c_str();
}
#else
errors::error::error(errors::state state, int32_t code, const std::string& msg)
    : err_state(state)
    , code(state == OK ? 0 : (code == 0 ? -1 : code))
    , message(msg)
{
}

void errors::handle_error(errors::error e)
{
    switch (e.err_state) {
        case OK:
            return;
        case WARN:
            LOG_WARN(e.message);
            break;
        case FATAL:
            LOG_ERROR(e.message);
            assert(false);
            break;
    }
}
#endif

ERROR_TYPE errors::handle_error_code(int32_t code, bool (*on_error)(int32_t), const char* err_msg)
{
    if (code != 0) {
        if (on_error != nullptr) {
            if (on_error(code)) {
                RAISE_ERROR_FATAL(code, err_msg != nullptr ? err_msg : "invalid error code. " + std::to_string(code));
            } else {
                RAISE_ERROR_OK();
            }
        } else {
            RAISE_ERROR_FATAL(code, err_msg != nullptr ? err_msg : "invalid error code. " + std::to_string(code));
        }
    }
}

ERROR_TYPE errors::handle_error_code(int32_t code, const std::function<bool(int32_t)>& on_error, const char* err_msg)
{
    if (code != 0) {
        if (on_error) {
            if (on_error(code)) {
                RAISE_ERROR_FATAL(code, err_msg != nullptr ? err_msg : "invalid error code. " + std::to_string(code));
            } else {
                RAISE_ERROR_OK();
            }
        } else {
            RAISE_ERROR_FATAL(code, err_msg != nullptr ? err_msg : "invalid error code. " + std::to_string(code));
        }
    }
}