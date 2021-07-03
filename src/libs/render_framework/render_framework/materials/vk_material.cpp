#include "vk_material.hpp"

#include <render_framework/textures/vk_texture.hpp>

#include <vk_utils/tools.hpp>
#include <vk_utils/context.hpp>


using namespace render_framework;
using namespace detail;

namespace
{
    ERROR_TYPE vk_stage_cast(material_builder::shading_stage_type stage, VkShaderStageFlagBits& vk_stage)
    {
        switch (stage) {
            case material_builder::shading_stage_type::vertex:
                vk_stage = VK_SHADER_STAGE_VERTEX_BIT;
                RAISE_ERROR_OK();
            case material_builder::shading_stage_type::fragment:
                vk_stage = VK_SHADER_STAGE_FRAGMENT_BIT;
                RAISE_ERROR_OK();
            default:
                RAISE_ERROR_WARN(-1, "bad stage.");
        }
    }
}


vk_material_impl::vk_material_impl(
    std::vector<vk_utils::shader_module_handler> modules,
    VkShaderStageFlags stages,
    std::vector<vk_utils::descriptor_set_layout_handler> desc_set_layouts,
    vk_utils::pipeline_layout_handler pipeline_layout,
    vk_utils::descriptor_pool_handler descriptor_pool,
    vk_utils::descriptor_set_handler descriptor_set,
    std::vector<const parameters_list*> parameters_lists)
    : m_stages(stages)
    , m_modules(std::move(modules))
    , m_descriptor_set_layouts(std::move(desc_set_layouts))
    , m_pipeline_layout(std::move(pipeline_layout))
    , m_descriptor_pool(std::move(descriptor_pool))
    , m_descriptor_set(std::move(descriptor_set))
    , m_parameters_lists(std::move(parameters_lists))
{
}

void vk_material_impl::apply()
{
}


void vk_material_impl::set_texture(size_t slot, const texture& texture)
{
    m_textures_write_cache[slot] = &texture;
}


void vk_material_impl::apply(VkCommandBuffer cmd_buffer)
{
    update_descriptor_sets();

    m_curr_apply_command_buffer = cmd_buffer;

    for (const auto* parameters_list : m_parameters_lists) {
        const auto* plist_impl = static_cast<const vk_parameters_list_impl*>(parameters_list->get_impl());
        plist_impl->visit(this);
    }

    m_curr_apply_command_buffer = nullptr;
}


void vk_material_impl::accept(vk_uniform_parameters_list_impl* impl)
{
    impl->flush(m_curr_apply_command_buffer);
}


void vk_material_impl::accept(const vk_uniform_parameters_list_impl* impl) const
{
    const_cast<vk_uniform_parameters_list_impl*>(impl)->flush(m_curr_apply_command_buffer);
}


void vk_material_impl::accept(vk_push_constant_parameters_list_impl* impl)
{
    impl->flush(m_curr_apply_command_buffer, m_pipeline_layout, m_stages);
}


void vk_material_impl::accept(const vk_push_constant_parameters_list_impl* impl) const
{
    const_cast<vk_push_constant_parameters_list_impl*>(impl)->flush(m_curr_apply_command_buffer, m_pipeline_layout, m_stages);
}


void vk_material_impl::update_descriptor_sets()
{
    if (m_textures_write_cache.empty()) {
        return;
    }

    std::vector<VkWriteDescriptorSet> texture_write_ops{};
    std::vector<VkDescriptorImageInfo> write_image_infos{};

    texture_write_ops.reserve(m_textures_write_cache.size());
    write_image_infos.reserve(m_textures_write_cache.size());

    for (const auto [binding, texture] : m_textures_write_cache) {
        const auto* texture_impl = static_cast<const vk_texture_impl*>(texture->get_impl());

        write_image_infos.emplace_back() = {
            .sampler = texture_impl->get_sampler(),
            .imageView = texture_impl->get_image_view(),
        };

        texture_write_ops.emplace_back() = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext = nullptr,
            .dstSet = m_descriptor_set.handlers_count() == 1 ? m_descriptor_set[0] : m_descriptor_set[1],
            .dstBinding = static_cast<uint32_t>(binding),
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .pImageInfo = &write_image_infos.back(),
        };
    }

    vkUpdateDescriptorSets(
        vk_utils::context::get().device(),
        texture_write_ops.size(),
        texture_write_ops.data(),
        0,
        nullptr);
}


void vk_material_builder::create(material& material)
{
    std::unique_ptr<void, std::function<void(void*)>> clear_guard{nullptr, [this](void*) {clear();}};

    if (m_stages.empty()) {
        RAISE_ERROR_WARN(-1, "cannot find any shader stage.");
    }

    PASS_ERROR(create_shader_modules());
    PASS_ERROR(create_buffers_data());
    PASS_ERROR(create_textures_data());
    PASS_ERROR(create_descriptor_set_layouts());
    PASS_ERROR(create_descriptor_pool());
    PASS_ERROR(allocate_descriptor_sets());
    PASS_ERROR(write_descriptors_into_sets());

    material = material::create<vk_material_impl>(
        std::move(m_modules),
        m_vk_stages_flags,
        std::move(m_descriptor_set_layouts),
        std::move(m_pipeline_layout),
        std::move(m_descriptor_pool),
        std::move(m_descriptor_set),
        std::move(m_parameters));

    RAISE_ERROR_OK();
}


void vk_material_builder::clear()
{
    m_vk_stages_flags = 0;

    m_descriptor_pool.destroy();
    m_descriptor_set.destroy();
    m_pipeline_layout.destroy();

    m_modules.clear();
    m_descriptor_image_infos.clear();
    m_descriptor_set_layouts.clear();
    m_push_constants_ranges.clear();
    m_descriptor_sets_layout_bindings.clear();
    m_descriptor_buffer_infos.clear();
    m_pool_sizes.clear();

    material_builder::clear();
}


void vk_material_builder::accept(vk_uniform_parameters_list_impl* params_list)
{
    add_buffer_descriptor_data(params_list);
}


void vk_material_builder::accept(const vk_uniform_parameters_list_impl* params_list) const
{
    add_buffer_descriptor_data(params_list);
}


void vk_material_builder::accept(vk_push_constant_parameters_list_impl* params_list)
{
    add_push_constant_range(params_list);
}


void vk_material_builder::accept(const vk_push_constant_parameters_list_impl* params_list) const
{
    add_push_constant_range(params_list);
}


void vk_material_builder::add_buffer_descriptor_data(
    const vk_uniform_parameters_list_impl* params_list) const
{
    if (m_pool_sizes.size() <= 1) {
        m_pool_sizes.emplace_back() = {
            .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        };

        m_descriptor_sets_layout_bindings.emplace_back() = {
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 0,
            .stageFlags = m_vk_stages_flags
        };
    }

    m_descriptor_buffer_infos.emplace_back() = {
        .buffer = params_list->get_buffer(),
        .offset = params_list->get_buffer_offset(),
        .range = params_list->get_buffer_size()
    };
}

void vk_material_builder::add_push_constant_range(
    const vk_push_constant_parameters_list_impl* params_list) const
{
    m_push_constants_ranges.emplace_back() = {
        .stageFlags = m_vk_stages_flags,
        .offset = static_cast<uint32_t>(params_list->get_buffer_offset()),
        .size = static_cast<uint32_t>(params_list->get_buffer_size())
    };
}


ERROR_TYPE vk_material_builder::create_shader_modules()
{
    m_modules.reserve(m_stages.size());

    VkShaderStageFlags stages{0};

    for (const auto& [stage, module] : m_stages) {
        VkShaderStageFlagBits vk_stage;
        PASS_ERROR(vk_stage_cast(stage, vk_stage));
        PASS_ERROR(vk_utils::create_shader_module(
            reinterpret_cast<const uint32_t*>(module.get()), module.get_size(), m_modules.emplace_back()));
        stages |= vk_stage;
    }

    m_vk_stages_flags = stages;

    RAISE_ERROR_OK();
}


ERROR_TYPE vk_material_builder::create_textures_data()
{
    m_descriptor_image_infos.reserve(m_textures.size());

    if (!m_textures.empty()) {
        m_pool_sizes.emplace_back() = {
            .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = 0
        };

        m_descriptor_sets_layout_bindings.emplace_back() = {
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = 0,
            .stageFlags = m_vk_stages_flags
        };
    }

    for (const auto* texture : m_textures) {
        m_pool_sizes.back().descriptorCount++;
        m_descriptor_sets_layout_bindings.back().descriptorCount++;

        const auto* texture_impl = static_cast<const vk_texture_impl*>(texture->get_impl());

        m_descriptor_image_infos.emplace_back() = VkDescriptorImageInfo {
            .sampler = texture_impl->get_sampler(),
            .imageView = texture_impl->get_image_view(),
        };
    }

    RAISE_ERROR_OK();
}


ERROR_TYPE vk_material_builder::create_buffers_data()
{
    m_descriptor_buffer_infos.reserve(m_parameters.size());

    for (const auto* params_list : m_parameters) {
        static_cast<const vk_parameters_list_impl*>(params_list->get_impl())->visit(this);
    }

    RAISE_ERROR_OK();
}


ERROR_TYPE vk_material_builder::create_descriptor_set_layouts()
{
    for (const auto& desc_set_layout_binding : m_descriptor_sets_layout_bindings) {
        VkDescriptorSetLayoutCreateInfo descriptor_set_layout_info {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .bindingCount = static_cast<uint32_t>(1),
            .pBindings = &desc_set_layout_binding
        };

        auto& descriptor_set_layout = m_descriptor_set_layouts.emplace_back();

        if (descriptor_set_layout.init(vk_utils::context::get().device(), &descriptor_set_layout_info) != VK_SUCCESS) {
            RAISE_ERROR_WARN(-1, "cannot init descriptor set layout.");
        }
    }

    RAISE_ERROR_OK();
}


ERROR_TYPE vk_material_builder::create_descriptor_pool()
{
    if (m_descriptor_set_layouts.empty()) {
        RAISE_ERROR_OK();
    }

    VkDescriptorPoolCreateInfo pool_info{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .pNext = nullptr,
        .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
        .maxSets = 2,
        .poolSizeCount = static_cast<uint32_t>(m_pool_sizes.size()),
        .pPoolSizes = m_pool_sizes.data()
    };

    vk_utils::descriptor_pool_handler descriptor_pool{};

    if (descriptor_pool.init(vk_utils::context::get().device(), &pool_info) != VK_SUCCESS) {
        RAISE_ERROR_WARN(-1, "cannot init descriptor pool.");
    }

    m_descriptor_pool = std::move(descriptor_pool);

    RAISE_ERROR_OK();
}


ERROR_TYPE vk_material_builder::allocate_descriptor_sets()
{
    if (m_descriptor_set_layouts.empty()) {
        RAISE_ERROR_OK();
    }

    std::vector<VkDescriptorSetLayout> layouts{};
    layouts.reserve(m_descriptor_set_layouts.size());
    std::copy(m_descriptor_set_layouts.begin(), m_descriptor_set_layouts.end(), std::back_inserter(layouts));

    VkDescriptorSetAllocateInfo descriptor_set_alloc_info{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .pNext = nullptr,
        .descriptorPool = m_descriptor_pool,
        .descriptorSetCount = static_cast<uint32_t>(layouts.size()),
        .pSetLayouts = layouts.data(),
    };

    vk_utils::descriptor_set_handler descriptor_set{};

    if (descriptor_set.init(
        vk_utils::context::get().device(),
        m_descriptor_pool,
        &descriptor_set_alloc_info,
        m_descriptor_set_layouts.size()) != VK_SUCCESS) {
        RAISE_ERROR_WARN(-1, "cannot init descriptor set.");
    }

    m_descriptor_set = std::move(descriptor_set);

    RAISE_ERROR_OK();
}


void vk_material_builder::write_descriptors_into_sets()
{
    if (m_descriptor_set_layouts.empty()) {
        RAISE_ERROR_OK();
    }

    std::vector<VkWriteDescriptorSet> write_ops{};
    write_ops.reserve(m_descriptor_set_layouts.size());

    if (!m_descriptor_buffer_infos.empty()) {
        write_ops.emplace_back() = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext = nullptr,
            .dstSet = m_descriptor_set[0],
            .dstBinding = 0,
            .dstArrayElement = 0,
            .descriptorCount = static_cast<uint32_t>(m_descriptor_buffer_infos.size()),
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .pBufferInfo = m_descriptor_buffer_infos.data(),
        };
    }

    if (!m_descriptor_image_infos.empty()) {
        write_ops.emplace_back() = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext = nullptr,
            .dstSet = m_descriptor_set_layouts.size() == 1 ? m_descriptor_set[0] : m_descriptor_set[1],
            .dstBinding = 0,
            .dstArrayElement = 0,
            .descriptorCount = static_cast<uint32_t>(m_descriptor_image_infos.size()),
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .pImageInfo = m_descriptor_image_infos.data(),
        };
    }

    if (!write_ops.empty()) {
        vkUpdateDescriptorSets(
            vk_utils::context::get().device(),
            write_ops.size(),
            write_ops.data(),
            0,
            nullptr);
    }

    RAISE_ERROR_OK();
}


ERROR_TYPE vk_material_builder::create_pipeline_layout()
{
    std::vector<VkDescriptorSetLayout> layouts{};
    layouts.reserve(m_descriptor_set_layouts.size());
    std::copy(m_descriptor_set_layouts.begin(), m_descriptor_set_layouts.end(), std::back_inserter(layouts));

    VkPipelineLayoutCreateInfo pipeline_layout_info {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .flags = 0,
        .setLayoutCount = static_cast<uint32_t>(m_descriptor_set_layouts.size()),
        .pSetLayouts = layouts.data(),
        .pushConstantRangeCount = static_cast<uint32_t>(m_push_constants_ranges.size()),
        .pPushConstantRanges = m_push_constants_ranges.data()
    };

    vk_utils::pipeline_layout_handler pipeline_layout{};

    if (pipeline_layout.init(vk_utils::context::get().device(), &pipeline_layout_info) != VK_SUCCESS) {
        RAISE_ERROR_WARN(-1, "cannot init pipeline layout");
    }

    m_pipeline_layout = std::move(pipeline_layout);

    RAISE_ERROR_OK();
}

