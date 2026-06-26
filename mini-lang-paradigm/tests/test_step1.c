#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "oop_vtable.h"
static int speak_count = 0;
static void* animal_speak(void* self, void** args) {
    (void)self; (void)args; speak_count++; return NULL;
}
int main(void) {
    printf("Step 1: OOP test\n");
    Class* animal = class_create("Animal", sizeof(Object));
    assert(animal != NULL);
    printf("  class_create OK\n");
    class_add_method(animal, "speak", 0, animal_speak);
    printf("  class_add_method OK\n");
    Object* a = object_create(animal);
    assert(a != NULL);
    printf("  object_create OK\n");
    speak_count = 0;
    object_call_virtual(a, "speak", NULL);
    assert(speak_count == 1);
    printf("  virtual dispatch OK\n");
    class_destroy(animal);
    object_destroy(a);
    printf("  cleanup OK\n");
    printf("ALL OOP TESTS PASSED\n");
    return 0;
}
