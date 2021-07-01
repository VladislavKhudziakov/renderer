#pragma once

#include <render_framework/asset.hpp>

#include <errors/error_handler.hpp>

namespace render_framework
{   
    class texture;
    class parameters_list;

    class material_impl
    {
    };

    class material : public asset<material, material_impl> 
    {
    public:
        
    private:
    };


    class material_builder
    {
    public:
        void add_shading_stage();
        void add_texture(const texture&);
        void add_parameters_list(const parameters_list&);

        virtual ERROR_TYPE create(material&) = 0;
    };
}