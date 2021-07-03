#pragma once

#include <render_framework/materials/material.hpp>
#include <render_framework/paramters/vk_parameter.hpp>

#include <vk_utils/handlers.hpp>

namespace render_framework
{
    namespace detail
    {
        class vk_material_impl : public material_impl, public vk_parameters_list_visitor
        {
            friend class vk_material_builder;
        public:
            vk_material_impl(
                std::vector<vk_utils::shader_module_handler>,
                VkShaderStageFlags,
                std::vector<vk_utils::descriptor_set_layout_handler>,
                vk_utils::pipeline_layout_handler,
                vk_utils::descriptor_pool_handler,
                vk_utils::descriptor_set_handler,
                std::vector<const parameters_list*> parameters_lists);

            void apply() override;
            void apply(VkCommandBuffer);
            void set_texture(size_t slot, const texture& texture) override;

            void accept(vk_uniform_parameters_list_impl* impl) override;
            void accept(const vk_uniform_parameters_list_impl* impl) const override;
            void accept(vk_push_constant_parameters_list_impl* impl) override;
            void accept(const vk_push_constant_parameters_list_impl* impl) const override;

        private:
            void update_descriptor_sets();

            std::unordered_map<size_t, const texture*> m_textures_write_cache;

            VkCommandBuffer m_curr_apply_command_buffer{nullptr};
            VkShaderStageFlags m_stages;
            std::vector<const parameters_list*> m_parameters_lists;
            std::vector<vk_utils::shader_module_handler> m_modules;
            std::vector<vk_utils::descriptor_set_layout_handler> m_descriptor_set_layouts;
            vk_utils::pipeline_layout_handler m_pipeline_layout;
            vk_utils::descriptor_pool_handler m_descriptor_pool;
            vk_utils::descriptor_set_handler m_descriptor_set;
        };
    }


    class vk_material_builder : public material_builder, public detail::vk_parameters_list_visitor
    {
    public:
        ERROR_TYPE create(material& material) override;
        void accept(detail::vk_uniform_parameters_list_impl* impl) override;
        void accept(const detail::vk_uniform_parameters_list_impl* impl) const override;
        void accept(detail::vk_push_constant_parameters_list_impl* impl) override;
        void accept(const detail::vk_push_constant_parameters_list_impl* impl) const override;

    protected:
        void clear() override;
        ERROR_TYPE create_shader_modules();
        ERROR_TYPE create_textures_data();
        ERROR_TYPE create_buffers_data();
        ERROR_TYPE create_descriptor_set_layouts();
        ERROR_TYPE create_descriptor_pool();
        ERROR_TYPE allocate_descriptor_sets();
        ERROR_TYPE write_descriptors_into_sets();
        ERROR_TYPE create_pipeline_layout();

        void add_buffer_descriptor_data(const detail::vk_uniform_parameters_list_impl*) const;
        void add_push_constant_range (const detail::vk_push_constant_parameters_list_impl*) const;

        VkShaderStageFlags m_vk_stages_flags = 0;

        vk_utils::descriptor_pool_handler m_descriptor_pool;
        vk_utils::descriptor_set_handler m_descriptor_set;
        vk_utils::pipeline_layout_handler m_pipeline_layout;

        std::vector<vk_utils::shader_module_handler> m_modules;
        std::vector<VkDescriptorImageInfo> m_descriptor_image_infos;
        std::vector<vk_utils::descriptor_set_layout_handler> m_descriptor_set_layouts;
        mutable std::vector<VkPushConstantRange> m_push_constants_ranges;
        mutable std::vector<VkDescriptorSetLayoutBinding> m_descriptor_sets_layout_bindings;
        mutable std::vector<VkDescriptorBufferInfo> m_descriptor_buffer_infos;
        mutable std::vector<VkDescriptorPoolSize> m_pool_sizes;
    };
}

