#include "reflect.h"
#include <string>


class node : public reflected_object
{
public:
    int value;
    std::string name;
    double ratio;
    bool active;

    node(int v = 0) : value(v), name("default"), ratio(1.0), active(true)
    {
        REGISTER_MEMBER(value);
        REGISTER_MEMBER(name);
        REGISTER_MEMBER(ratio);
        REGISTER_MEMBER(active);

        REGISTER_FUNCTION(process);      // 0 
        REGISTER_FUNCTION(get_info);     // 0 
        REGISTER_FUNCTION(set_value);    // 1 
        REGISTER_FUNCTION(calculate);    // 2 
        REGISTER_FUNCTION(complex_calc); // 5 
    }

    void print_source_location(const std::source_location &loc = std::source_location::current())
    {
        ::print_source_location(loc);
    }

    void process()
    {
        std::cout << "Processing node: " << name << " with value: " << value << std::endl;
    }

    std::string get_info()
    {
        return "Node '" + name + "' has value " + std::to_string(value) +
               ", ratio " + std::to_string(ratio) +
               ", active: " + (active ? "true" : "false");
    }

    void set_value(int new_value)
    {
        std::cout << "Setting value from " << value << " to " << new_value << std::endl;
        value = new_value;
    }

    double calculate(double multiplier, double offset)
    {
        double result = value * multiplier * ratio + offset;
        std::cout << "Calculation: " << value << " * " << multiplier << " * " << ratio
                  << " + " << offset << " = " << result << std::endl;
        return result;
    }

    std::string complex_calc(int base, double factor, const std::string &prefix, bool round_result, float precision)
    {
        double result = (value + base) * factor * ratio;
        if (round_result)
        {
            result = std::round(result * precision) / precision;
        }

        std::string output = prefix + "_" + std::to_string(result);
        std::cout << "Complex calculation with 5 parameters: " << output << std::endl;
        return output;
    }
};

class person : public reflected_object {
  public:
    std::string name;
    int age;
    double height;
    bool is_employed;

    person(const std::string &n = "Unknown", int a = 0)
        : name(n), age(a), height(170.0), is_employed(false) {
        REGISTER_MEMBERS(MEMBER(name), MEMBER(age), MEMBER(height),
                         MEMBER(is_employed));
        REGISTER_FUNCTIONS(FUNCTION(introduce), FUNCTION(get_info),
                           FUNCTION(set_age), FUNCTION(calculate_bmi));
    }

    void introduce() {
        std::cout << "Hi, I'm " << name << ", " << age << " years old, "
                  << height << "cm tall, "
                  << (is_employed ? "employed" : "unemployed") << std::endl;
    }

    std::string get_info() {
        return name + " (Age: " + std::to_string(age) +
               ", Height: " + std::to_string(height) + "cm)";
    }

    void set_age(int new_age) {
        std::cout << "Age updated: " << age << " -> " << new_age << std::endl;
        age = new_age;
    }

    double calculate_bmi(double weight, bool use_metric = true) {
        double height_m = use_metric ? height / 100.0 : height; // 转换为米
        double bmi = weight / (height_m * height_m);
        std::cout << "BMI calculation: " << weight << "kg / (" << height_m
                  << "m)² = " << bmi << std::endl;
        return bmi;
    }
};

class variadic_demo : public reflected_object {
  public:
    std::string name;
    int value;
    double ratio;

    variadic_demo(const std::string &n = "demo", int v = 42)
        : name(n), value(v), ratio(1.0) {
        REGISTER_MEMBER(name);
        REGISTER_MEMBER(value);
        REGISTER_MEMBER(ratio);

        REGISTER_FUNCTION(func0); // 0 
        REGISTER_FUNCTION(func1); // 1 
        REGISTER_FUNCTION(func2); // 2 
        REGISTER_FUNCTION(func3); // 3 
        REGISTER_FUNCTION(func4); // 4 
        REGISTER_FUNCTION(func5); // 5 
    }

    void func0() { std::cout << "func0() - no parameters" << std::endl; }

    int func1(int x) {
        std::cout << "func1(" << x << ") - returns " << (x * 2) << std::endl;
        return x * 2;
    }

    std::string func2(int x, const std::string &s) {
        std::string result = s + "_" + std::to_string(x);
        std::cout << "func2(" << x << ", \"" << s << "\") - returns \""
                  << result << "\"" << std::endl;
        return result;
    }

    double func3(int a, double b, bool c) {
        double result = c ? (a + b) : (a - b);
        std::cout << "func3(" << a << ", " << b << ", "
                  << (c ? "true" : "false") << ") - returns " << result
                  << std::endl;
        return result;
    }

    void func4(int a, double b, const std::string &c, bool d) {
        std::cout << "func4(" << a << ", " << b << ", \"" << c << "\", "
                  << (d ? "true" : "false") << ")";
        if (d) {
            std::cout << " - processed: " << c << "_" << (a + b);
        }
        std::cout << std::endl;
    }

    std::string func5(int a, double b, const std::string &c, bool d, float e) {
        std::string result = c + "_" + std::to_string(a) + "_" +
                             std::to_string(b) + "_" + std::to_string(e);
        if (d)
            result += "_enabled";
        std::cout << "func5(5 params) - returns \"" << result << "\""
                  << std::endl;
        return result;
    }
};

void print_separator(const std::string &title) {
    std::cout << "\n" << std::string(50, '=') << std::endl;
    std::cout << "=== " << title << " ===" << std::endl;
    std::cout << std::string(50, '=') << std::endl;
}

int main() {
    std::cout << "🚀 C++ Variadic Template Reflection System Demo 🚀"
              << std::endl;

    print_separator("Basic Reflection Features");

    person p("Alice", 25);
    p.print_reflection_info();

    std::cout << "\n--- Property Modification ---" << std::endl;
    p.set_property("name", std::string("Bob"));
    p.set_property("age", 30);
    p.set_property("is_employed", true);

    std::cout << "\nAfter modifications:" << std::endl;
    std::cout << "Name: " << std::any_cast<std::string>(*p.get_property("name"))
              << std::endl;
    std::cout << "Age: " << std::any_cast<int>(*p.get_property("age"))
              << std::endl;
    std::cout << "Employed: "
              << (std::any_cast<bool>(*p.get_property("is_employed")) ? "Yes"
                                                                      : "No")
              << std::endl;

    std::cout << "\n--- Function Calls ---" << std::endl;
    p.call_function("introduce");
    auto info = p.call_function("get_info");
    std::cout << "Info: " << std::any_cast<std::string>(info) << std::endl;

    p.call_function("set_age", {35});
    auto bmi = p.call_function("calculate_bmi", {70.5, true});
    std::cout << "BMI: " << std::any_cast<double>(bmi) << std::endl;

    print_separator("Variadic Template Functions (0-5 Parameters)");

    variadic_demo demo("test_obj", 100);
    demo.print_reflection_info();

    std::cout << "\n--- Testing All Parameter Counts ---" << std::endl;

    try {
        std::cout << "🔹 0 parameters: ";
        demo.call_function("func0");

        std::cout << "🔹 1 parameter: ";
        auto result1 = demo.call_function("func1", {42});

        std::cout << "🔹 2 parameters: ";
        auto result2 = demo.call_function("func2", {123, std::string("hello")});

        std::cout << "🔹 3 parameters: ";
        auto result3 = demo.call_function("func3", {10, 3.14, true});

        std::cout << "🔹 4 parameters: ";
        demo.call_function("func4", {5, 2.5, std::string("test"), true});

        std::cout << "🔹 5 parameters: ";
        auto result5 = demo.call_function(
            "func5", {7, 1.5, std::string("complex"), false, 9.9f});
    } catch (const std::exception &e) {
        std::cerr << "❌ Error: " << e.what() << std::endl;
    }

    print_separator("Built-in Node Class Demo");

    node n(42);
    n.name = "demo_node";
    n.ratio = 1.5;
    n.active = true;

    n.print_reflection_info();

    std::cout << "\n--- Node Function Tests ---" << std::endl;
    try {
        n.call_function("process");

        auto node_info = n.call_function("get_info");
        std::cout << "Node info: " << std::any_cast<std::string>(node_info)
                  << std::endl;

        n.call_function("set_value", {200});

        auto calc_result = n.call_function("calculate", {2.0, 50.0});
        std::cout << "Calculation result: "
                  << std::any_cast<double>(calc_result) << std::endl;

        auto complex_result =
            n.call_function("complex_calc",
                            {
                                25,                    // int base
                                1.2,                   // double factor
                                std::string("result"), // string prefix
                                true,                  // bool round_result
                                10.0f                  // float precision
                            });
        std::cout << "Complex result: \""
                  << std::any_cast<std::string>(complex_result) << "\""
                  << std::endl;
    } catch (const std::exception &e) {
        std::cerr << "❌ Error: " << e.what() << std::endl;
    }

    print_separator("Visitor Pattern Demo");

    std::cout << "Using visit_all_members to inspect person object:"
              << std::endl;
    p.visit_all_members(
        [](const std::string &name, const std::any &value,
           std::string_view type) {
            std::cout << "  📋 [Property] " << name << " (" << type << ") = ";
            if (type == "int") {
                std::cout << std::any_cast<int>(value);
            } else if (type == "string") {
                std::cout << "\"" << std::any_cast<std::string>(value) << "\"";
            } else if (type == "double") {
                std::cout << std::any_cast<double>(value);
            } else if (type == "bool") {
                std::cout << (std::any_cast<bool>(value) ? "true" : "false");
            } else {
                std::cout << "unknown";
            }
            std::cout << std::endl;
        },
        [](const std::string &name, std::string_view signature,
           size_t param_count, const std::vector<std::string> &) {
            std::cout << "  [Function] " << name << " -> " << signature
                      << " (params: " << param_count << ")" << std::endl;
        });


    return 0;
}
