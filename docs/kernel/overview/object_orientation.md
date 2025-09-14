# Object Orientation in the Kernel

Some objects (like [devices](../devices/devices.md)) use object orientation to inherit from common base classes. All objects derived from `kobject` are managed in a tree.

## Methods

Class methods are stored in virtual function tables named `struct_name_` + `ops` (this can be found in Linux as well).

## Derived objects

Assuming the derived struct always places the base struct as the first member, any pointer to the derived type can be cased to a pointer of the base to access the members. But this insn't very robust and save.

```C
struct Base
{
	// Base members and methods
};

struct Derived_Class
{
	Base base;
	// additional members and methods
};
```

Using compiler tricks, the offset of the base struct inside of the derived struct can be found (instead of assuming it to be zero and just doing a cast). With this offset, the base can be cast safely into the derived class even if it was not the first member. This is done in the macro `container_of` which can also be found in the Linux kernel.

Objects often define a macro based on `container_of`: `derived_from_base(base *)`.

---
**Overview:** [kernel](../kernel.md)

**Related:** [common_objects](common_objects.md) | [object_orientation](object_orientation.md)
