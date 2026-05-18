#include "protobuf_lang.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    printf("=== Protobuf/IDL Parser Demo ===\n\n");

    const char *proto_src =
        "syntax = \"proto3\";\n"
        "package example;\n"
        "\n"
        "message Person {\n"
        "    required string name = 1;\n"
        "    required int32  id   = 2;\n"
        "    optional string email = 3;\n"
        "    repeated int32  scores = 4;\n"
        "}\n"
        "\n"
        "message Address {\n"
        "    required string street = 1;\n"
        "    required string city   = 2;\n"
        "    optional string state  = 3;\n"
        "    required int32  zip    = 4;\n"
        "}\n"
        "\n"
        "enum PhoneType {\n"
        "    HOME   = 0;\n"
        "    WORK   = 1;\n"
        "    MOBILE = 2;\n"
        "}\n";

    ProtoFile file;
    proto_init_file(&file);

    printf("Parsing proto definition...\n");
    if (!proto_parse(&file, proto_src)) {
        printf("Parse failed!\n");
        return 1;
    }

    printf("\n--- Schema ---\n");
    proto_print_schema(&file);

    printf("\n--- Validation ---\n");
    if (proto_validate(&file)) {
        printf("Schema is valid.\n");
    } else {
        printf("Schema has errors!\n");
    }

    printf("\n--- Generated C Header ---\n");
    const char *header_path = "generated_person.h";
    if (proto_generate_c_header(&file, header_path)) {
        printf("Generated: %s\n", header_path);

        FILE *f = fopen(header_path, "r");
        if (f) {
            char buf[1024];
            while (fgets(buf, sizeof(buf), f)) printf("%s", buf);
            fclose(f);
        }
    } else {
        printf("Failed to generate header.\n");
    }

    printf("\n--- Lookup ---\n");
    ProtoMessage *person = proto_find_message(&file, "Person");
    if (person) {
        printf("Found message '%s' with %d fields:\n", person->name,
               person->fields ? person->fields->field_number : -1);
        for (ProtoField *f = person->fields; f; f = f->next) {
            printf("  field: %s %s #%d (%s)\n",
                   proto_label_name(f->label),
                   proto_field_type_name(f->type),
                   f->field_number, f->name);
        }
    }

    ProtoEnum *pt = proto_find_enum(&file, "PhoneType");
    if (pt) {
        printf("\nFound enum '%s':\n", pt->name);
        for (ProtoEnumValue *v = pt->values; v; v = v->next)
            printf("  %s = %d\n", v->name, v->number);
    }

    proto_free_file(&file);
    printf("\nDone.\n");
    return 0;
}
