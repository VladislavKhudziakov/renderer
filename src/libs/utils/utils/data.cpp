

#include "data.hpp"


utils::data::data(
    uint8_t* data_ptr,
    size_t data_size,
    const std::function<void(uint8_t*)>& free_function)
    : m_data_handler(data_ptr, free_function)
    , m_data_size(data_size)
{
}

utils::data::data(utils::data&&) noexcept = default;


utils::data& utils::data::operator=(utils::data&&) noexcept = default;


const uint8_t* utils::data::get() const
{
    return m_data_handler.get();
}


uint8_t* utils::data::get()
{
    return m_data_handler.get();
}


size_t utils::data::get_size() const
{
    return m_data_size;
}
