#pragma once

#include <render_framework/asset.hpp>
#include <errors/error_handler.hpp>
#include <utils/data.hpp>

namespace render_framework
{
    class material;
    class vertex_format;

    namespace detail
    {
        class mesh_impl
        {
        public:
            virtual ~mesh_impl() = default;
            virtual const vertex_format& get_format() const = 0;
        };
    }


    class vertex_format
    {
    public:
        enum class attribute_type {
            float32, int32, int16, int8
        };

        struct vertex_attribute {
            attribute_type type;
            size_t elements_count;
        };

        void add_attribute(vertex_attribute);
        const std::vector<vertex_attribute>& get_attributes() const;

    private:
        std::vector<vertex_attribute> m_attributes;
    };


    class mesh : public asset<mesh, detail::mesh_impl>
    {
    public:
        const vertex_format& get_format() const;
    };


    class mesh_builder
    {
    public:
        enum class index_type {
            int32, int16, int8
        };

        virtual ~mesh_builder() = default;
        mesh_builder& set_format(const vertex_format&);
        mesh_builder& set_index_format(index_type);
        mesh_builder& set_vertex_data(utils::data);
        mesh_builder& set_index_data(utils::data);

        virtual ERROR_TYPE create(mesh&) = 0;

    protected:
        virtual void clear();

        utils::data m_vertex_data{};
        utils::data m_index_data{};

        std::optional<vertex_format> m_vertex_format{};
        index_type m_index_format{};
    };
}

