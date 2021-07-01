#include "test_ktx_app.hpp"


int main(int argc, const char** argv)
{
    test_ktx_app app{"test_ktx"};
    HANDLE_ERROR(app.run(argc, argv));

    return 0;
}