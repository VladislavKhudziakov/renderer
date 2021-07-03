#include "parameter.hpp"


using namespace render_framework;


parameter::parameter(value_type value_type, size_t elements_count)
    : m_value_type(value_type)
    , m_elements_count(elements_count)
{
}


void parameter::subscribe_onupdate_listenner(parameter_onupdate_listenner* listenner)
{
    m_parameter_update_listenners_list.push_back(listenner);
}


void parameter::unsubscribe_onupdate_listenner(const parameter_onupdate_listenner* listenner)
{
    m_parameter_update_listenners_list.erase(
      std::remove(m_parameter_update_listenners_list.begin(), m_parameter_update_listenners_list.end(), listenner), 
      m_parameter_update_listenners_list.end());
}


void parameter::load_value(const uint8_t* value)
{
    onupdate(value);
}


parameter::value_type parameter::get_value_type() const
{
    return m_value_type;
}


size_t parameter::get_elements_count() const
{
    return m_elements_count;
}


void parameter::onupdate(const uint8_t* value)
{ 
    for (auto* listenner : m_parameter_update_listenners_list) {
        listenner->on_parameter_updated(this, value);
    }
}


parameter_builder& parameter_builder::set_value_type(parameter::value_type value_type)
{
    m_value_type = value_type;
    return *this;
}


parameter_builder& parameter_builder::set_elements_count(size_t count)
{
    m_elements_count = count;
    return *this;
}


ERROR_TYPE parameter_builder::create(parameter& res)
{
    if (m_elements_count == 0) {
        RAISE_ERROR_WARN(-1, "bad elements count.");
    }

    if (m_value_type == parameter::value_type::empty) {
        RAISE_ERROR_WARN(-1, "bad parameter value.");
    }

    res = parameter(m_value_type, m_elements_count);

    RAISE_ERROR_OK();
}


parameters_list_builder& parameters_list_builder::add_parameter(parameter& new_paramters)
{
    m_parameters_list.push_back(&new_paramters);
    return *this;
}


parameters_list::parameters_list() = default;


parameters_list::~parameters_list() = default;


parameters_list::parameters_list(parameters_list&&) noexcept = default;


parameters_list& parameters_list::operator=(parameters_list&&) noexcept = default;