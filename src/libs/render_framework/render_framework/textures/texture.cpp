#include "texture.hpp"


using namespace render_framework;


texture::texture() = default;


texture::texture(texture&& other) noexcept = default;


texture& texture::operator=(texture&& other) noexcept = default;


texture::~texture() = default;


uint64_t texture::get_view() const
{
    return m_impl->get_view();
}


uint64_t texture::get_sampler() const
{
    return m_impl->get_sampler();
}


texture_builder* texture_builder::set_filtering(texture_filtering filtering)
{
    m_filtering = filtering;
    return this;
}


texture_builder* texture_builder::set_address_mode(texture_address_mode address_mode)
{
    m_address_mode = address_mode;
    return this;
}
