#pragma once

#include <memory>

namespace render_framework
{
    template <typename AssetType, typename ImplType>
    class asset
    {
    public:
        template <typename T, typename... Args>
        static AssetType create(Args&& ...args)
        {
            AssetType asset{};
            asset.m_impl = std::make_unique<T>(std::forward<Args>(args)...);
            return asset;
        }

        asset() = default;
        asset(asset&& other) noexcept = default;
        asset& operator=(asset&& other) noexcept = default;
        virtual ~asset() = default;

        ImplType* get_impl()
        {
            m_impl.get();
        }

        const ImplType* get_impl() const
        {
            m_impl.get();
        }

    protected:
        std::unique_ptr<ImplType> m_impl;
    };
}
