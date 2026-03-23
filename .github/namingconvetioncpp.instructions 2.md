---
applyTo: '*.cpp, *.hpp'
---
Provide project context and coding guidelines that AI should follow when generating code, answering questions, or reviewing changes.

- if code with other rules exists, use the existing rules
- if no code with other rules exists, use the following rules:
- all variable names shall be in snake_case with a leading lowercase letter
- all class and struct names shall be in CamelCase with a leading uppercase letter
- Classes shall have an uppercase C as a prefix
- all function names shall be in CamelCase with a leading lowercase letter
- all constants shall have the format kConstantName and be in CamelCase with a k and a leading uppercase letter afterwards
- all enumeration names shall be in CamelCase with a leading uppercase letter
- all enumeration values shall have the format kEnumValue and be in CamelCase with a k and a leading uppercase letter afterwards
- all private variables shall have a a underscore at the end of the name
- do not use prefixes like gu16_, lu16_, g_ and others for any variable, class, function, constant, enumeration, or object
- example class name `MyClass`
- example function name `myFunction`
- example variable name `my_variable`
- example constant name `kMyConstant`
- example enumeration name `MyEnum`
- example enumeration value `kMyEnumValue`
- example private variable name `my_variable_`
- example interface name `IMyInterface`
- example abstract class name `AAbstractClass`
- example object name `my_object`
