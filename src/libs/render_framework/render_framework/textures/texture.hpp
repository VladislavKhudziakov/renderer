#pragma once

#include <render_framework/asset.hpp>
#include <errors/error_handler.hpp>

#include <cinttypes>

namespace render_framework
{
    class texture_impl
    {
    public:
        virtual ~texture_impl() = default;
        virtual uint64_t get_view() const = 0;
        virtual uint64_t get_sampler() const = 0;
    };

    class texture : public asset<texture, texture_impl>
    {
    public:
        texture();
        texture(texture&& other) noexcept;
        texture& operator=(texture&& other) noexcept;

        virtual ~texture() override;

        virtual uint64_t get_view() const;
        virtual uint64_t get_sampler() const;
    };

    class texture_builder
    {
    public:
        enum class texture_filtering
        {
            point,
            linear,
            bilinear,
            anizotropic
        };

        enum class texture_address_mode
        {
            repeat,
            mirror_repeat,
            clamp_to_egde,
            clamp_to_border,
            mirror_clamp_to_edge
        };

        virtual ~texture_builder() = default;

        texture_builder* set_filtering(texture_filtering filterind);
        texture_builder* set_address_mode(texture_address_mode address_mode);

        virtual ERROR_TYPE create(texture& result) = 0;

    private:
        texture_filtering m_filtering = texture_filtering::point;
        texture_address_mode m_address_mode = texture_address_mode::clamp_to_egde;
    };
}

