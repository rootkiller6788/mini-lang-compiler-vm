#include "oop_vtable.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void* animal_speak_impl(void* self, void** args) {
    (void)args;
    printf("Animal speaks: ...\n");
    return NULL;
}

static void* dog_speak_impl(void* self, void** args) {
    (void)args;
    printf("Dog speaks: Woof! Woof!\n");
    return NULL;
}

static void* cat_speak_impl(void* self, void** args) {
    (void)args;
    printf("Cat speaks: Meow~\n");
    return NULL;
}

int main(void) {
    printf("=== OOP vtable Demo ===\n\n");

    Class* animal = class_create("Animal", sizeof(Object));
    class_add_method(animal, "speak", 0, animal_speak_impl);
    printf("Created base class:\n");
    oop_print_class_hierarchy(animal, 0);
    oop_print_vtable(animal);
    printf("\n");

    Class* dog = class_inherit("Dog", animal);
    class_override_method(dog, "speak", dog_speak_impl);
    printf("Created subclass (inherits):\n");
    oop_print_class_hierarchy(dog, 0);
    oop_print_vtable(dog);
    printf("\n");

    Class* cat = class_inherit("Cat", animal);
    class_override_method(cat, "speak", cat_speak_impl);
    printf("Created subclass (inherits):\n");
    oop_print_class_hierarchy(cat, 0);
    printf("\n");

    printf("=== Virtual method dispatch ===\n\n");

    Object* a = object_create(animal);
    Object* d = object_create(dog);
    Object* c = object_create(cat);

    Object* zoo[] = { a, d, c };
    const char* names[] = { "Animal", "Dog", "Cat" };

    for (int i = 0; i < 3; i++) {
        printf("[%s] ", names[i]);
        object_call_virtual(zoo[i], "speak", NULL);
    }

    printf("\n=== Class hierarchy ===\n\n");
    oop_print_class_hierarchy(dog, 0);

    object_destroy(a);
    object_destroy(d);
    object_destroy(c);
    class_destroy(animal);
    class_destroy(dog);
    class_destroy(cat);

    printf("\nDone.\n");
    return 0;
}
