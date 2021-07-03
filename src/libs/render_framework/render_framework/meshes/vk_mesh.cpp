

#include "vk_mesh.hpp"

#include <vk_utils/tools.hpp>
#include <vk_utils/context.hpp>

#include <cstring>

using namespace render_framework;
using namespace detail;

namespace
{
    ERROR_TYPE get_vertex_element_data(
        vertex_format::attribute_type attr_type,
        size_t elements_count,
        VkFormat& out_format,
        size_t& out_size)
    {
        switch (attr_type) {
            case vertex_format::attribute_type::float32:
                switch (elements_count) {
                    case 1:
                        out_format = VK_FORMAT_R32_SFLOAT;
                        out_size = sizeof(float) * elements_count;
                        break;
                    case 2:
                        out_format = VK_FORMAT_R32G32_SFLOAT;
                        out_size = sizeof(float) * elements_count;
                        break;
                    case 3:
                        out_format = VK_FORMAT_R32G32B32_SFLOAT;
                        out_size = sizeof(float) * elements_count;
                        break;
                    case 4:
                        out_format = VK_FORMAT_R32G32B32A32_SFLOAT;
                        out_size = sizeof(float) * elements_count;
                        break;
                    default:
                        RAISE_ERROR_WARN(-1, "bad elements count");
                }
                break;
            case vertex_format::attribute_type::int32:
                switch (elements_count) {
                    case 1:
                        out_format = VK_FORMAT_R32_SINT;
                        out_size = sizeof(int32_t) * elements_count;
                        break;
                    case 2:
                        out_format = VK_FORMAT_R32G32_SINT;
                        out_size = sizeof(int32_t) * elements_count;
                        break;
                    case 3:
                        out_format = VK_FORMAT_R32G32B32_SINT;
                        out_size = sizeof(int32_t) * elements_count;
                        break;
                    case 4:
                        out_format = VK_FORMAT_R32G32B32A32_SINT;
                        out_size = sizeof(int32_t) * elements_count;
                        break;
                    default:
                        RAISE_ERROR_WARN(-1, "bad elements count");
                }
                break;
            case vertex_format::attribute_type::int16:
                switch (elements_count) {
                    case 1:
                        out_format = VK_FORMAT_R16_SINT;
                        out_size = sizeof(int16_t) * elements_count;
                        break;
                    case 2:
                        out_format = VK_FORMAT_R16G16_SINT;
                        out_size = sizeof(int16_t) * elements_count;
                        break;
                    case 3:
                        out_format = VK_FORMAT_R16G16B16_SINT;
                        out_size = sizeof(int16_t) * elements_count;
                        break;
                    case 4:
                        out_format = VK_FORMAT_R16G16B16A16_SINT;
                        out_size = sizeof(int16_t) * elements_count;
                        break;
                    default:
                        RAISE_ERROR_WARN(-1, "bad elements count");
                }
                break;
            case vertex_format::attribute_type::int8:
                switch (elements_count) {
                    case 1:
                        out_format = VK_FORMAT_R8_SINT;
                        out_size = sizeof(int8_t) * elements_count;
                        break;
                    case 2:
                        out_format = VK_FORMAT_R8G8_SINT;
                        out_size = sizeof(int8_t) * elements_count;
                        break;
                    case 3:
                        out_format = VK_FORMAT_R8G8B8_SINT;
                        out_size = sizeof(int8_t) * elements_count;
                        break;
                    case 4:
                        out_format = VK_FORMAT_R8G8B8A8_SINT;
                        out_size = sizeof(int8_t) * elements_count;
                        break;
                    default:
                        RAISE_ERROR_WARN(-1, "bad elements count.");
                }
                break;
            default:
                RAISE_ERROR_WARN(-1, "bad vertex format.");
        }
        RAISE_ERROR_OK();
    }


    ERROR_TYPE get_index_type(mesh_builder::index_type in_index_type, VkIndexType& out_index_type)
    {
        switch (in_index_type) {
            case mesh_builder::index_type::int32:
                out_index_type = VK_INDEX_TYPE_UINT32;
                break;
            case mesh_builder::index_type::int16:
                out_index_type = VK_INDEX_TYPE_UINT16;
                break;
            case mesh_builder::index_type::int8:
                out_index_type = VK_INDEX_TYPE_UINT8_EXT;
                break;
            default:
                RAISE_ERROR_WARN(-1, "bad index type.");
        }

        RAISE_ERROR_OK();
    }
}


vk_mesh_impl::vk_mesh_impl(
    const vertex_format& vertex_format,
    VkIndexType index_format,
    VkVertexInputBindingDescription input_binding,
    const std::vector<VkVertexInputAttributeDescription>& input_attrs,
    vk_utils::vma_buffer_handler vertex_buffer,
    vk_utils::vma_buffer_handler index_buffer)
    : m_vertex_format(vertex_format)
    , m_index_format(index_format)
    , m_input_binding_description(input_binding)
    , m_vert_input_descriptions(input_attrs)
    , m_vertex_buffer(std::move(vertex_buffer))
    , m_index_buffer(std::move(index_buffer))
{
}


const render_framework::vertex_format& vk_mesh_impl::get_format() const
{
    return m_vertex_format;
}


VkBuffer vk_mesh_impl::get_vertex_buffer() const
{
    return m_vertex_buffer;
}


VkBuffer vk_mesh_impl::get_index_buffer() const
{
    return m_index_buffer;
}


VkIndexType vk_mesh_impl::get_index_format() const
{
    return m_index_format;
}


const VkVertexInputBindingDescription* vk_mesh_impl::get_input_bindings() const
{
    return &m_input_binding_description;
}


const VkVertexInputAttributeDescription* vk_mesh_impl::get_input_attrs(uint32_t& attributes_count) const
{
    attributes_count = m_vert_input_descriptions.size();
    return m_vert_input_descriptions.data();
}


vk_mesh_builder::vk_mesh_builder(VkCommandBuffer cmd_buffer, uint32_t queue_family_index)
    : m_command_buffer(cmd_buffer)
    , m_queue_family(queue_family_index)
{
}


ERROR_TYPE vk_mesh_builder::create(mesh& mesh)
{
    std::unique_ptr<void, std::function<void(void*)>> clear_guard{nullptr, [this](void*) {clear();}};

    PASS_ERROR(create_vertex_inputs());
    PASS_ERROR(create_mesh_buffers());
    PASS_ERROR(write_buffers_data());
    VkIndexType index_type{};
    PASS_ERROR(get_index_type(m_index_format, index_type));

    mesh = mesh::create<vk_mesh_impl>(
        *m_vertex_format,
        index_type,
        m_input_binding_description,
        m_vert_input_descriptions,
        std::move(m_vertex_buffer),
        std::move(m_index_buffer));

    RAISE_ERROR_OK();
}


ERROR_TYPE vk_mesh_builder::create_vertex_inputs()
{
    if (!m_vertex_format.has_value()) {
        RAISE_ERROR_WARN(-1, "vertex format wasn't settled.");
    }

    const auto& vertex_attrs = m_vertex_format->get_attributes();
    m_vert_input_descriptions.reserve(vertex_attrs.size());

    for (uint32_t i = 0; i < vertex_attrs.size(); ++i) {
        const auto& curr_attr = vertex_attrs[i];
        VkFormat attr_format{};
        size_t attr_size{};

        PASS_ERROR(get_vertex_element_data(curr_attr.type, curr_attr.elements_count, attr_format, attr_size));

        m_vert_input_descriptions.emplace_back() = {
            .location = i,
            .binding = 0,
            .format = attr_format,
            .offset = m_vertex_format_size
        };

        m_vertex_format_size += attr_size;
    }

    m_input_binding_description = {
        .binding = 0,
        .stride = m_vertex_format_size,
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
    };
}


ERROR_TYPE vk_mesh_builder::create_mesh_buffers()
{
    if (m_vertex_data.get() == nullptr) {
        RAISE_ERROR_WARN(-1, "vertex data wasn't settled.");
    }

    PASS_ERROR(load_staging_buffer_data(m_vertex_staging_buffer, m_vertex_data.get_size(), m_vertex_data.get()));

    PASS_ERROR(vk_utils::create_buffer(
        m_vertex_buffer,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY,
        m_vertex_data.get_size(),
        nullptr,
        m_queue_family));

    if (m_index_data.get() == nullptr) {
        RAISE_ERROR_OK();
    }

    PASS_ERROR(load_staging_buffer_data(m_index_staging_buffer, m_index_data.get_size(), m_index_data.get()));

    PASS_ERROR(vk_utils::create_buffer(
        m_index_buffer,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY,
        m_index_data.get_size(),
        nullptr,
        m_queue_family));

    m_force_reset_staging_buffers = false;

    RAISE_ERROR_OK();
}


void vk_mesh_builder::clear()
{
    m_vertex_format_size = 0;

    m_vert_input_descriptions.clear();

    m_vertex_buffer = std::move(vk_utils::vma_buffer_handler{});
    m_index_buffer = std::move(vk_utils::vma_buffer_handler{});

    mesh_builder::clear();
}


ERROR_TYPE vk_mesh_builder::load_staging_buffer_data(
    vk_utils::vma_buffer_handler& buffer,
    size_t data_size,
    void* data_to_copy)
{
    if (static_cast<VkBuffer>(buffer) != nullptr &&
        buffer.get_alloc_info().size >= data_size &&
       !m_force_reset_staging_buffers) {
        void* mapped_data{nullptr};
        vmaMapMemory(vk_utils::context::get().allocator(), buffer, &mapped_data);
        std::memcpy(mapped_data, data_to_copy, data_size);
        vmaFlushAllocation(vk_utils::context::get().allocator(), buffer, 0, data_size);
        return;
    }

    vk_utils::vma_buffer_handler new_buffer;

    PASS_ERROR(vk_utils::create_buffer(
        new_buffer,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VMA_MEMORY_USAGE_CPU_TO_GPU,
        data_size,
        data_to_copy,
        m_queue_family));

    buffer = std::move(new_buffer);

    RAISE_ERROR_OK();
}


ERROR_TYPE vk_mesh_builder::write_buffers_data()
{
    VkBufferCopy buffer_copy{
        .srcOffset = 0,
        .dstOffset = 0,
        .size = m_vertex_data.get_size()
    };

    vkCmdCopyBuffer(m_command_buffer, m_vertex_staging_buffer, m_vertex_buffer, 1, &buffer_copy);

    if (m_index_data.get() != nullptr) {
        buffer_copy.size = m_index_data.get_size();
        vkCmdCopyBuffer(m_command_buffer, m_index_staging_buffer, m_index_buffer, 1, &buffer_copy);
    }

    RAISE_ERROR_OK();
}


vk_mesh_builder& vk_mesh_builder::set_command_buffer(VkCommandBuffer command_buffer)
{
    m_command_buffer = command_buffer;
    return *this;
}


vk_mesh_builder& vk_mesh_builder::set_queue_family_index(uint32_t queue_family_index)
{
    m_force_reset_staging_buffers = m_queue_family == queue_family_index;
    m_queue_family = queue_family_index;
    return *this;
}
