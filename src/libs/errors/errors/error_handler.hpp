#pragma once

#include <cinttypes>
#include <string>


#define ERROR_WARN(code, m) errors::error(errors::WARN, uint32_t(code), std::string(m) + " in file " + __FILE__ " at line " + std::to_string(__LINE__))
#define ERROR_FATAL(code, m) errors::error(errors::FATAL, uint32_t(code), std::string(m) + " in file " + __FILE__ " at line " + std::to_string(__LINE__))
#define ERROR(code, m) errors::error(errors::ERR, uint32_t(code), std::string(m) + " in file " + __FILE__ " at line " + std::to_string(__LINE__))
#define ERROR_DEBUG(code, message) errors::error(errors::ERR, code, std::string(message) + " in file " __FILE__ " at line " + std::to_string(__LINE__), errors::error::serverity::debug)

namespace errors
{
    enum state
    {
        OK,
        WARN,
        ERR,
        FATAL
    };

    enum serverity
    {
        ALL,
        DEBUG,
        RELEASE
    };

    struct error
    {
        error() = default;
        ~error() = default;
        error(state err_state, int32_t code = 0, const std::string& msg = "", serverity s = ALL);

        operator state() const
        {
            return err_state;
        }

        state err_state{OK};
        serverity err_serverity{ALL};
        uint32_t code{0};
        std::string message;
    };

    void handle_error(error e);
} // namespace errors
