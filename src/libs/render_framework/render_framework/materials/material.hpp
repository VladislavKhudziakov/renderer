#pragma once

#include <render_framework/asset.hpp>
#include <errors/error_handler.hpp>
#include <utils/data.hpp>

#include <unordered_map>

namespace render_framework
{   
    class texture;
    class parameters_list;


    namespace detail
    {
        class material_impl
        {
        public:
            virtual void apply() = 0;
            virtual void set_texture(size_t slot, const texture& texture) = 0;
        };
    }


    class material : public asset<material, detail::material_impl>
    {
    public:
        void apply();
        void set_texture(size_t slot, const texture& texture);
    };


    class material_builder
    {
    public:
        enum class shading_stage_type
        {
            vertex, fragment
        };

        material_builder& add_stage(shading_stage_type, utils::data code);
        material_builder& add_texture(const texture&);
        material_builder& add_parameters_list(const parameters_list&);

        virtual ERROR_TYPE create(material&) = 0;

    protected:
        virtual void clear();

        std::unordered_map<shading_stage_type, utils::data> m_stages;
        std::vector<const texture*> m_textures;
        std::vector<const parameters_list*> m_parameters;
    };
}