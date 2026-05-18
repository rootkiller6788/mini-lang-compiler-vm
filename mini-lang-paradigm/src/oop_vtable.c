#include "oop_vtable.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

Class* class_create(const char* name, size_t instance_size) {
    Class* cls = malloc(sizeof(Class));
    if (!cls) return NULL;
    snprintf(cls->name, OOP_MAX_NAME_LEN, "%s", name);
    memset(cls->vtable, 0, sizeof(cls->vtable));
    cls->vtable_size = 0;
    cls->parent_class = NULL;
    cls->instance_size = instance_size;
    cls->num_fields = 0;
    memset(cls->field_names, 0, sizeof(cls->field_names));
    return cls;
}

void class_add_method(Class* cls, const char* name, int arity, OopMethodFn fn) {
    if (cls->vtable_size >= OOP_MAX_METHODS) return;
    Method* m = &cls->vtable[cls->vtable_size];
    snprintf(m->name, OOP_MAX_NAME_LEN, "%s", name);
    m->arity = arity;
    m->fn_ptr = fn;
    cls->vtable_size++;
}

int class_method_index(const Class* cls, const char* name) {
    const Class* current = cls;
    while (current) {
        for (int i = 0; i < current->vtable_size; i++) {
            if (strcmp(current->vtable[i].name, name) == 0) return i;
        }
        current = current->parent_class;
    }
    return -1;
}

Class* class_inherit(const char* name, const Class* parent) {
    Class* cls = class_create(name, parent->instance_size);
    if (!cls) return NULL;
    cls->parent_class = (Class*)parent;
    memcpy(cls->vtable, parent->vtable, sizeof(Method) * parent->vtable_size);
    cls->vtable_size = parent->vtable_size;
    cls->num_fields = parent->num_fields;
    memcpy(cls->field_names, parent->field_names, sizeof(cls->field_names));
    return cls;
}

void class_override_method(Class* cls, const char* name, OopMethodFn fn) {
    const Class* current = cls;
    while (current) {
        for (int i = 0; i < current->vtable_size; i++) {
            if (strcmp(current->vtable[i].name, name) == 0) {
                for (int j = 0; j < cls->vtable_size; j++) {
                    if (strcmp(cls->vtable[j].name, name) == 0) {
                        cls->vtable[j].fn_ptr = fn;
                        return;
                    }
                }
                return;
            }
        }
        current = current->parent_class;
    }
}

Object* object_create(const Class* cls) {
    Object* obj = malloc(sizeof(Object));
    if (!obj) return NULL;
    obj->class = (Class*)cls;
    memset(obj->fields, 0, sizeof(obj->fields));
    return obj;
}

void* object_call_virtual(Object* obj, const char* method_name, void** args) {
    int idx = class_method_index(obj->class, method_name);
    if (idx < 0) return NULL;
    Method* m = NULL;
    const Class* current = obj->class;
    while (current) {
        if (idx < current->vtable_size) {
            const Method* candidate = &current->vtable[idx];
            bool found = false;
            const Class* check = obj->class;
            while (check) {
                for (int i = 0; i < check->vtable_size; i++) {
                    if (strcmp(check->vtable[i].name, method_name) == 0) {
                        m = (Method*)&check->vtable[i];
                        found = true;
                        break;
                    }
                }
                if (found) break;
                check = check->parent_class;
            }
            if (found) break;
        }
        current = current->parent_class;
    }
    if (!m) return NULL;
    return m->fn_ptr(obj, args);
}

void object_set_field(Object* obj, int index, void* value) {
    if (index >= 0 && index < OOP_MAX_FIELDS) {
        obj->fields[index] = value;
    }
}

void* object_get_field(Object* obj, int index) {
    if (index >= 0 && index < OOP_MAX_FIELDS) {
        return obj->fields[index];
    }
    return NULL;
}

static void print_indent(int depth) {
    for (int i = 0; i < depth; i++) printf("  ");
}

void oop_print_class_hierarchy(const Class* cls, int depth) {
    if (!cls) return;
    print_indent(depth);
    printf("Class: %s", cls->name);
    if (cls->parent_class) printf(" extends %s", cls->parent_class->name);
    printf("\n");
    if (cls->vtable_size > 0) {
        print_indent(depth + 1);
        printf("vtable (%d methods): ", cls->vtable_size);
        for (int i = 0; i < cls->vtable_size; i++) {
            printf("%s", cls->vtable[i].name);
            if (i < cls->vtable_size - 1) printf(", ");
        }
        printf("\n");
    }
}

void oop_print_vtable(const Class* cls) {
    if (!cls) return;
    printf("vtable for %s (%d entries):\n", cls->name, cls->vtable_size);
    for (int i = 0; i < cls->vtable_size; i++) {
        printf("  [%d] %s (arity=%d, fn=%p)\n",
               i, cls->vtable[i].name, cls->vtable[i].arity,
               (void*)cls->vtable[i].fn_ptr);
    }
}

void class_destroy(Class* cls) {
    free(cls);
}

void object_destroy(Object* obj) {
    free(obj);
}
