

#include <app/vk_app.hpp>
#include <logger/log.hpp>

#include <vk_utils/context.hpp>
#include <vk_utils/tools.hpp>
#include <vk_utils/obj_loader.hpp>

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/gtc/matrix_transform.hpp>

#include <vector>
#include <cstring>

struct shader
{
    std::vector<VkShaderModule> modules;
    std::vector<VkShaderStageFlagBits> stages;
    std::vector<VkDescriptorSetLayout> descriptor_set_layouts;
    std::vector<vk_utils::descriptor_set_handler> descriptor_sets;
};


struct shader_group
{
    std::vector<vk_utils::shader_module_handler> shader_modules;
    vk_utils::descriptor_pool_handler desc_pool;
    std::vector<vk_utils::descriptor_set_layout_handler> shaders_layouts;
    std::vector<shader> shaders;
};


struct global_ubo
{
    glm::mat4 projection = glm::identity<glm::mat4>();
    glm::mat4 view = glm::identity<glm::mat4>();
    glm::mat4 model = glm::identity<glm::mat4>();
    glm::mat4 mvp = glm::identity<glm::mat4>();
};


class dummy_obj_viewer_app : public app::vk_app
{
public:
    dummy_obj_viewer_app(const char* app_name, int argc, const char** argv)
        : vk_app(app_name, argc, argv)
    {
    }


protected:
    ERROR_TYPE on_vulkan_initialized() override
    {
        if (m_app_info.argc <= 1) {
            RAISE_ERROR_FATAL(-1, "invalid arguments count.");
        }

        const char** textures_list = nullptr;

        if (m_app_info.argc > 2) {
            textures_list = m_app_info.argv + 2;
        }

        PASS_ERROR(init_main_render_pass());
        PASS_ERROR(init_main_frame_buffers());
        PASS_ERROR(init_dummy_model_resources(
            m_app_info.argv[1], textures_list, m_app_info.argc - 2));
        PASS_ERROR(record_obj_model_dummy_draw_commands(
            m_model, m_dummy_shader_group, m_command_pool, m_command_buffers, m_pipelines_layout, m_graphics_pipelines));
        RAISE_ERROR_OK();
    }


    ERROR_TYPE on_swapchain_recreated() override
    {
        HANDLE_ERROR(init_main_frame_buffers());
        m_pipelines_layout.clear();
        m_graphics_pipelines.clear();
        m_command_buffers.destroy();
        HANDLE_ERROR(record_obj_model_dummy_draw_commands(
            m_model, m_dummy_shader_group, m_command_pool, m_command_buffers, m_pipelines_layout, m_graphics_pipelines));

        RAISE_ERROR_OK();
    }


    ERROR_TYPE draw_frame() override
    {
        static float angle = 0.0;
        angle += 0.01;

        if (angle >= 360.0f) {
            angle = 0.0f;
        }

        static global_ubo ubo_data{};

        auto model = glm::identity<glm::mat4>();
        model = glm::translate(model, {0.0f, 0.0f, 0.0f});
        model = glm::rotate(model, glm::radians(-90.0f), glm::vec3{1.0f, 0.0f, 0.0f});
        model = glm::rotate(model, glm::radians(angle), glm::vec3{0.0f, 0.0f, 1.0f});
        ubo_data.model = model;

        ubo_data.projection = glm::perspectiveFov(glm::radians(90.0f), float(m_swapchain_data.swapchain_info->imageExtent.width), float(m_swapchain_data.swapchain_info->imageExtent.height), 0.01f, 100.0f);

        ubo_data.view = glm::lookAt(glm::vec3{0.0f, 0.0f, -3.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f});
        ubo_data.mvp = ubo_data.projection * ubo_data.view * ubo_data.model;

        void* mapped_data;
        vmaMapMemory(vk_utils::context::get().allocator(), m_ubo, &mapped_data);
        std::memcpy(mapped_data, &ubo_data, sizeof(ubo_data));
        vmaFlushAllocation(vk_utils::context::get().allocator(), m_ubo, 0, VK_WHOLE_SIZE);
        vmaUnmapMemory(vk_utils::context::get().allocator(), m_ubo);

        begin_frame();
        finish_frame(m_command_buffers[m_swapchain_data.current_image]);

        RAISE_ERROR_OK();
    }

    ERROR_TYPE on_window_size_changed(int w, int h) override
    {
        PASS_ERROR(vk_app::on_window_size_changed(w, h));
        RAISE_ERROR_OK();
    }


    ERROR_TYPE init_dummy_shaders(const vk_utils::obj_loader::obj_model& model, const VkBuffer ubo, shader_group& sgroup)
    {
        sgroup.shader_modules.reserve(2);
        auto& vs = sgroup.shader_modules.emplace_back();
        VkShaderStageFlagBits vs_stage{};
        auto& fs = sgroup.shader_modules.emplace_back();
        VkShaderStageFlagBits fs_stage{};

        vk_utils::load_shader("./shaders/dummy.vert.glsl.spv", vs, vs_stage);
        vk_utils::load_shader("./shaders/dummy.frag.glsl.spv", fs, fs_stage);

        VkDescriptorSetLayoutBinding bindings[]{
            {
                .binding = 0,
                .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
            },
            {
                .binding = 0,
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
            }};

        VkDescriptorSetLayoutCreateInfo desc_set_layout_info{};
        desc_set_layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        desc_set_layout_info.pNext = nullptr;
        desc_set_layout_info.flags = 0;
        desc_set_layout_info.bindingCount = 1;
        desc_set_layout_info.pBindings = &bindings[0];

        sgroup.shaders_layouts.reserve(2);
        auto& ubo_set_layout = sgroup.shaders_layouts.emplace_back();
        ubo_set_layout.init(vk_utils::context::get().device(), &desc_set_layout_info);

        desc_set_layout_info.pBindings = &bindings[1];
        auto& texture_set_layout = sgroup.shaders_layouts.emplace_back();
        texture_set_layout.init(vk_utils::context::get().device(), &desc_set_layout_info);

        VkDescriptorPoolSize descriptor_pool_sizes[]{
            {
                .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                .descriptorCount = static_cast<uint32_t>(model.sub_geometries.size()),
            },
            {
                .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .descriptorCount = static_cast<uint32_t>(model.sub_geometries.size()),
            }};

        VkDescriptorPoolCreateInfo desc_pool_info{};
        desc_pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        desc_pool_info.pNext = nullptr;
        desc_pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        desc_pool_info.maxSets = model.sub_geometries.size() * 2;
        desc_pool_info.pPoolSizes = descriptor_pool_sizes;
        desc_pool_info.poolSizeCount = std::size(descriptor_pool_sizes);

        if (sgroup.desc_pool.init(vk_utils::context::get().device(), &desc_pool_info) != VK_SUCCESS) {
            RAISE_ERROR_FATAL(-1, "cannot init desc pool.");
        }

        for (auto& subgeom : model.sub_geometries) {
            auto& s = sgroup.shaders.emplace_back();
            s.modules.emplace_back(vs);
            s.modules.emplace_back(fs);
            s.stages.emplace_back(vs_stage);
            s.stages.emplace_back(fs_stage);
            s.descriptor_set_layouts.emplace_back(ubo_set_layout);
            s.descriptor_set_layouts.emplace_back(texture_set_layout);

            VkDescriptorSetAllocateInfo desc_sets_alloc_info{};
            desc_sets_alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
            desc_sets_alloc_info.pNext = nullptr;
            desc_sets_alloc_info.descriptorPool = sgroup.desc_pool;
            desc_sets_alloc_info.descriptorSetCount = 1;

            desc_sets_alloc_info.pSetLayouts = ubo_set_layout;
            s.descriptor_sets.reserve(2);
            auto& ubo_desc_set = s.descriptor_sets.emplace_back();
            ubo_desc_set.init(vk_utils::context::get().device(), sgroup.desc_pool, &desc_sets_alloc_info, desc_sets_alloc_info.descriptorSetCount);

            desc_sets_alloc_info.pSetLayouts = texture_set_layout;
            auto& tex_desc_set = s.descriptor_sets.emplace_back();
            tex_desc_set.init(vk_utils::context::get().device(), sgroup.desc_pool, &desc_sets_alloc_info, desc_sets_alloc_info.descriptorSetCount);

            VkWriteDescriptorSet write_desc_set{};
            write_desc_set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            write_desc_set.pNext = nullptr;
            write_desc_set.descriptorCount = 1;
            write_desc_set.dstArrayElement = 0;
            write_desc_set.dstBinding = 0;
            write_desc_set.dstSet = ubo_desc_set[0];
            write_desc_set.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            VkDescriptorBufferInfo ubo_info{};
            ubo_info.offset = 0;
            ubo_info.buffer = ubo;
            ubo_info.range = VK_WHOLE_SIZE;
            write_desc_set.pBufferInfo = &ubo_info;

            vkUpdateDescriptorSets(vk_utils::context::get().device(), 1, &write_desc_set, 0, nullptr);
            VkImageView img_view{nullptr};
            VkSampler sampler{nullptr};

            switch (subgeom.render_technique) {
                case vk_utils::obj_loader::PHONG: {
                    auto& m = std::get<vk_utils::obj_loader::obj_phong_material>(subgeom.material);
                    if (m.material_textures[vk_utils::obj_loader::PHONG_DIFFUSE] >= 0) {
                        img_view = model.textures[m.material_textures[m.material_textures[vk_utils::obj_loader::PHONG_DIFFUSE]]].image_view;
                        sampler = model.textures[m.material_textures[m.material_textures[vk_utils::obj_loader::PHONG_DIFFUSE]]].sampler;
                    }
                } break;
                case vk_utils::obj_loader::PBR:
                    RAISE_ERROR_FATAL(-1, "unsupported render technique.");
            }

            if (img_view != nullptr) {
                VkDescriptorImageInfo img_info{};
                img_info.imageView = img_view;
                img_info.sampler = sampler;
                img_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                write_desc_set.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                write_desc_set.dstSet = tex_desc_set[0];
                write_desc_set.pBufferInfo = nullptr;
                write_desc_set.pImageInfo = &img_info;
                vkUpdateDescriptorSets(vk_utils::context::get().device(), 1, &write_desc_set, 0, nullptr);
            }
        }

        RAISE_ERROR_OK();
    }

    ERROR_TYPE init_dummy_model_resources(const char* path, const char** textures = nullptr, size_t textures_size = 0)
    {
        VkCommandPoolCreateInfo cmd_pool_create_info{};
        cmd_pool_create_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        cmd_pool_create_info.pNext = nullptr;
        cmd_pool_create_info.queueFamilyIndex = vk_utils::context::get().queue_family_index(vk_utils::context::QUEUE_TYPE_GRAPHICS);
        m_command_pool.init(vk_utils::context::get().device(), &cmd_pool_create_info);
        PASS_ERROR(vk_utils::create_buffer(m_ubo, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, sizeof(global_ubo)));

        vk_utils::obj_loader::obj_model_info model_info{};
        for (int i = 0; i < textures_size; ++i) {
            auto& mat_textures = model_info.model_textures.emplace_back();
            mat_textures[0] = textures[i];
        }

        vk_utils::obj_loader loader{};
        PASS_ERROR(loader.load_model(path, model_info, vk_utils::context::get().queue(vk_utils::context::QUEUE_TYPE_GRAPHICS), m_command_pool, m_model));
        PASS_ERROR(init_dummy_shaders(m_model, m_ubo, m_dummy_shader_group));

        RAISE_ERROR_OK();
    }

    ERROR_TYPE init_geom_pipelines(
        const vk_utils::obj_loader::obj_model& model,
        const std::vector<shader>& shaders,
        VkPrimitiveTopology topo,
        VkExtent2D input_viewport,
        std::vector<vk_utils::graphics_pipeline_handler>& pipelines,
        std::vector<vk_utils::pipeline_layout_handler>& pipeline_layouts)
    {
        for (int i = 0; i < model.sub_geometries.size(); ++i) {
            const auto& subgeom = model.sub_geometries[i];
            const auto& shader = shaders[i];

            std::vector<VkPipelineShaderStageCreateInfo> shader_stages{};
            shader_stages.reserve(shader.stages.size());

            if (shader.stages.size() != shader.modules.size()) {
                RAISE_ERROR_FATAL(-1, "invalid shader modules count");
            }

            for (size_t ii = 0; ii < shader.stages.size(); ++ii) {
                auto& stage = shader_stages.emplace_back();
                stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
                stage.pNext = nullptr;
                stage.flags = 0;
                stage.module = shader.modules[ii];
                stage.stage = shader.stages[ii];
                stage.pName = "main";
            }

            VkVertexInputAttributeDescription vert_attrs[3] =
                {
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
                    }};
            if (subgeom.format.size() > std::size(vert_attrs)) {
                RAISE_ERROR_FATAL(-1, "invalid vertex format");
            }
            uint32_t stride = 0;
            for (int i = 0; i < subgeom.format.size(); ++i) {
                vert_attrs[i].binding = 0;
                vert_attrs[i].location = i;
                vert_attrs[i].format = subgeom.format[i];
                vert_attrs[i].offset = stride;

                switch (subgeom.format[i]) {
                    case VK_FORMAT_R32G32B32_SFLOAT:
                        stride += sizeof(float) * 3;
                        break;
                    case VK_FORMAT_R32G32_SFLOAT:
                        stride += sizeof(float) * 2;
                        break;
                    default:
                        RAISE_ERROR_FATAL(-1, "invalid vertex attribute format");
                }
            }

            VkVertexInputBindingDescription vert_binding{};
            vert_binding.binding = 0;
            vert_binding.stride = stride;
            vert_binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

            VkPipelineVertexInputStateCreateInfo vert_input_info{};
            vert_input_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
            vert_input_info.pNext = nullptr;
            vert_input_info.flags = 0;
            vert_input_info.pVertexAttributeDescriptions = vert_attrs;
            vert_input_info.vertexAttributeDescriptionCount = std::size(vert_attrs);
            vert_input_info.pVertexBindingDescriptions = &vert_binding;
            vert_input_info.vertexBindingDescriptionCount = 1;

            VkPipelineInputAssemblyStateCreateInfo input_assembly_info{};
            input_assembly_info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
            input_assembly_info.pNext = nullptr;
            input_assembly_info.flags = 0;
            input_assembly_info.primitiveRestartEnable = VK_FALSE;
            input_assembly_info.topology = topo;

            VkRect2D scissor;
            scissor.offset.x = 0;
            scissor.offset.y = 0;
            scissor.extent = input_viewport;

            VkViewport viewport;
            viewport.width = input_viewport.width;
            viewport.height = input_viewport.height;
            viewport.x = 0;
            viewport.y = 0;
            viewport.minDepth = 0;
            viewport.maxDepth = 1;

            VkPipelineViewportStateCreateInfo viewport_info{};
            viewport_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
            viewport_info.pNext = nullptr;
            viewport_info.flags = 0;

            viewport_info.pScissors = &scissor;
            viewport_info.scissorCount = 1;
            viewport_info.pViewports = &viewport;
            viewport_info.viewportCount = 1;

            VkPipelineRasterizationStateCreateInfo rasterization_info{};
            rasterization_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
            rasterization_info.pNext = nullptr;
            rasterization_info.flags = 0;
            rasterization_info.cullMode = VK_CULL_MODE_BACK_BIT;
            rasterization_info.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
            rasterization_info.polygonMode = VK_POLYGON_MODE_FILL;
            rasterization_info.lineWidth = 1;
            rasterization_info.depthClampEnable = VK_FALSE;
            rasterization_info.depthBiasEnable = VK_FALSE;
            rasterization_info.rasterizerDiscardEnable = VK_FALSE;

            VkPipelineMultisampleStateCreateInfo multisample_info{};
            multisample_info.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
            multisample_info.pNext = nullptr;
            multisample_info.flags = 0;
            multisample_info.alphaToCoverageEnable = VK_FALSE;
            multisample_info.rasterizationSamples = VK_SAMPLE_COUNT_4_BIT;
            multisample_info.sampleShadingEnable = VK_FALSE;
            multisample_info.alphaToOneEnable = VK_FALSE;
            multisample_info.minSampleShading = 1.0f;
            multisample_info.pSampleMask = nullptr;

            VkPipelineDepthStencilStateCreateInfo depth_stencil_info{};
            depth_stencil_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
            depth_stencil_info.pNext = nullptr;
            depth_stencil_info.flags = 0;
            depth_stencil_info.depthTestEnable = VK_TRUE;
            depth_stencil_info.depthWriteEnable = VK_TRUE;
            depth_stencil_info.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
            depth_stencil_info.stencilTestEnable = VK_FALSE;
            depth_stencil_info.depthBoundsTestEnable = VK_FALSE;

            VkPipelineColorBlendAttachmentState color_blend_attachment{};
            color_blend_attachment.blendEnable = VK_FALSE;
            color_blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

            VkPipelineColorBlendStateCreateInfo color_blend_info{};
            color_blend_info.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
            color_blend_info.pNext = nullptr;
            color_blend_info.flags = 0;
            color_blend_info.pAttachments = &color_blend_attachment;
            color_blend_info.attachmentCount = 1;
            color_blend_info.logicOpEnable = VK_FALSE;

            VkPipelineLayoutCreateInfo pipeline_layout_info{};
            pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
            pipeline_layout_info.pNext = nullptr;
            pipeline_layout_info.flags = 0;

            std::vector<VkDescriptorSetLayout> desc_set_layouts;
            std::transform(shader.descriptor_set_layouts.begin(), shader.descriptor_set_layouts.end(), std::back_inserter(desc_set_layouts), [](VkDescriptorSetLayout l) { return l; });

            pipeline_layout_info.pSetLayouts = desc_set_layouts.data();
            pipeline_layout_info.setLayoutCount = desc_set_layouts.size();
            pipeline_layout_info.pushConstantRangeCount = 0;
            pipeline_layout_info.pPushConstantRanges = nullptr;

            auto& pipeline_layout = pipeline_layouts.emplace_back();
            pipeline_layout.init(vk_utils::context::get().device(), &pipeline_layout_info);

            VkGraphicsPipelineCreateInfo pipeline_create_info{};
            pipeline_create_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
            pipeline_create_info.pNext = nullptr;
            pipeline_create_info.flags = 0;

            pipeline_create_info.pStages = shader_stages.data();
            pipeline_create_info.stageCount = shader_stages.size();
            pipeline_create_info.pVertexInputState = &vert_input_info;
            pipeline_create_info.pInputAssemblyState = &input_assembly_info;
            pipeline_create_info.pTessellationState = nullptr;
            pipeline_create_info.pViewportState = &viewport_info;
            pipeline_create_info.pRasterizationState = &rasterization_info;
            pipeline_create_info.pMultisampleState = &multisample_info;
            pipeline_create_info.pDepthStencilState = &depth_stencil_info;
            pipeline_create_info.pColorBlendState = &color_blend_info;
            pipeline_create_info.pDynamicState = nullptr;
            pipeline_create_info.layout = pipeline_layout;

            pipeline_create_info.renderPass = m_main_render_pass;
            pipeline_create_info.subpass = 0;
            pipeline_create_info.basePipelineHandle = nullptr;
            pipeline_create_info.basePipelineIndex = -1;

            auto& pipeline = pipelines.emplace_back();
            if (pipeline.init(vk_utils::context::get().device(), &pipeline_create_info) != VK_SUCCESS) {
                RAISE_ERROR_FATAL(-1, "cannot init pipeline.");
            }
        }
    }

    ERROR_TYPE record_obj_model_dummy_draw_commands(
        const vk_utils::obj_loader::obj_model& model,
        const shader_group& sgroup,
        VkCommandPool cmd_pool,
        vk_utils::cmd_buffers_handler& cmd_buffers,
        std::vector<vk_utils::pipeline_layout_handler>& pipelines_layouts,
        std::vector<vk_utils::graphics_pipeline_handler>& pipelines)
    {
        init_geom_pipelines(
            model,
            sgroup.shaders,
            VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
            m_swapchain_data.swapchain_info->imageExtent,
            pipelines,
            pipelines_layouts);

        VkCommandBufferAllocateInfo buffers_alloc_info{};
        buffers_alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        buffers_alloc_info.pNext = nullptr;
        buffers_alloc_info.commandPool = cmd_pool;
        buffers_alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        buffers_alloc_info.commandBufferCount = m_main_pass_framebuffers.size();

        cmd_buffers.init(vk_utils::context::get().device(), cmd_pool, &buffers_alloc_info, m_main_pass_framebuffers.size());

        VkCommandBufferBeginInfo cmd_buffer_begin_info{};
        cmd_buffer_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        cmd_buffer_begin_info.pNext = nullptr;
        cmd_buffer_begin_info.flags = 0;
        cmd_buffer_begin_info.pInheritanceInfo = nullptr;

        VkClearValue clear_values[]{
            {.color{.float32 = {0.0, 0.0, 0.0, 1.}}},
            {.depthStencil = {.depth = 1.0f, .stencil = 0}},
            {.color{.float32 = {0.0, 0.0, 0.0, 1.}}},
        };

        VkRenderPassBeginInfo pass_begin_info{};
        pass_begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        pass_begin_info.pNext = nullptr;
        pass_begin_info.renderPass = m_main_render_pass;
        pass_begin_info.pClearValues = clear_values;
        pass_begin_info.clearValueCount = std::size(clear_values);
        pass_begin_info.renderArea = {
            .offset = {0, 0},
            .extent = m_swapchain_data.swapchain_info->imageExtent};

        for (int i = 0; i < m_main_pass_framebuffers.size(); ++i) {
            pass_begin_info.framebuffer = m_main_pass_framebuffers[i];

            vkBeginCommandBuffer(cmd_buffers[i], &cmd_buffer_begin_info);
            vkCmdBeginRenderPass(cmd_buffers[i], &pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);
            for (int j = 0; j < model.sub_geometries.size(); j++) {
                vkCmdBindPipeline(cmd_buffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines[j]);
                VkDeviceSize vertex_buffer_offset = model.sub_geometries[j].vertices_offset;
                vkCmdBindVertexBuffers(cmd_buffers[i], 0, 1, model.vertex_buffer, &vertex_buffer_offset);
                vkCmdBindIndexBuffer(cmd_buffers[i], model.index_buffer, model.sub_geometries[j].indices_offset * sizeof(uint32_t), VK_INDEX_TYPE_UINT32);
                std::vector<VkDescriptorSet> desc_sets;
                desc_sets.reserve(sgroup.shaders[j].descriptor_sets.size());
                std::transform(sgroup.shaders[j].descriptor_sets.begin(), sgroup.shaders[j].descriptor_sets.end(), std::back_inserter(desc_sets), [](const vk_utils::descriptor_set_handler& set) { return set[0]; });
                vkCmdBindDescriptorSets(cmd_buffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines_layouts[j], 0, sgroup.shaders[j].descriptor_sets.size(), desc_sets.data(), 0, nullptr);
                vkCmdDrawIndexed(cmd_buffers[i], model.sub_geometries[j].indices_size, 1, 0, 0, 0);
            }
            vkCmdEndRenderPass(cmd_buffers[i]);
            vkEndCommandBuffer(cmd_buffers[i]);
        }

        RAISE_ERROR_OK();
    }


    ERROR_TYPE init_main_render_pass()
    {
        VkRenderPassCreateInfo pass_create_info{};

        pass_create_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        pass_create_info.pNext = nullptr;

        VkAttachmentDescription pass_attachments[]{
            {
                .format = m_swapchain_data.swapchain_info->imageFormat,
                .samples = VK_SAMPLE_COUNT_1_BIT,
                .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            },
            {
                .format = VK_FORMAT_D24_UNORM_S8_UINT,
                .samples = VK_SAMPLE_COUNT_4_BIT,
                .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            },
            {
                .format = m_swapchain_data.swapchain_info->imageFormat,
                .samples = VK_SAMPLE_COUNT_4_BIT,
                .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            }};

        VkAttachmentReference refs[]{
            {
                .attachment = 0,
                .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            },
            {
                .attachment = 1,
                .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            },
            {
                .attachment = 2,
                .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            }};


        VkSubpassDescription subpass_description{};
        subpass_description.colorAttachmentCount = 1;
        subpass_description.pColorAttachments = &refs[2];
        subpass_description.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass_description.pDepthStencilAttachment = &refs[1];
        subpass_description.pResolveAttachments = &refs[0];

        VkSubpassDependency dependency{};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.srcAccessMask = 0;
        dependency.dstSubpass = 0;
        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        pass_create_info.pAttachments = pass_attachments;
        pass_create_info.attachmentCount = std::size(pass_attachments);
        pass_create_info.pSubpasses = &subpass_description;
        pass_create_info.subpassCount = 1;
        pass_create_info.pDependencies = &dependency;
        pass_create_info.dependencyCount = 0;

        if (auto err_code = m_main_render_pass.init(vk_utils::context::get().device(), &pass_create_info);
            err_code != VK_SUCCESS) {
            RAISE_ERROR_FATAL(-1, "cannot create rpass.");
        }

        RAISE_ERROR_OK();
    }

    ERROR_TYPE init_main_frame_buffers()
    {
        m_main_depth_image.destroy();
        m_main_depth_image_view.destroy();
        m_main_msaa_image.destroy();
        m_main_msaa_image_view.destroy();
        m_main_pass_framebuffers.clear();
        m_main_pass_framebuffers.reserve(m_swapchain_data.swapchain_images.size());

        VkImageCreateInfo img_info{};
        img_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        img_info.pNext = nullptr;

        img_info.imageType = VK_IMAGE_TYPE_2D;
        img_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        img_info.extent.width = m_swapchain_data.swapchain_info->imageExtent.width;
        img_info.extent.height = m_swapchain_data.swapchain_info->imageExtent.height;
        img_info.extent.depth = 1;
        img_info.mipLevels = 1;
        img_info.arrayLayers = 1;

        img_info.format = VK_FORMAT_D24_UNORM_S8_UINT;
        img_info.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        img_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        img_info.samples = VK_SAMPLE_COUNT_4_BIT;
        img_info.tiling = VK_IMAGE_TILING_OPTIMAL;

        VmaAllocationCreateInfo img_alloc_info{};
        img_alloc_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;

        errors::handle_error_code(m_main_depth_image.init(vk_utils::context::get().allocator(), &img_info, &img_alloc_info));
        img_info.format = m_swapchain_data.swapchain_info->imageFormat;
        img_info.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        errors::handle_error_code(m_main_msaa_image.init(vk_utils::context::get().allocator(), &img_info, &img_alloc_info));


        VkImageViewCreateInfo img_view_info{};
        img_view_info.image = m_main_depth_image;
        img_view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        img_view_info.pNext = nullptr;
        img_view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        img_view_info.components.r = VK_COMPONENT_SWIZZLE_R;
        img_view_info.components.g = VK_COMPONENT_SWIZZLE_G;
        img_view_info.components.b = VK_COMPONENT_SWIZZLE_B;
        img_view_info.components.a = VK_COMPONENT_SWIZZLE_A;
        img_view_info.subresourceRange.baseArrayLayer = 0;
        img_view_info.subresourceRange.layerCount = 1;
        img_view_info.subresourceRange.baseMipLevel = 0;
        img_view_info.subresourceRange.levelCount = 1;
        img_view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        img_view_info.format = VK_FORMAT_D24_UNORM_S8_UINT;

        errors::handle_error_code(m_main_depth_image_view.init(vk_utils::context::get().device(), &img_view_info));
        img_view_info.image = m_main_msaa_image;
        img_view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        img_view_info.format = m_swapchain_data.swapchain_info->imageFormat;
        errors::handle_error_code(m_main_msaa_image_view.init(vk_utils::context::get().device(), &img_view_info));

        VkFramebufferCreateInfo fb_create_info{};
        fb_create_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fb_create_info.pNext = nullptr;
        fb_create_info.renderPass = m_main_render_pass;

        fb_create_info.width = m_swapchain_data.swapchain_info->imageExtent.width;
        fb_create_info.height = m_swapchain_data.swapchain_info->imageExtent.height;
        fb_create_info.layers = 1;

        for (int i = 0; i < m_swapchain_data.swapchain_images.size(); ++i) {
            VkImageView fb_attachments[] = {
                m_swapchain_data.swapchain_images_views[i],
                m_main_depth_image_view,
                m_main_msaa_image_view};

            fb_create_info.pAttachments = fb_attachments;
            fb_create_info.attachmentCount = std::size(fb_attachments);

            auto& fb_handle = m_main_pass_framebuffers.emplace_back();
            fb_handle.init(vk_utils::context::get().device(), &fb_create_info);
        }

        RAISE_ERROR_OK();
    }

    vk_utils::obj_loader::obj_model m_model{};
    shader_group m_dummy_shader_group{};

    vk_utils::vma_buffer_handler m_vert_staging_buffer{};
    vk_utils::vma_buffer_handler m_index_staging_buffer{};

    std::vector<vk_utils::pipeline_layout_handler> m_pipelines_layout{};
    std::vector<vk_utils::graphics_pipeline_handler> m_graphics_pipelines{};
    vk_utils::vma_buffer_handler m_ubo{};
    vk_utils::cmd_pool_handler m_command_pool{};

    vk_utils::pass_handler m_main_render_pass{};
    vk_utils::vma_image_handler m_main_msaa_image{};
    vk_utils::vma_image_handler m_main_depth_image{};

    vk_utils::image_view_handler m_main_msaa_image_view{};
    vk_utils::image_view_handler m_main_depth_image_view{};

    std::vector<vk_utils::framebuffer_handler> m_main_pass_framebuffers{};

    vk_utils::cmd_buffers_handler m_command_buffers{};
};


int main(int argc, const char** argv)
{
    dummy_obj_viewer_app app{"obj_viewer", argc, argv};
    HANDLE_ERROR(app.run());

    return 0;
}