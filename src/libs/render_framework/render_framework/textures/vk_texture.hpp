#pragma once

#include <render_framework/textures/texture.hpp>

#include <vk_utils/handlers.hpp>


namespace render_framework
{
    namespace detail
    {
        class vk_texture_impl : public texture_impl
        {
        public:
            vk_texture_impl(
                vk_utils::vma_image_handler image,
                vk_utils::image_view_handler image_view,
                vk_utils::sampler_handler image_sampler);

            ~vk_texture_impl() override = default;

            VkImage get_image() const;
            VkImageView get_image_view() const;
            VkSampler get_sampler() const;

        private:
            vk_utils::vma_image_handler m_image;
            vk_utils::image_view_handler m_image_view;
            vk_utils::sampler_handler m_image_sampler;
        };
    }

    class vk_texture_builder : public texture_builder
    {
    public:
        explicit vk_texture_builder(uint32_t queue_family);

        vk_texture_builder& set_command_pool(VkCommandPool command_pool);
        vk_texture_builder& set_queue(VkQueue queue);
        vk_texture_builder& set_command_buffer(VkCommandBuffer command_buffer);
        vk_texture_builder& set_queue_family(uint32_t queue_family);

        ERROR_TYPE create(texture& result) override;

    protected:
        virtual ERROR_TYPE create_vk_texture(
          vk_utils::vma_image_handler& image,
          vk_utils::image_view_handler& image_view,
          vk_utils::sampler_handler& image_sampler,
          uint32_t queue_family, 
          VkCommandBuffer command_buffer) = 0;
    
    private:
        VkCommandPool m_command_pool{nullptr};
        VkQueue m_queue{nullptr};
        VkCommandBuffer m_command_buffer{nullptr};
        uint32_t m_queue_family = -1;
    };


    class vk_ktx_texture_builder : public vk_texture_builder
    {
        
    };
     

    class vk_stb_texture_builder : public vk_texture_builder
    {
    };
};

