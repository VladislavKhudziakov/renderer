#pragma once

#include <memory>
#include <functional>

namespace utils
{
    class data
    {
    public:
        data() = default;
        data(uint8_t* data_ptr, size_t data_size, const std::function<void(uint8_t*)>&);

        data(data&&) noexcept;
        data& operator=(data&&) noexcept;

        const uint8_t* get() const;
        uint8_t* get();
        size_t get_size() const;

    private:
        std::unique_ptr<uint8_t, std::function<void(uint8_t*)>> m_data_handler{nullptr, nullptr};
        size_t m_data_size;
    };
}


