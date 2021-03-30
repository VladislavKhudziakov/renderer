

#include "error_handler.hpp"

#include <logger/log.hpp>
#include <cassert>

#ifndef NDEBUG
    #define ASSERT_ERR(exception)                                                              \
        LOG_ERROR((exception).message + " occured. code " + std::to_string((exception).code)); \
        assert(false)
#else
    #define ASSERT_ERR(error) LOG_ERROR((exception).message + " occured. code " + std::to_string((exception).code))
#endif

#define ASSERT_FATAL(exception)                                                            \
    LOG_ERROR((exception).message + " occured. code " + std::to_string((exception).code)); \
    assert(false)

void errors::handle_error(errors::error e)
{
    if (e.err_state == OK) {
        return;
    }

    if (e.err_state == WARN) {
        if (!e.message.empty()) {
            switch (e.err_serverity) {
                case ALL:
                    LOG_WARN(e.message);
                    break;
                case DEBUG:
                 #ifndef NDEBUG
                    LOG_DEBUG(e.message);
                #endif
                    break;
                case RELEASE:
                #ifdef NDEBUG
                    LOG_WARN(e.message);
                #endif
                    break;
            }
        }
        return;
    }

    if (e.err_state == ERR) {
        ASSERT_ERR(e);
        return;
    }

    if (e.err_state == FATAL) {
       ASSERT_FATAL(e);
       return;
    }
}


errors::error::error(errors::state state, int32_t code, const std::string& msg, errors::serverity s)
    : err_state(state)
    , code(state == OK ? 0 : (code == 0 ? -1 : code))
    , err_serverity(s)
    , message(msg)
{
}
