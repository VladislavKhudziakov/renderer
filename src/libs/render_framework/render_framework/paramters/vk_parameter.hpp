#pragma once

#include <render_framework/paramters/parameter.hpp>

#include <vk_utils/handlers.hpp>

#include <unordered_map>

namespace render_framework
{
    class vk_parameters_list_builder;

    namespace detail
    {
        class vk_parameters_list_visitor;


        class vk_parameters_list_flush_data_source
        {
        public:
            virtual ~vk_parameters_list_flush_data_source() = default;
            virtual VkCommandBuffer get_command_buffer() const = 0;
        };


        class vk_parameters_list_impl : public parameters_list_impl
        {
            friend class render_framework::vk_parameters_list_builder;

        protected:
            struct parameter_data;

        public:
            vk_parameters_list_impl(
                std::vector<parameter_data>& parameters,
                std::unordered_map<const parameter*, size_t>& parameters_map,
                size_t buffer_size);

            ~vk_parameters_list_impl() override = default;

            virtual void on_parameter_updated(const parameter* parameter, const uint8_t* value) override;

            virtual void visit(vk_parameters_list_visitor*) = 0;
            virtual void visit(const vk_parameters_list_visitor*) const = 0;

        protected:
            struct parameter_data
            {
                size_t data_offset;
                size_t data_size;
            };

            std::vector<uint8_t> m_data_buffer;
            std::vector<parameter_data> m_parameters_data;
            std::unordered_map<const parameter*, size_t> m_parameters_map;
        };


        class vk_uniform_parameters_list_impl : public vk_parameters_list_impl
        {
        public:
            void visit(vk_parameters_list_visitor* visitor) override;
            void visit(const vk_parameters_list_visitor* visitor) const override;

        public:
            vk_uniform_parameters_list_impl(
                std::vector<parameter_data>& parameters,
                std::unordered_map<const parameter*, size_t>& parameters_map,
                size_t buffer_size,
                vk_utils::vma_buffer_handler staging_buffer,
                vk_utils::vma_buffer_handler uniform_buffer);

            ~vk_uniform_parameters_list_impl() override = default;

            virtual void on_parameter_updated(const parameter* parameter, const uint8_t* value) override;

            VkBuffer get_buffer() const;
            size_t get_buffer_offset() const;
            size_t get_buffer_size() const;

            void flush(VkCommandBuffer);

        private:
            vk_utils::vma_buffer_handler m_staging_buffer;
            vk_utils::vma_buffer_handler m_uniform_buffer;

            bool m_dirty = true;
        };


        class vk_push_constant_parameters_list_impl : public vk_parameters_list_impl
        {
        public:
            vk_push_constant_parameters_list_impl(
                std::vector<parameter_data>& parameters,
                std::unordered_map<const parameter*, size_t>& parameters_map,
                size_t buffer_size);

            ~vk_push_constant_parameters_list_impl() override = default;

            void visit(vk_parameters_list_visitor* visitor) override;
            void visit(const vk_parameters_list_visitor* visitor) const override;

            size_t get_buffer_offset() const;
            size_t get_buffer_size() const;

            void flush(VkCommandBuffer command_buffer, VkPipelineLayout pipeline_layout, VkShaderStageFlags shader_stages);
        };


        class vk_parameters_list_visitor
        {
        public:
            virtual void accept(vk_uniform_parameters_list_impl*) = 0;
            virtual void accept(const vk_uniform_parameters_list_impl*) const = 0;
            virtual void accept(vk_push_constant_parameters_list_impl*) = 0;
            virtual void accept(const vk_push_constant_parameters_list_impl*) const = 0;
        };
    }


    class vk_parameters_list_builder : public parameters_list_builder
    {
    protected:
        struct parameters_list_args
        {
            std::unordered_map<const parameter*, size_t> parameters_map;
            std::vector<detail::vk_parameters_list_impl::parameter_data> params_data;
            size_t buffer_size{0};
        };

        ERROR_TYPE get_paramters_list_args(parameters_list_args&);
    };
     

    class vk_uniform_parameters_list_builder : public vk_parameters_list_builder
    {
    public:
        virtual ERROR_TYPE create(parameters_list&);
    };
     

    class vk_push_constant_parameters_list_builder : public vk_parameters_list_builder
    {
    public:
        virtual ERROR_TYPE create(parameters_list&);
    };
}