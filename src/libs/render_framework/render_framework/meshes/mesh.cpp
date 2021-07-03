

#include "mesh.hpp"

using namespace render_framework;


void vertex_format::add_attribute(vertex_format::vertex_attribute attr)
{
    m_attributes.push_back(attr);
}


const std::vector<vertex_format::vertex_attribute>& vertex_format::get_attributes() const
{
    return m_attributes;
}


const vertex_format& mesh::get_format() const
{
    return m_impl->get_format();
}


mesh_builder& mesh_builder::set_vertex_data(utils::data data)
{
    m_vertex_data = std::move(data);
    return *this;
}


mesh_builder& mesh_builder::set_index_data(utils::data data)
{
    m_index_data = std::move(data);
    return *this;
}


void mesh_builder::clear()
{
    m_vertex_data = std::move(utils::data{});
    m_index_data = std::move(utils::data{});
    m_vertex_format.reset();
}


mesh_builder& mesh_builder::set_format(const vertex_format& format)
{
    m_vertex_format.emplace(format);
    return *this;
}


mesh_builder& mesh_builder::set_index_format(mesh_builder::index_type index_type)
{
    m_index_format = index_type;
    return *this;
}
