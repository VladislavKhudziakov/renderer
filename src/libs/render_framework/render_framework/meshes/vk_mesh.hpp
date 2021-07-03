#pragma once

#include <render_framework/meshes/mesh.hpp>
#include <vk_utils/handlers.hpp>


namespace render_framework
{
    namespace detail
    {
        class vk_mesh_impl : public mesh_impl
        {
        public:
            vk_mesh_impl(
                const vertex_format& vertex_format,
                VkIndexType index_format,
                VkVertexInputBindingDescription input_binding,
                const std::vector<VkVertexInputAttributeDescription>& input_attrs,
                vk_utils::vma_buffer_handler vertex_buffer,
                vk_utils::vma_buffer_handler index_buffer);

            ~vk_mesh_impl() override = default;

            const vertex_format& get_format() const override;

            VkBuffer get_vertex_buffer() const;
            VkBuffer get_index_buffer() const;
            VkIndexType get_index_format() const;

            const VkVertexInputBindingDescription* get_input_bindings() const;
            const VkVertexInputAttributeDescription* get_input_attrs(uint32_t& attributes_count) const;
        private:
            vertex_format m_vertex_format{};
            VkIndexType m_index_format{};

            VkVertexInputBindingDescription m_input_binding_description{};
            std::vector<VkVertexInputAttributeDescription> m_vert_input_descriptions{};
            vk_utils::vma_buffer_handler m_vertex_buffer{};
            vk_utils::vma_buffer_handler m_index_buffer{};
        };
    }


    class vk_mesh_builder : public mesh_builder
    {
    public:
        vk_mesh_builder(VkCommandBuffer, uint32_t queue_family_index);

        vk_mesh_builder& set_command_buffer(VkCommandBuffer);
        vk_mesh_builder& set_queue_family_index(uint32_t);

        virtual ~vk_mesh_builder() = default;
        ERROR_TYPE create(mesh& mesh) override;

    protected:
        void clear() override;

    private:
        ERROR_TYPE create_vertex_inputs();
        ERROR_TYPE create_mesh_buffers();
        ERROR_TYPE write_buffers_data();
        ERROR_TYPE load_staging_buffer_data(
            vk_utils::vma_buffer_handler& buffer,
            size_t data_size,
            void* data_to_copy);

        bool m_force_reset_staging_buffers{false};
        VkCommandBuffer m_command_buffer{nullptr};
        uint32_t m_queue_family{};

        VkVertexInputBindingDescription m_input_binding_description{};
        std::vector<VkVertexInputAttributeDescription> m_vert_input_descriptions{};
        uint32_t m_vertex_format_size{0};

        vk_utils::vma_buffer_handler m_vertex_buffer{};
        vk_utils::vma_buffer_handler m_index_buffer{};

        vk_utils::vma_buffer_handler m_vertex_staging_buffer{};
        vk_utils::vma_buffer_handler m_index_staging_buffer{};
    };
}

