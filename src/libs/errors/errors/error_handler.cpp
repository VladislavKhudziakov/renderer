

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

