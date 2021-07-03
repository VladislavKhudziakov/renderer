#include <render_framework/paramters/vk_parameter.hpp>

#include <vk_utils/context.hpp>
#include <vk_utils/tools.hpp>

#include <cstring>

using namespace render_framework;

namespace
{
    constexpr size_t buffer_alignment = sizeof(float) * 4;

    ERROR_TYPE get_value_size(render_framework::parameter::value_type value_type, size_t& size)
    {
        switch (value_type) {
            case render_framework::parameter::value_type::int32:
                size = sizeof(int32_t);
                break;
            case render_framework::parameter::value_type::float32:
                size = sizeof(float);
                break;
            case render_framework::parameter::value_type::vec4:
                size = sizeof(float) * 4;
                break;
            case render_framework::parameter::value_type::mat4:
                size = sizeof(float) * 4 * 4;
                break;
            default:
                RAISE_ERROR_WARN(-1, "bad value type.");
        }

        RAISE_ERROR_OK();
    }

    size_t align(size_t size, size_t alignment)
    {
        auto padding = alignment - size & (alignment - 1);
        return size + padding;
    }
}


detail::vk_parameters_list_impl::vk_parameters_list_impl(
    std::vector<parameter_data>& parameters, std::unordered_map<const parameter*, size_t>& parameters_map, size_t buffer_size)
    : m_parameters_data(std::move(parameters))
    , m_parameters_map(std::move(parameters_map))
{
    m_data_buffer.reserve(buffer_size);
}


void detail::vk_parameters_list_impl::on_parameter_updated(const parameter* p, const uint8_t* value)
{
    const parameter_data& data_view = m_parameters_data[m_parameters_map[p]];
    std::memcpy(m_data_buffer.data() + data_view.data_offset, value, data_view.data_size);
}


detail::vk_uniform_parameters_list_impl::vk_uniform_parameters_list_impl(
    std::vector<parameter_data>& parameters,
    std::unordered_map<const parameter*, size_t>& parameters_map,
    size_t buffer_size,
    vk_utils::vma_buffer_handler staging_buffer,
    vk_utils::vma_buffer_handler uniform_buffer)
    : vk_parameters_list_impl(parameters, parameters_map, buffer_size)
    , m_staging_buffer(std::move(staging_buffer))
    , m_uniform_buffer(std::move(uniform_buffer))
{
}


void detail::vk_uniform_parameters_list_impl::on_parameter_updated(const parameter* parameter, const uint8_t* value)
{
    vk_parameters_list_impl::on_parameter_updated(parameter, value);
    m_dirty = true;
}


void detail::vk_uniform_parameters_list_impl::flush(VkCommandBuffer command_buffer)
{
    if (!m_dirty) {
        return;
    }

    void* mapped_memory{nullptr};

    vmaMapMemory(vk_utils::context::get().allocator(), m_staging_buffer, &mapped_memory);

    std::memcpy(mapped_memory, m_data_buffer.data(), m_data_buffer.size());
    
    vmaFlushAllocation(
        vk_utils::context::get().allocator(),
        m_staging_buffer,
        0,
        VK_WHOLE_SIZE);
    
    VkBufferCopy buffer_copy{
        .srcOffset = 0,
        .dstOffset = 0,
        .size = m_data_buffer.size()
    };

    vkCmdCopyBuffer(
        command_buffer,
        m_staging_buffer,
        m_uniform_buffer,
        1,
        &buffer_copy);

    m_dirty = false;
}


void detail::vk_uniform_parameters_list_impl::visit(detail::vk_parameters_list_visitor* visitor)
{
    visitor->accept(this);
}


void detail::vk_uniform_parameters_list_impl::visit(const detail::vk_parameters_list_visitor* visitor) const
{
    visitor->accept(this);
}


VkBuffer detail::vk_uniform_parameters_list_impl::get_buffer() const
{
    return m_uniform_buffer;
}


size_t detail::vk_uniform_parameters_list_impl::get_buffer_offset() const
{
    return 0;
}


size_t detail::vk_uniform_parameters_list_impl::get_buffer_size() const
{
    return m_data_buffer.size();
}


detail::vk_push_constant_parameters_list_impl::vk_push_constant_parameters_list_impl(
    std::vector<parameter_data>& parameters, 
    std::unordered_map<const parameter*, size_t>& parameters_map, 
    size_t buffer_size)
    : vk_parameters_list_impl(parameters, parameters_map, buffer_size)
{
}


void detail::vk_push_constant_parameters_list_impl::flush(
    VkCommandBuffer command_buffer,
    VkPipelineLayout pipeline_layout,
    VkShaderStageFlags shader_stages)
{
    vkCmdPushConstants(command_buffer, pipeline_layout, shader_stages, 0, m_data_buffer.size(), m_data_buffer.data());
}


void detail::vk_push_constant_parameters_list_impl::visit(detail::vk_parameters_list_visitor* visitor)
{
    visitor->accept(this);
}


void detail::vk_push_constant_parameters_list_impl::visit(const detail::vk_parameters_list_visitor* visitor) const
{
    visitor->accept(this);
}


size_t detail::vk_push_constant_parameters_list_impl::get_buffer_offset() const
{
    return 0;
}


size_t detail::vk_push_constant_parameters_list_impl::get_buffer_size() const
{
    return m_data_buffer.size();
}


ERROR_TYPE vk_uniform_parameters_list_builder::create(parameters_list& reslut)
{
    parameters_list_args args{};
    PASS_ERROR(get_paramters_list_args(args));

    vk_utils::vma_buffer_handler uniform_buffer;
    vk_utils::vma_buffer_handler uniform_staging_buffer;

    PASS_ERROR(vk_utils::create_buffer(
        uniform_buffer,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY,
        args.buffer_size,
        nullptr));

    PASS_ERROR(vk_utils::create_buffer(
        uniform_staging_buffer,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VMA_MEMORY_USAGE_CPU_TO_GPU,
        args.buffer_size,
        nullptr));

    reslut = parameters_list::create<detail::vk_uniform_parameters_list_impl>(
        args.params_data, args.parameters_map, args.buffer_size, std::move(uniform_staging_buffer), std::move(uniform_buffer));

    RAISE_ERROR_OK();
}


ERROR_TYPE vk_push_constant_parameters_list_builder::create(parameters_list& reslut)
{
    parameters_list_args args{};
    PASS_ERROR(get_paramters_list_args(args));

    reslut = parameters_list::create<detail::vk_push_constant_parameters_list_impl>(args.params_data, args.parameters_map, args.buffer_size);

    RAISE_ERROR_OK();
}

ERROR_TYPE vk_parameters_list_builder::get_paramters_list_args(parameters_list_args& args)
{
    args.params_data.reserve(m_parameters_list.size());

    for (const auto& param : m_parameters_list) {
        const auto el_count = param->get_elements_count();
        const auto value_type = param->get_value_type();

        size_t value_size{0};

        PASS_ERROR(get_value_size(value_type, value_size));

        detail::vk_uniform_parameters_list_impl::parameter_data curr_data{
            .data_offset = args.buffer_size,
            .data_size = value_size};

        args.parameters_map[param] = args.params_data.size();
        args.params_data.emplace_back() = curr_data;
        args.buffer_size += align(value_size, buffer_alignment);
    }

    RAISE_ERROR_OK();
}
