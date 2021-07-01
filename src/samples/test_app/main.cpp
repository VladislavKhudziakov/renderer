
#include "dummy_obj_viewer_app.hpp"


int main(int argc, const char** argv)
{
    dummy_obj_viewer_app app{"obj_viewer"};
    HANDLE_ERROR(app.run(argc, argv));

    return 0;
}