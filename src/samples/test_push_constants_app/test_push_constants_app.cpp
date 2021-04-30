#include "test_push_constants_app.hpp"

#include <vk_utils/context.hpp>
#include <vk_utils/tools.hpp>

#include <glm/gtc/matrix_transform.hpp>

test_push_constants_app::test_push_constants_app(const char* app_name, int argc, const char** argv)
    : base_obj_viewer_app(app_name, argc, argv)
{
}

ERROR_TYPE test_push_constants_app::on_vulkan_initialized()
{
    PASS_ERROR(base_obj_viewer_app::on_vulkan_initialized());
    PASS_ERROR(init_render_passes());
    PASS_ERROR(init_framebuffers());
    PASS_ERROR(init_shaders());
    PASS_ERROR(init_pipelines());
    PASS_ERROR(record_command_buffers());

    RAISE_ERROR_OK();
}

ERROR_TYPE test_push_constants_app::on_swapchain_recreated()
{
    PASS_ERROR(init_framebuffers());
    PASS_ERROR(record_command_buffers());

    RAISE_ERROR_OK();
}

ERROR_TYPE test_push_constants_app::draw_frame()
{
    PASS_ERROR(base_obj_viewer_app::draw_frame());
    PASS_ERROR(begin_frame());
    PASS_ERROR(finish_frame(m_command_buffers[m_swapchain_data.current_image]));

    RAISE_ERROR_OK();
}

ERROR_TYPE test_push_constants_app::init_render_passes(){
    vk_utils::pass_handler render_pass{};
     
    VkAttachmentDescription pass_attachments[] {
        {
            .flags = 0,
            .format = m_swapchain_data.swapchain_info->imageFormat,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
        },
        {
            .flags = 0,
            .format = VK_FORMAT_D24_UNORM_S8_UINT,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
        },
    };

    VkAttachmentReference pass_attachment_refs[]{
        {
            .attachment = 0,
            .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
        },
        {
            .attachment = 1,
            .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
        }
    };
    
    VkSubpassDescription pass_subpasses[]{
        {
            .flags = 0,
            .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
            .colorAttachmentCount = 1,
            .pColorAttachments = pass_attachment_refs,
            .pDepthStencilAttachment = pass_attachment_refs + 1
        }
    };

    VkSubpassDependency pass_deps[]{
        {
            .srcSubpass = VK_SUBPASS_EXTERNAL,
            .dstSubpass = 0,
            .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT,
            .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
        },
        {
            .srcSubpass = 0,
            .dstSubpass = VK_SUBPASS_EXTERNAL,
            .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT
        }
    };

    VkRenderPassCreateInfo pass_info {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .attachmentCount = std::size(pass_attachments),
        .pAttachments = pass_attachments,
        .subpassCount = std::size(pass_subpasses),
        .pSubpasses = pass_subpasses,
        .dependencyCount = std::size(pass_deps),
        .pDependencies = pass_deps
    };

    if (render_pass.init(vk_utils::context::get().device(), &pass_info) != VK_SUCCESS) {
        RAISE_ERROR_FATAL(-1, "cannot init main pass");
    }

    m_main_render_pass = std::move(render_pass);

    RAISE_ERROR_OK();
}

ERROR_TYPE test_push_constants_app::init_framebuffers()
{ 
  vk_utils::vma_image_handler depth_image{};
  vk_utils::image_view_handler depth_image_view{};
  std::vector<vk_utils::framebuffer_handler> framebuffers{};
  framebuffers.reserve(m_swapchain_data.swapchain_images.size());

  uint32_t graphics_queue_index = vk_utils::context::get().queue_family_index(vk_utils::context::QUEUE_TYPE_GRAPHICS);
    
    VkImageCreateInfo img_info{
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = VK_FORMAT_D24_UNORM_S8_UINT,
        .extent = VkExtent3D{
            .width = m_swapchain_data.swapchain_info->imageExtent.width,
            .height = m_swapchain_data.swapchain_info->imageExtent.height,
            .depth = 1
        },
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 1,
        .pQueueFamilyIndices = &graphics_queue_index,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };

    VmaAllocationCreateInfo alloc_info{
        .usage = VMA_MEMORY_USAGE_GPU_ONLY
    };

    if (depth_image.init(vk_utils::context::get().allocator(), &img_info, &alloc_info) != VK_SUCCESS) {
        RAISE_ERROR_FATAL(-1, "cannot init depth image");
    }

    VkImageViewCreateInfo img_view_info{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .image = depth_image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = img_info.format,
        .components = {
            .r = VK_COMPONENT_SWIZZLE_R,
            .g = VK_COMPONENT_SWIZZLE_G,
            .b = VK_COMPONENT_SWIZZLE_B,
            .a = VK_COMPONENT_SWIZZLE_A
        },
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
        }
    };

    if (depth_image_view.init(vk_utils::context::get().device(), &img_view_info) != VK_SUCCESS) {
        RAISE_ERROR_FATAL(-1, "cannot init depth image view");
    }

    VkFramebufferCreateInfo framebuffer_info{
        .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .renderPass = m_main_render_pass,
        .width = m_swapchain_data.swapchain_info->imageExtent.width,
        .height = m_swapchain_data.swapchain_info->imageExtent.height,
        .layers = 1,
    };

    for (const auto& color_image_view : m_swapchain_data.swapchain_images_views) {
        VkImageView framebuffer_attachments[] {
            color_image_view,
            depth_image_view
        };

        framebuffer_info.attachmentCount = std::size(framebuffer_attachments);
        framebuffer_info.pAttachments = framebuffer_attachments;

        if (framebuffers.emplace_back().init(vk_utils::context::get().device(), &framebuffer_info) != VK_SUCCESS) {
            RAISE_ERROR_FATAL(-1, "cannot init main framebuffer.");
        }
    }

    m_main_depth_image = std::move(depth_image);
    m_main_depth_image_view = std::move(depth_image_view);
    m_main_pass_framebuffers = std::move(framebuffers);

    RAISE_ERROR_OK();
}

ERROR_TYPE test_push_constants_app::init_shaders()
{
    vk_utils::shader_module_handler vs{};
    vk_utils::shader_module_handler fs{};

    VkShaderStageFlagBits vs_bit{};
    VkShaderStageFlagBits fs_bit{};

    PASS_ERROR(vk_utils::load_shader(WORK_DIR"/shaders/push_constants.vert.spv", vs, vs_bit));
    PASS_ERROR(vk_utils::load_shader(WORK_DIR"/shaders/push_constants.frag.spv", fs, fs_bit));

    VkDescriptorSetLayoutBinding bindings[] {
        {
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT 
        }
    };

    VkDescriptorSetLayoutCreateInfo desc_set_layout_info{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .bindingCount = std::size(bindings),
        .pBindings = bindings,
    };
    
    vk_utils::descriptor_set_layout_handler desc_set_layout{};

    if (desc_set_layout.init(vk_utils::context::get().device(), &desc_set_layout_info) != VK_SUCCESS) {
        RAISE_ERROR_FATAL(-1, "cannot init descriptor set layout.");
    }

    VkPushConstantRange range{
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
        .offset = 0,
        .size = sizeof(push_constant_data)
    };

    VkPipelineLayoutCreateInfo pipeline_layout_info{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .setLayoutCount = 1,
        .pSetLayouts = desc_set_layout,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &range
    };

    vk_utils::pipeline_layout_handler pipeline_layout{};

    if (pipeline_layout.init(vk_utils::context::get().device(), &pipeline_layout_info) != VK_SUCCESS) {
        RAISE_ERROR_FATAL(-1, "cannot init pipeline layout.");
    }

    VkDescriptorPoolSize desc_pool_size{
        .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .descriptorCount = 1,
    };

    vk_utils::descriptor_pool_handler descriptor_pool{};

    VkDescriptorPoolCreateInfo desc_pool_info{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .pNext = nullptr,
        .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
        .maxSets = 1,
        .poolSizeCount = 1,
        .pPoolSizes = &desc_pool_size,
    };
    
    if (descriptor_pool.init(vk_utils::context::get().device(), &desc_pool_info) != VK_SUCCESS) {
        RAISE_ERROR_FATAL(-1, "cannot init descriptor pool.");
    }

    VkDescriptorSetAllocateInfo desc_set_info{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .pNext = nullptr,
        .descriptorPool = descriptor_pool,
        .descriptorSetCount = 1,
        .pSetLayouts = desc_set_layout
    };
    
    vk_utils::descriptor_set_handler desc_set{};

    if (desc_set.init(vk_utils::context::get().device(), descriptor_pool, &desc_set_info, 1) != VK_SUCCESS) {
        RAISE_ERROR_FATAL(-1, "cannot init descriptor set.");
    }

    VkDescriptorBufferInfo desc_buffer_info{
        .buffer = m_ubo,
        .offset = 0,
        .range = sizeof(global_ubo)
    };

    VkWriteDescriptorSet write_op{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .pNext = nullptr,
        .dstSet = desc_set[0],
        .dstBinding = 0,
        .dstArrayElement = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .pBufferInfo = &desc_buffer_info,
    };

    vkUpdateDescriptorSets(vk_utils::context::get().device(), 1, &write_op, 0, nullptr);

    m_vert_shader = std::move(vs);
    m_frag_shader = std::move(fs);
    m_descriptor_set_layout = std::move(desc_set_layout);
    m_pipeline_layout = std::move(pipeline_layout);
    m_descriptor_pool = std::move(descriptor_pool);
    m_descriptor_set = std::move(desc_set);

    RAISE_ERROR_OK();
}

ERROR_TYPE test_push_constants_app::init_pipelines()
{ 
    VkPipelineShaderStageCreateInfo shader_stages[]{
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .stage = VK_SHADER_STAGE_VERTEX_BIT,
            .module = m_vert_shader,
            .pName = "main",
            .pSpecializationInfo = nullptr
        },
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module = m_frag_shader,
            .pName = "main",
            .pSpecializationInfo = nullptr
        },
    };

    VkPipelineInputAssemblyStateCreateInfo input_assembly_info{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .primitiveRestartEnable = VK_FALSE,
    };

    VkPipelineViewportStateCreateInfo viewport_info{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .viewportCount = 1,
        .pViewports = nullptr,
        .scissorCount = 1,
        .pScissors = nullptr,
    };

    VkPipelineRasterizationStateCreateInfo rasterization_info{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .depthClampEnable = VK_FALSE,
        .rasterizerDiscardEnable = VK_FALSE,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .cullMode = VK_CULL_MODE_BACK_BIT,
        .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
        .depthBiasEnable = VK_FALSE,
        .lineWidth = 1
    };

    VkPipelineMultisampleStateCreateInfo multisample_info{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
        .sampleShadingEnable = VK_FALSE,
        .alphaToCoverageEnable = VK_FALSE,
        .alphaToOneEnable = VK_FALSE,
    };

    VkPipelineDepthStencilStateCreateInfo depth_stencil_info{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .depthTestEnable = VK_TRUE,
        .depthWriteEnable = VK_TRUE,
        .depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL,
        .depthBoundsTestEnable = VK_FALSE,
        .stencilTestEnable = VK_FALSE,
    };

    VkPipelineColorBlendAttachmentState blend_state{
        .blendEnable = VK_FALSE,
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
    };

    VkPipelineColorBlendStateCreateInfo color_blend_info{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .logicOpEnable = VK_FALSE,
        .attachmentCount = 1,
        .pAttachments = &blend_state
    };

    VkDynamicState dyn_states[]{
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR,
    };
    
    VkPipelineDynamicStateCreateInfo dyn_state_info{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .dynamicStateCount = std::size(dyn_states),
        .pDynamicStates = dyn_states
    };

    VkGraphicsPipelineCreateInfo pipeline_info{
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .stageCount = std::size(shader_stages),
        .pStages = shader_stages,
        .pInputAssemblyState = &input_assembly_info,
        .pTessellationState = nullptr,
        .pViewportState = &viewport_info,
        .pRasterizationState = &rasterization_info,
        .pMultisampleState = &multisample_info,
        .pDepthStencilState = &depth_stencil_info,
        .pColorBlendState = &color_blend_info,
        .pDynamicState = &dyn_state_info,
        .layout = m_pipeline_layout,
        .renderPass = m_main_render_pass,
        .subpass = 0,
        .basePipelineHandle = VK_NULL_HANDLE,
        .basePipelineIndex = -1
    };

    std::unordered_map<uint32_t, vk_utils::graphics_pipeline_handler> pipelines{};
    m_graphics_pipelines.reserve(m_model.sub_geometries.size());

    for (auto& subgeom : m_model.sub_geometries) {
        uint32_t pipeline_id{0};

        VkVertexInputAttributeDescription vert_attrs[3] {
            {
                .location = 0,
                .binding = 0,
                .format = VK_FORMAT_R32G32B32_SFLOAT,
                .offset = 0,
            },
            {
                .location = 1,
                .binding = 0,
                .format = VK_FORMAT_R32G32B32_SFLOAT,
                .offset = 0,
            },
            {
                .location = 2,
                .binding = 0,
                .format = VK_FORMAT_R32G32_SFLOAT,
                .offset = 0,
            }
        };

        if (subgeom.format.size() > std::size(vert_attrs)) {
            RAISE_ERROR_FATAL(-1, "invalid vertex format");
        }

        uint32_t stride = 0;

        for (int i = 0; i < subgeom.format.size(); ++i) {
            if (vert_attrs[i].format != subgeom.format[i]) {
                RAISE_ERROR_FATAL(-1, "incompatible vertex attrs layout");
            }

            vert_attrs[i].binding = 0;
            vert_attrs[i].location = i;
            vert_attrs[i].offset = stride;

            switch (subgeom.format[i]) {
                case VK_FORMAT_R32G32B32_SFLOAT:
                    stride += sizeof(float) * 3;
                    pipeline_id |= 1 << i;
                    break;
                case VK_FORMAT_R32G32_SFLOAT:
                    stride += sizeof(float) * 2;
                    pipeline_id |= 1 << i * 2;
                    break;
                default:
                    RAISE_ERROR_FATAL(-1, "invalid vertex attribute format");
            }
        }

        VkVertexInputBindingDescription vertex_binding{
            .binding = 0,
            .stride = stride,
            .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
        };

        VkPipelineVertexInputStateCreateInfo vertex_input_state{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .vertexBindingDescriptionCount = 1,
            .pVertexBindingDescriptions = &vertex_binding,
            .vertexAttributeDescriptionCount = std::size(vert_attrs),
            .pVertexAttributeDescriptions = vert_attrs
        };

        pipeline_info.pVertexInputState = &vertex_input_state;

        auto pipeline_it = pipelines.find(pipeline_id);

        if (pipeline_it == pipelines.end()) {
            vk_utils::graphics_pipeline_handler pipeline{};
            if (pipeline.init(vk_utils::context::get().device(), &pipeline_info) != VK_SUCCESS) {
                RAISE_ERROR_FATAL(-1, "cannot init pipeline.");
            }
            pipelines[pipeline_id] = std::move(pipeline);
        }

        m_graphics_pipelines.emplace_back(pipelines[pipeline_id]);
    }

    m_pipelines_handlers = std::move(pipelines);

    RAISE_ERROR_OK();
}

ERROR_TYPE test_push_constants_app::record_command_buffers()
{
    vk_utils::cmd_buffers_handler cmd_buffers{};

    VkCommandBufferAllocateInfo buffers_alloc_info {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext = nullptr,
        .commandPool = m_command_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = static_cast<uint32_t>(m_main_pass_framebuffers.size())
    };

    if (cmd_buffers.init(vk_utils::context::get().device(), m_command_pool, &buffers_alloc_info, m_main_pass_framebuffers.size()) != VK_SUCCESS) {
        RAISE_ERROR_FATAL(-1, "cannot init command buffers");
    }

    VkCommandBufferBeginInfo cmd_buffer_begin_info{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = nullptr,
        .flags = 0,
        .pInheritanceInfo = nullptr
    };

    VkClearValue clear_values[]{
        {.color{.float32 = {0.2, 0.3, 0.7, 1.}}},
        {.depthStencil = {.depth = 1.0f, .stencil = 0}}
    };

    VkRenderPassBeginInfo pass_begin_info{
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .pNext = nullptr,
        .renderPass = m_main_render_pass,
        .renderArea = {
            .offset = {0, 0},
            .extent = m_swapchain_data.swapchain_info->imageExtent},
        .clearValueCount = std::size(clear_values),
        .pClearValues = clear_values,
    };

    VkViewport viewport{
        .x = 0,
        .y = 0,
        .width = static_cast<float>(m_swapchain_data.swapchain_info->imageExtent.width),
        .height = static_cast<float>(m_swapchain_data.swapchain_info->imageExtent.height),
        .minDepth = 0,
        .maxDepth = 1,
    };

    VkRect2D scissor{
        .offset = {0, 0},
        .extent = m_swapchain_data.swapchain_info->imageExtent
    };

    static glm::vec4 colors[]{
        {0., 0., 0., 1.},
        {1., 0., 0., 1.},
        {0., 1., 0., 1.},
        {0., 0., 1., 1.},
        {1., 1., 0., 1.},
        {0., 1., 1., 1.},
        {1., 0., 1., 1.},
        {1., 1., 1., 1.},
        {1., 0.5, 0., 1.},
        {0., 0.5, 1., 1.},
    };

    float rot_angle = 360.0f / std::size(colors);

    for (int i = 0; i < m_main_pass_framebuffers.size(); ++i) {
        pass_begin_info.framebuffer = m_main_pass_framebuffers[i];

        vkBeginCommandBuffer(cmd_buffers[i], &cmd_buffer_begin_info);

        vkCmdBeginRenderPass(cmd_buffers[i], &pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdSetViewport(cmd_buffers[i], 0, 1, &viewport);
        vkCmdSetScissor(cmd_buffers[i], 0, 1, &scissor);

        VkPipeline last_pipeline = VK_NULL_HANDLE;

        float curr_angle = 0;

        for (size_t j = 0; j < std::size(colors); j++) {
            push_constant_data data{
                .color = colors[j],
                .transform = glm::mat4{1}
            };
            
            data.transform = glm::rotate(data.transform, glm::radians(curr_angle), {0.0f, 0.0f, 1.0f});
            data.transform = glm::translate(data.transform, {0, 10, 0});
            
            curr_angle += rot_angle;

            vkCmdPushConstants(cmd_buffers[i], m_pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(push_constant_data), &data);

            for (int j = 0; j < m_model.sub_geometries.size(); j++) {
                if (m_graphics_pipelines[j] != last_pipeline) {
                    vkCmdBindPipeline(cmd_buffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphics_pipelines[j]);        
                    vkCmdBindDescriptorSets(cmd_buffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline_layout, 0, 1, m_descriptor_set, 0, nullptr);
                    last_pipeline = m_graphics_pipelines[j];
                }

                VkDeviceSize vertex_buffer_offset = m_model.sub_geometries[j].vertices_offset;
                vkCmdBindVertexBuffers(cmd_buffers[i], 0, 1, m_model.vertex_buffer, &vertex_buffer_offset);
                vkCmdBindIndexBuffer(cmd_buffers[i], m_model.index_buffer, m_model.sub_geometries[j].indices_offset * sizeof(uint32_t), VK_INDEX_TYPE_UINT32);
                std::vector<VkDescriptorSet> desc_sets;
                vkCmdDrawIndexed(cmd_buffers[i], m_model.sub_geometries[j].indices_size, 1, 0, 0, 0);
            }
        }

        vkCmdEndRenderPass(cmd_buffers[i]);
        
        if (vkEndCommandBuffer(cmd_buffers[i]) != VK_SUCCESS) {
            RAISE_ERROR_FATAL(-1, "cannot record command buffer.");
        }
    }

    m_command_buffers = std::move(cmd_buffers);

    RAISE_ERROR_OK();
}
