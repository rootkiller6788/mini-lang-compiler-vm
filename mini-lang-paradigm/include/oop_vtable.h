#ifndef OOP_VTABLE_H
#define OOP_VTABLE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define OOP_MAX_METHODS     64
#define OOP_MAX_NAME_LEN    64
#define OOP_MAX_FIELDS      32
#define OOP_MAX_INHERITANCE_DEPTH 16

typedef void* (*OopMethodFn)(void* self, void** args);

typedef struct {
    char name[OOP_MAX_NAME_LEN];
    int  arity;
    OopMethodFn fn_ptr;
} Method;

typedef struct Class Class;
struct Class {
    char   name[OOP_MAX_NAME_LEN];
    Method vtable[OOP_MAX_METHODS];
    int    vtable_size;
    Class* parent_class;
    size_t instance_size;
    int    num_fields;
    char   field_names[OOP_MAX_FIELDS][OOP_MAX_NAME_LEN];
};

typedef struct {
    Class*  class;
    void*   fields[OOP_MAX_FIELDS];
} Object;

Class*  class_create(const char* name, size_t instance_size);
void    class_add_method(Class* cls, const char* name, int arity, OopMethodFn fn);
int     class_method_index(const Class* cls, const char* name);
Class*  class_inherit(const char* name, const Class* parent);
void    class_override_method(Class* cls, const char* name, OopMethodFn fn);
Object* object_create(const Class* cls);
void*   object_call_virtual(Object* obj, const char* method_name, void** args);
void    object_set_field(Object* obj, int index, void* value);
void*   object_get_field(Object* obj, int index);
void    oop_print_class_hierarchy(const Class* cls, int depth);
void    oop_print_vtable(const Class* cls);
void    class_destroy(Class* cls);
void    object_destroy(Object* obj);

#endif
