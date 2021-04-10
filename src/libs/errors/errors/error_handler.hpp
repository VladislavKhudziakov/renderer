#pragma once

#include <logger/log.hpp>

#include <cinttypes>
#include <string>


//#define ERROR_WARN(code, m) errors::error(errors::WARN, uint32_t(code), std::string(m) + " in file " + __FILE__ " at line " + std::to_string(__LINE__))
//#define ERROR_FATAL(code, m) errors::error(errors::FATAL, uint32_t(code), std::string(m) + " in file " + __FILE__ " at line " + std::to_string(__LINE__))
//#define ERROR(code, m) errors::error(errors::ERR, uint32_t(code), std::string(m) + " in file " + __FILE__ " at line " + std::to_string(__LINE__))
//#define ERROR_DEBUG(code, message) errors::error(errors::ERR, code, std::string(message) + " in file " __FILE__ " at line " + std::to_string(__LINE__), errors::error::serverity::debug)

#ifndef NDEBUG
    #include <stdexcept>
    #define ERROR_TYPE void
    #define RAISE_ERROR_WARN(code, m) throw errors::error(errors::WARN, code, std::string(m) + " in file " + __FILE__ " at line " + std::to_string(__LINE__))
    #define RAISE_ERROR_FATAL(code, m) throw errors::error(errors::FATAL, code, std::string(m) + " in file " + __FILE__ " at line " + std::to_string(__LINE__))
    #define RAISE_ERROR_OK() return;

    #define HANDLE_ERROR(code)                                                           \
        try {                                                                            \
            code;                                                                        \
        } catch (const errors::error& e) {                                               \
            switch (e.err_state) {                                                       \
                case errors::state::FATAL:                                               \
                    LOG_ERROR(e.what());                                                 \
                    break;                                                               \
                case errors::state::WARN:                                                \
                    LOG_WARN(e.what());                                                  \
                    break;                                                               \
                case errors::state::OK:                                                  \
                    break;                                                               \
            }                                                                            \
        } catch (const std::exception& e) {                                              \
            LOG_ERROR(e.what());                                                         \
        } catch (...) {                                                                  \
            LOG_ERROR("Unknown Error. Occured in file " __FILE__ " at line ", __LINE__); \
        }

#define PASS_ERROR(code) code;

#else
    #define ERROR_TYPE errors::error
    #define RAISE_ERROR_WARN(code, m) return errors::error(errors::WARN, code, std::string(m) + " in file " + __FILE__ " at line " + std::to_string(__LINE__))
    #define RAISE_ERROR_FATAL(code, m) return errors::error(errors::FATAL, code, std::string(m) + " in file " + __FILE__ " at line " + std::to_string(__LINE__))
    #define RAISE_ERROR_OK() return errors::OK
    #define HANDLE_ERROR(code) errors::handle_error(code)
    #define PASS_ERROR(code)  \
        if(const auto err = (code); err != errors::OK) { \
        return err;                      \
        }
#endif

namespace errors
{
    enum state
    {
        OK,
        WARN,
        FATAL
    };

#ifndef NDEBUG
    class error : public std::exception
    {
    public:
        explicit error(state err_state, int32_t code = 0, const std::string& msg = "");
        const char* what() const _GLIBCXX_TXN_SAFE_DYN _GLIBCXX_NOTHROW override;

        state err_state{OK};
        uint32_t code{0};

    private:
        std::string m_message;
    };

#else
    struct error
    {
        error() = default;
        ~error() = default;
        error(state err_state, int32_t code = 0, const std::string& msg = "");

        operator state() const
        {
            return err_state;
        }

        state err_state{OK};
        uint32_t code{0};
        std::string message;
    };

    void handle_error(errors::error);
#endif
} // namespace errors
