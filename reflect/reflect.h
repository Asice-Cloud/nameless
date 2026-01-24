#pragma once

#include <source_location>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>
#include <any>
#include <optional>
#include <functional>
#include <unordered_map>
#include <memory>
#include <typeinfo>
#include <type_traits>
#include <cmath>

#define Nothing_TODO_With_Member  [](const std::string &name, const std::any &value, std::string_view type){}
#define Nothing_TODO_With_Function  [](const std::string&, std::string_view, size_t, const std::vector<std::string>&) {}

// member info structure
template <typename T>
struct member_info
{
    const char *name;
    T *ptr;

    member_info(const char *n, T *p) : name(n), ptr(p) {}
};

// member function info structure
template <typename FuncPtr>
struct function_info
{
    const char *name;
    FuncPtr ptr;

    function_info(const char *n, FuncPtr p) : name(n), ptr(p) {}
};

// Macros for member and function info
#define MEMBER(member) member_info(#member, &member)
#define FUNCTION(func) function_info(#func, &std::remove_reference_t<decltype(*this)>::func)

// Paser for function names to extract type information
class function_name_parser
{
public:
    static std::string extract_type_name(const std::source_location &loc)
    {
        std::string_view func_name = loc.function_name();
        size_t eq_pos = func_name.find("T = ");
        if (eq_pos != std::string_view::npos)
        {
            size_t start = eq_pos + 4;
            size_t end = func_name.find_first_of("];,)", start);
            if (end != std::string_view::npos)
            {
                std::string type_name = std::string(func_name.substr(start, end - start));
                return clean_type_name(type_name);
            }
        }
        return "unknown";
    }

    static std::string clean_type_name(const std::string &type_name)
    {
        std::string clean_name = type_name;
        if (clean_name.find("basic_string") != std::string::npos)
        {
            return "string";
        }
        if (clean_name == "int" || clean_name == "i")
            return "int";
        if (clean_name == "double" || clean_name == "d")
            return "double";
        if (clean_name == "float" || clean_name == "f")
            return "float";
        if (clean_name == "bool" || clean_name == "b")
            return "bool";
        if (clean_name == "char" || clean_name == "c")
            return "char";
        if (clean_name == "void" || clean_name == "v")
            return "void";
        return clean_name;
    }
};

// Base property class
class property_base
{
public:
    virtual ~property_base() = default;
    virtual std::any get_value() const = 0;
    virtual void set_value(const std::any &value) = 0;
    virtual std::string_view get_type_name() const = 0;
    virtual size_t get_type_hash() const = 0;
};

// Base function class
class function_base
{
public:
    virtual ~function_base() = default;
    virtual std::string_view get_name() const = 0;
    virtual std::string_view get_signature() const = 0;
    virtual std::any invoke(void *obj, const std::vector<std::any> &args) = 0;
    virtual size_t get_param_count() const = 0;
    virtual std::vector<std::string> get_param_types() const = 0;
};

// Concrete property implementation
template <typename T>
class property : public property_base
{
private:
    T *ptr_;
    std::string type_name_;

public:
    property(T *ptr, const std::source_location &loc = std::source_location::current())
        : ptr_(ptr)
    {
        type_name_ = function_name_parser::extract_type_name(loc);
        if (type_name_ == "unknown")
        {
            type_name_ = function_name_parser::clean_type_name(typeid(T).name());
        }
    }

    std::any get_value() const override
    {
        return std::any(*ptr_);
    }

    void set_value(const std::any &value) override
    {
        *ptr_ = std::any_cast<T>(value); // let expect spread to caller
    }

    std::string_view get_type_name() const override
    {
        return type_name_;
    }

    size_t get_type_hash() const override
    {
        return typeid(T).hash_code();
    }
};

// member function with variadic templates
template <typename Class, typename ReturnType, typename... Args>
class member_function : public function_base
{
private:
    std::string name_;
    std::string signature_;
    ReturnType (Class::*func_ptr_)(Args...);

    // convert to function signature string
    std::string build_signature() const
    {
        std::string sig = name_ + "(";
        if constexpr (sizeof...(Args) > 0)
        {
            auto type_names = std::vector<std::string>{
                function_name_parser::clean_type_name(typeid(Args).name())...};
            for (size_t i = 0; i < type_names.size(); ++i)
            {
                if (i > 0)
                    sig += ", ";
                sig += type_names[i];
            }
        }
        sig += ") -> " + function_name_parser::clean_type_name(typeid(ReturnType).name());
        return sig;
    }

    // extract args from std::any
    template<typename T>
    static T extract_arg(const std::any& a) {
        using base_t = std::remove_reference_t<T>;
        if constexpr (std::is_lvalue_reference_v<T>) {
            if (a.type() == typeid(std::reference_wrapper<base_t>))
                return static_cast<T>(std::any_cast<std::reference_wrapper<base_t>>(a).get());
            if (a.type() == typeid(base_t))
                return static_cast<T>(std::any_cast<base_t&>(const_cast<std::any&>(a)));
            return std::any_cast<T>(a);
        } else {
            return std::any_cast<T>(a);
        }
    }

    // invoke implementation
    template <std::size_t... I>
    std::any invoke_impl(Class *obj, const std::vector<std::any> &args, std::index_sequence<I...>)
    {
        if constexpr (std::is_void_v<ReturnType>)
        {
            (obj->*func_ptr_)(extract_arg<Args>(args[I])...);
            return std::any{};
        }
        else
        {
            return std::any((obj->*func_ptr_)(extract_arg<Args>(args[I])...));
        }
    }

public:
    member_function(const std::string &name, ReturnType (Class::*func_ptr)(Args...))
        : name_(name), func_ptr_(func_ptr)
    {
        signature_ = build_signature();
    }

    std::string_view get_name() const override { return name_; }
    std::string_view get_signature() const override { return signature_; }
    size_t get_param_count() const override { return sizeof...(Args); }

    std::vector<std::string> get_param_types() const override
    {
        if constexpr (sizeof...(Args) == 0)
        {
            return {};
        }
        else
        {
            return {function_name_parser::clean_type_name(typeid(Args).name())...};
        }
    }

    std::any invoke(void *obj, const std::vector<std::any> &args) override
    {
        if (args.size() != sizeof...(Args))
        {
            throw std::invalid_argument("Function expects " + std::to_string(sizeof...(Args)) +
                                        " arguments, got " + std::to_string(args.size()));
        }

        Class *class_obj = static_cast<Class *>(obj);
        return invoke_impl(class_obj, args, std::index_sequence_for<Args...>{});
    }
};

// const member function
template<typename Class, typename ReturnType, typename... Args>
class const_member_function : public function_base {
private:
    std::string name_;
    std::string signature_;
    ReturnType (Class::*func_ptr_)(Args...) const;

    std::string build_signature() const {
        std::string sig = name_ + "(";
        if constexpr (sizeof...(Args) > 0) {
            auto type_names = std::vector<std::string>{
                function_name_parser::clean_type_name(typeid(Args).name())...};
            for (size_t i = 0; i < type_names.size(); ++i) {
                if (i > 0)
                    sig += ", ";
                sig += type_names[i];
            }
        }
        sig += ") -> " + function_name_parser::clean_type_name(typeid(ReturnType).name());
        return sig;
    }

    template<typename T>
    static T extract_arg(const std::any& a) {
        using base_t = std::remove_reference_t<T>;
        if constexpr (std::is_lvalue_reference_v<T>) {
            if (a.type() == typeid(std::reference_wrapper<base_t>))
                return static_cast<T>(std::any_cast<std::reference_wrapper<base_t>>(a).get());
            if (a.type() == typeid(base_t))
                return static_cast<T>(std::any_cast<base_t&>(const_cast<std::any&>(a)));
            return std::any_cast<T>(a);
        } else {
            return std::any_cast<T>(a);
        }
    }

    template<std::size_t... I>
    std::any invoke_impl(const Class* obj, const std::vector<std::any>& args, std::index_sequence<I...>) {
        if constexpr (std::is_void_v<ReturnType>) {
            (obj->*func_ptr_)(extract_arg<Args>(args[I])...);
            return std::any{};
        } else {
            return std::any((obj->*func_ptr_)(extract_arg<Args>(args[I])...));
        }
    }
public:
    const_member_function(const std::string &name, ReturnType (Class::*func_ptr)(Args...) const)
        : name_(name), func_ptr_(func_ptr) {
        signature_ = build_signature();
    }


    std::string_view get_name() const override { return name_; }
    std::string_view get_signature() const override { return signature_; }
    size_t get_param_count() const override { return sizeof...(Args); }

    std::vector<std::string> get_param_types() const override {
        if constexpr (sizeof...(Args) == 0) {
            return {};
        } else {
            return {function_name_parser::clean_type_name(typeid(Args).name())...};
        }
    }

    std::any invoke(void* obj, const std::vector<std::any>& args) override {
        if (args.size() != sizeof...(Args)) {
            throw std::invalid_argument("Function expects " + std::to_string(sizeof...(Args)) +
                                        " arguments, got " + std::to_string(args.size()));
        }

        const Class* class_obj = static_cast<const Class*>(obj);
        return invoke_impl(class_obj, args, std::index_sequence_for<Args...>{});
    }
};

// Base reflected object class
class reflected_object
{
private:
    std::unordered_map<std::string, std::unique_ptr<property_base>> properties_;
    std::unordered_map<std::string, std::unique_ptr<function_base>> functions_;

    // recursive variadic template to set properties
    template <typename T, typename... Rest>
    void set_properties_impl(const std::string &name, T &&value, Rest &&...rest)
    {
        set_property(name, std::forward<T>(value));
        if constexpr (sizeof...(rest) > 0)
        {
            set_properties_impl(std::forward<Rest>(rest)...);
        }
    }

protected:
    template <typename T>
    void register_member(const std::string &name, T *ptr)
    {
        properties_[name] = std::make_unique<property<T>>(ptr);
    }

    // register member function
    template <typename Class, typename ReturnType, typename... Args>
    void register_function(const std::string &name, ReturnType (Class::*func_ptr)(Args...))
    {
        functions_[name] = std::make_unique<member_function<Class, ReturnType, Args...>>(name, func_ptr);
    }

    //overload
    template<typename Class, typename ReturnType, typename... Args>
    void register_function(const std::string &name, ReturnType (Class::*func_ptr)(Args...) const) {
        functions_[name] = std::make_unique<const_member_function<Class, ReturnType, Args...>>(name, func_ptr);
    }

    // register all
    template <typename... Members>
    void register_all_members(Members... members)
    {
        // fold expression (C++17)
        (register_member_helper(members), ...);
    }

    // register all functions
    template <typename... Functions>
    void register_all_functions(Functions... functions)
    {
        (register_function_helper(functions), ...);
    }

private:
    // member registration helper
    template <typename T>
    void register_member_helper(T &&member_info)
    {
        register_member(member_info.name, member_info.ptr);
    }

    template <typename T>
    void register_function_helper(T &&func_info)
    {
        register_function(func_info.name, func_info.ptr);
    }

public:
    std::optional<std::any> get_property(const std::string &name) const
    {
        auto it = properties_.find(name);
        if (it != properties_.end())
        {
            return it->second->get_value();
        }
        return std::nullopt;
    }

    bool set_property(const std::string &name, const std::any &value)
    {
        auto it = properties_.find(name);
        if (it != properties_.end())
        {
            try
            {
                it->second->set_value(value);
                return true;
            }
            catch (const std::bad_any_cast &)
            {
                std::cout << "Type conversion failed: bad any_cast" << std::endl;
                return false;
            }
        }
        return false;
    }

    // Get all names
    std::vector<std::string> get_property_names() const
    {
        std::vector<std::string> names;
        for (const auto &[name, _] : properties_)
        {
            names.push_back(name);
        }
        return names;
    }

    // Get type info
    std::string_view get_property_type(const std::string &name) const
    {
        auto it = properties_.find(name);
        if (it != properties_.end())
        {
            return it->second->get_type_name();
        }
        return "unknown";
    }

    // visit member
    template <typename Visitor>
    void visit_members(Visitor &&visitor) const
    {
        for (const auto &[name, property] : properties_)
        {
            visitor(name, property->get_value(), property->get_type_name());
        }
    }

    // visit all
    template <typename PropertyVisitor, typename FunctionVisitor>
    void visit_all_members(PropertyVisitor &&prop_visitor, FunctionVisitor &&func_visitor) const
    {
        for (const auto &[name, property] : properties_)
        {
            prop_visitor(name, property->get_value(), property->get_type_name());
        }

        for (const auto &[name, function] : functions_)
        {
            func_visitor(name, function->get_signature(), function->get_param_count(), function->get_param_types());
        }
    } 
    template <typename... Args>
    void set_properties_variadic(Args &&...args)
    {
        static_assert(sizeof...(args) % 2 == 0, "Arguments must come in name-value pairs");
        set_properties_impl(std::forward<Args>(args)...);
    }

    void set_properties(const std::unordered_map<std::string, std::any> &props)
    {
        for (const auto &[name, value] : props)
        {
            set_property(name, value);
        }
    }

    void set_properties(const std::vector<std::pair<std::string, std::any>> &props)
    {
        for (const auto &[name, value] : props)
        {
            set_property(name, value);
        }
    }

    std::unordered_map<std::string, std::any> get_all_properties() const
    {
        std::unordered_map<std::string, std::any> result;
        for (const auto &[name, property] : properties_)
        {
            result[name] = property->get_value();
        }
        return result;
    }

    size_t property_count() const
    {
        return properties_.size();
    }

    bool has_property(const std::string &name) const
    {
        return properties_.find(name) != properties_.end();
    }

    std::any call_function(const std::string &name, const std::vector<std::any> &args = {})
    {
        auto it = functions_.find(name);
        if (it != functions_.end())
        {
            try
            {
                return it->second->invoke(this, args);
            }
            catch (const std::exception &e)
            {
                std::cout << "Function call failed: " << e.what() << std::endl;
                throw;
            }
        }
        throw std::runtime_error("Function '" + name + "' not found");
    }

    std::vector<std::string> get_function_names() const
    {
        std::vector<std::string> names;
        for (const auto &[name, _] : functions_)
        {
            names.push_back(name);
        }
        return names;
    }

    std::string_view get_function_signature(const std::string &name) const
    {
        auto it = functions_.find(name);
        if (it != functions_.end())
        {
            return it->second->get_signature();
        }
        return "unknown";
    }

    bool has_function(const std::string &name) const
    {
        return functions_.find(name) != functions_.end();
    }

    size_t get_function_param_count(const std::string &name) const
    {
        auto it = functions_.find(name);
        if (it != functions_.end())
        {
            return it->second->get_param_count();
        }
        return 0;
    }

    std::vector<std::string> get_function_param_types(const std::string &name) const
    {
        auto it = functions_.find(name);
        if (it != functions_.end())
        {
            return it->second->get_param_types();
        }
        return {};
    }

    // print reflection info
    void print_reflection_info() const
    {
        std::cout << "=== Reflection Info ===\n";

        std::cout << "Properties:\n";
        for (const auto &prop_name : get_property_names())
        {
            auto value_opt = get_property(prop_name);
            auto type_name = get_property_type(prop_name);

            std::cout << "  " << prop_name << " (" << type_name << "): ";

            if (value_opt)
            {
                const auto &value = *value_opt;
                if (value.type() == typeid(int))
                {
                    std::cout << std::any_cast<int>(value);
                }
                else if (value.type() == typeid(std::string))
                {
                    std::cout << "\"" << std::any_cast<std::string>(value) << "\"";
                }
                else if (value.type() == typeid(double))
                {
                    std::cout << std::any_cast<double>(value);
                }
                else if (value.type() == typeid(float))
                {
                    std::cout << std::any_cast<float>(value);
                }
                else if (value.type() == typeid(bool))
                {
                    std::cout << (std::any_cast<bool>(value) ? "true" : "false");
                }
                else
                {
                    std::cout << "unknown type";
                }
            }
            else
            {
                std::cout << "null";
            }
            std::cout << "\n";
        }

        // print functions
        auto function_names = get_function_names();
        if (!function_names.empty())
        {
            std::cout << "Functions:\n";
            for (const auto &func_name : function_names)
            {
                std::cout << "  " << get_function_signature(func_name) << "\n";
            }
        }
    }
};

// Get type name
template <typename T>
std::string get_type_name(const std::source_location &loc = std::source_location::current())
{
    return function_name_parser::extract_type_name(loc);
}

// print source location
inline void print_source_location(const std::source_location &loc = std::source_location::current())
{
    std::cout << "=== Source Location ===\n";
    std::cout << "File: " << loc.file_name() << "\n";
    std::cout << "Function: " << loc.function_name() << "\n";
    std::cout << "Line: " << loc.line() << "\n";
    std::cout << "Column: " << loc.column() << "\n";
}

// Macros
#define REGISTER_MEMBER(member) register_member(#member, &member)
#define REGISTER_FUNCTION(func) register_function(#func, &std::remove_reference_t<decltype(*this)>::func)

// Register multiple members/functions
#define REGISTER_MEMBERS(...)              \
    do                                     \
    {                                      \
        register_all_members(__VA_ARGS__); \
    } while (0)

#define REGISTER_FUNCTIONS(...)              \
    do                                       \
    {                                        \
        register_all_functions(__VA_ARGS__); \
    } while (0)

