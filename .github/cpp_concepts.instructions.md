---
applyTo: '*.cpp, *.hpp'
---
- Always focus on low execution time and low memory consumption
- Always use cstdint type like uint32_t, int16_t, etc. Do not use standard types like int, long, short, etc.
- The code shall use constexpr wherever possible to save RAM
- The code shall not use dynamic memory allocation (new, delete, malloc, free, etc.)
- Dont use '!' in conditions, instead use positive logic. For example, instead of `if (!condition)`, write `if (condition == false)` or `if (condition == true)` depending on the context. This improves readability and reduces the chance of mistakes when negating conditions.
- The code shall not use exceptions (try, catch, throw)
- The code shall not use RTTI (Run-Time Type Information)
- The code shall use the 'override' specifier for all overridden virtual functions
- The code shall use the 'final' specifier for classes that are not intended to be inherited from
- The code shall use 'nullptr' instead of 'NULL' or '0' for pointer
- The code shall use the Configuration concept to save memory. Configuration is a nested constexpr struct that holds all configuration parameters for a class, including pointers/references to other objects, constant config values, pointers o parameter functions/lambdas, etc. The configuration struct shall be passed to the class constructor as a const reference. It shall always be called "Configuration". Example:
```cpp
class CMyClass {
public:
    struct Configuration {
        constexpr Configuration(int v1, ObjectType1 obj1, ObjectType2& obj2, const uint32_t& toBeTestedVal, void (*callback)(int))
            : value1(v1), object1(obj1), object2(obj2), toBeTestedValue(toBeTestedVal), callbackFunction(callback) {}
        int value1;
        ObjectType1 object1;
        ObjectType2& object2;
        const uint32_t& toBeTestedValue;
        void (*callbackFunction)(int);
    };
    CMyClass(const Configuration& config) : config_(config) {}
private:
    const Configuration& config_;
};
```
- When a derived class extends a base class, the Configuration struct of the derived class shall inherit from the base class Configuration struct. This allows seamless initialization of both base and derived configurations. The derived class constructor passes its Configuration (which is-a base Configuration) directly to the base class constructor. Example:
```cpp
class CBaseClass {
public:
    struct Configuration {
        constexpr Configuration(int base_value) : base_value(base_value) {}
        int base_value;
    };
    explicit CBaseClass(const Configuration& config) : config_(config) {}
protected:
    const Configuration& config_;
};

class CDerivedClass : public CBaseClass {
public:
    struct Configuration : public CBaseClass::Configuration {
        constexpr Configuration(int base_value, int derived_value)
            : CBaseClass::Configuration(base_value), derived_value(derived_value) {}
        int derived_value;
    };
    explicit CDerivedClass(const Configuration& config) : CBaseClass(config) {}
};
```
- The code shall avoid using macros (#define) wherever possible. Prefer constexpr variables, inline functions, or templates instead.
- The code shall always be loosely coupled and highly cohesive. Use function pointers, lambdas, to decouple classes and enable easier unit testing and mocking. Example:
```cpp
//class
class CMyClass {
public:
    struct Configuration {
        constexpr Configuration(void (*callback)(int), bool& some_global_variable) : callbackFunction(callback), someGlobalVariable(some_global_variable) {}
        void (*callbackFunction)(int);
        bool& someGlobalVariable;
    };
    CMyClass(const Configuration& config) : config_(config) {}
    void DoSomething() {
        // ...
        config_.callbackFunction(42); // call the callback function
        config_.someGlobalVariable = true; // modify the global variable through the reference
    }
private:
    const Configuration& config_;
};
//instantiation
bool some_global_variable = false;
CMyClass::Configuration config([](int value) {
    // this lambda can capture variables from the surrounding scope if needed
    std::cout << "Callback called with value: " << value << std::endl;
}, some_global_variable);
CMyClass myObject(config);
``` 
- do not use interfaces and virtual functions at all
- The code shall use the 'explicit' specifier for single-argument constructors to prevent implicit conversions.
- When a class is written that accesses hardware registers, the class shall be designed to allow mocking of the hardware access. Its is allowed to use the register definition file, but should have a pointer to the register structure in its configuration, so that in unit tests a mock register structure can be passed. Example:
```cpp
struct HW_Registers {
    volatile uint32_t REG1;
    volatile uint32_t REG2;
    // ...
};
class CHardwareAccess {
public:
    struct Configuration {
        constexpr Configuration(HW_Registers& regs) : registers(regs) {}
        HW_Registers& registers;
    };
    explicit CHardwareAccess(const Configuration& config) : config_(config) {}
    void WriteReg1(uint32_t value) {
        config_.registers->REG1 = value;
    }
    uint32_t ReadReg2() const {
        return config_.registers->REG2;
    }
};
```
- Use templates only if needed, try to avoid code bloat.
- use this header comment for each file (brief and file tags must be filled out):
```cpp
/**
 * \brief           Description of the file's purpose
 * \compiler        ARM GCC
 *
 * \file            {Filename without path}  
 * \copyright       COPYRIGHT (C) ERMA Group s.r.o.
 * \copyright       ALL RIGHTS RESERVED
 * \copyright       The copying, use, distribution or disclosure of the
 *                  confidential and proprietary information contained in this
 *                  document(s) is strictly prohibited without prior written
 *                  consent. Any breach shall subject the infringing party to
 *                  remedies. The owner reserves all rights in the event of the
 *                  grant of a patent or the registration of a utility model
 *                  or design.
 */
```
- each function is only allowed to have one return at the end of the function
- when a class is called CMyClass, then the file name shall be c_my_class.cpp and c_my_class.hpp
- constants shall always be part of the class as static constexpr members, not as global constants
- whenever possible, add the initialization of a member directly to its declaration, not to the constructor's initialization list.
