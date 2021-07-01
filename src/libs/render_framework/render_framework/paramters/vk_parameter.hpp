#pragma once

#include <render_framework/paramters/parameter.hpp>

#include <vk_utils/handlers.hpp>

#include <unordered_map>

namespace render_framework
{
    class vk_parameters_list_flush_data_source
    {
    public:
        virtual ~vk_parameters_list_flush_data_source() = default;
        virtual VkCommandBuffer get_command_buffer() const = 0;
    };


    class vk_push_constant_parameters_list_flush_data_source : public vk_parameters_list_flush_data_source
    {
    public:
        virtual ~vk_push_constant_parameters_list_flush_data_source() = default;
        virtual VkPipelineLayout get_pipeline_layout() const = 0;
        virtual VkShaderStageFlags get_shader_stages() const = 0;
    };


    class vk_parameters_list_impl : public parameters_list_impl
    {
        friend class vk_parameters_list_builder;
        struct parameter_data;

    public:
        vk_parameters_list_impl(
            std::vector<parameter_data>& parameters,
            std::unordered_map<const parameter*, size_t>& parameters_map,
            size_t buffer_size);

        ~vk_parameters_list_impl() override = default;
        
        virtual void on_parameter_updated(const parameter* parameter, const uint8_t* value) override;

        virtual void flush(const vk_parameters_list_flush_data_source*) = 0;

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
        vk_uniform_parameters_list_impl(
            std::vector<parameter_data>& parameters,
            std::unordered_map<const parameter*, size_t>& parameters_map,
            size_t buffer_size,
            vk_utils::vma_buffer_handler staging_buffer,
            vk_utils::vma_buffer_handler uniform_buffer);

        ~vk_uniform_parameters_list_impl() override = default;

        virtual void on_parameter_updated(const parameter* parameter, const uint8_t* value) override;

        virtual void flush(const vk_parameters_list_flush_data_source*) override;

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
        virtual void flush(const vk_parameters_list_flush_data_source*) override;
    };
    

    class vk_parameters_list_builder : public parameters_list_builder
    {
    protected:
        struct parameters_list_args
        {
            std::unordered_map<const parameter*, size_t> parameters_map;
            std::vector<vk_parameters_list_impl::parameter_data> params_data;
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