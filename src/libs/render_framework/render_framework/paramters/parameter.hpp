#pragma once

#include <render_framework/asset.hpp>

#include <errors/error_handler.hpp>

#include <vector>


namespace render_framework
{
    class parameter;
     

    class parameter_onupdate_listenner
    {
    public:
        virtual ~parameter_onupdate_listenner() = default;
        virtual void on_parameter_updated(const parameter* parameter, const uint8_t* value) = 0;
    };
    

    class parameter 
    {
        friend class parameter_builder;

    public:
        enum class value_type
        {
            empty,
            int32,
            float32,
            vec4,
            mat4
        };

        parameter() = default;

        void subscribe_onupdate_listenner(parameter_onupdate_listenner*);
        void unsubscribe_onupdate_listenner(const parameter_onupdate_listenner*);
        
        void load_value(const uint8_t* value);

        value_type get_value_type() const;
        size_t get_elements_count() const;

    private:
        parameter(value_type value_type, size_t elements_count);
        void onupdate(const uint8_t* value);

        std::vector<parameter_onupdate_listenner*> m_parameter_update_listenners_list;
        value_type m_value_type = value_type::empty;
        size_t m_elements_count{0};
    };


    class parameter_builder
    {
    public:
        virtual ~parameter_builder() = default;
        parameter_builder* set_value_type(parameter::value_type);
        parameter_builder* set_elements_count(size_t);

        ERROR_TYPE create(parameter& res);

    protected:
        parameter::value_type m_value_type = parameter::value_type::vec4;
        size_t m_elements_count{1};
    };


    class parameters_list_impl : public parameter_onupdate_listenner
    {
    public:
        virtual ~parameters_list_impl() = default;
    };


    class parameters_list : public asset<parameters_list, parameters_list_impl>
    {
    public:
        parameters_list();
        ~parameters_list() override;
        parameters_list(parameters_list&&) noexcept;
        parameters_list& operator=(parameters_list&&) noexcept;
    };


    class parameters_list_builder
    {
    public:
        parameters_list_builder* add_parameter(parameter&);
        virtual ERROR_TYPE create(parameters_list&) = 0;

    protected:
        std::vector<parameter*> m_parameters_list;
    };
}
