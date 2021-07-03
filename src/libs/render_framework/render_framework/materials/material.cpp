#include "material.hpp"


void render_framework::material::apply()
{
    m_impl->apply();
}


void render_framework::material::set_texture(size_t slot, const render_framework::texture& texture)
{
    m_impl->set_texture(slot, texture);
}


render_framework::material_builder& render_framework::material_builder::add_stage(
    render_framework::material_builder::shading_stage_type stage,
    utils::data code)
{
    m_stages.emplace(stage, std::move(code));
    return *this;
}


render_framework::material_builder& render_framework::material_builder::add_texture(const render_framework::texture& texture)
{
    m_textures.emplace_back(&texture);
    return *this;
}


render_framework::material_builder& render_framework::material_builder::add_parameters_list(const render_framework::parameters_list& plist)
{
    m_parameters.emplace_back(&plist);
    return *this;
}


void render_framework::material_builder::clear()
{
    m_stages.clear();
    m_textures.clear();
    m_parameters.clear();
}
