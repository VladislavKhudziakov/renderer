#include "test_push_constants_app.hpp"


int main(int argc, const char** argv)
{
    test_push_constants_app app{"test_push_constants", argc, argv};
    HANDLE_ERROR(app.run());

    return 0;
}