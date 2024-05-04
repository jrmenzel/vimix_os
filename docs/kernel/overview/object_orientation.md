# Object Orientation in the Kernel

## Methods

Class methods are stored in virtual function tables named `struct_name_` + `ops` (this can be found in Linux as well).

## Derived objects

Assuming the derived struct always places the base struct as the first member, any pointer to the derived type can be cased to a pointer of the base to access the members.

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

Using compiler tricks, the offset of the base struct inside of the derived struct can be found. With it, the base can be "cast" into the derived class even if it was not the first member. This is done in the macro `container_of` which can also be found in the Linux kernel.


## Used where

[devices](../devices/devices.md)
