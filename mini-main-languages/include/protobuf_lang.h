#ifndef PROTOBUF_LANG_H
#define PROTOBUF_LANG_H

#include <stdbool.h>
#include <stddef.h>

typedef enum {
    PF_INT32,
    PF_INT64,
    PF_STRING,
    PF_BOOL,
    PF_FLOAT,
    PF_DOUBLE,
    PF_ENUM,
    PF_MESSAGE
} ProtoFieldType;

typedef enum {
    PROTO_OPTIONAL,
    PROTO_REQUIRED,
    PROTO_REPEATED
} ProtoLabel;

typedef struct ProtoField {
    char name[128];
    int field_number;
    ProtoFieldType type;
    ProtoLabel label;
    char type_name[128]; /* for ENUM or MESSAGE references */
    struct ProtoField *next;
} ProtoField;

typedef struct ProtoEnumValue {
    char name[128];
    int number;
    struct ProtoEnumValue *next;
} ProtoEnumValue;

typedef struct ProtoEnum {
    char name[128];
    ProtoEnumValue *values;
    struct ProtoEnum *next;
} ProtoEnum;

typedef struct ProtoMessage {
    char name[128];
    ProtoField *fields;
    struct ProtoMessage *next;
} ProtoMessage;

typedef struct ProtoFile {
    char syntax[16];
    char package[128];
    ProtoMessage *messages;
    ProtoEnum *enums;
} ProtoFile;

void   proto_init_file(ProtoFile *file);
bool   proto_parse(ProtoFile *file, const char *source);
bool   proto_validate(ProtoFile *file);
bool   proto_generate_c_header(ProtoFile *file, const char *filename);
void   proto_print_schema(ProtoFile *file);
void   proto_free_file(ProtoFile *file);

ProtoMessage  *proto_find_message(ProtoFile *file, const char *name);
ProtoEnum     *proto_find_enum(ProtoFile *file, const char *name);
const char    *proto_field_type_name(ProtoFieldType t);
const char    *proto_label_name(ProtoLabel l);
const char    *proto_ctype_name(ProtoFieldType t);

#endif
