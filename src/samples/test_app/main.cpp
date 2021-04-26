
#include "lib.hpp"


int main(int argc, const char** argv)
{
    dummy_obj_viewer_app app{"obj_viewer", argc, argv};
    HANDLE_ERROR(app.run());

    return 0;
}