#ifndef PROTOBUF_LANG_H
#define PROTOBUF_LANG_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* ── L1: Protocol Buffer Schema Types ──────────────────────────────
 * Reference: Google Protocol Buffers Language Specification (proto3)
 *            https://developers.google.com/protocol-buffers/docs/proto3
 */
typedef enum {
    PF_INT32,
    PF_INT64,
    PF_UINT32,
    PF_UINT64,
    PF_SINT32,
    PF_SINT64,
    PF_STRING,
    PF_BOOL,
    PF_FLOAT,
    PF_DOUBLE,
    PF_BYTES,
    PF_ENUM,
    PF_MESSAGE
} ProtoFieldType;

/* ── L3: Field Labels ───────────────────────────────────────────── */
typedef enum {
    PROTO_OPTIONAL,   /* proto3 default */
    PROTO_REQUIRED,   /* proto2 — must be set */
    PROTO_REPEATED    /* array/list field */
} ProtoLabel;

/* ── L3: Schema AST ─────────────────────────────────────────────── */
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

/* ── L3: Wire Format Types ────────────────────────────────────────
 * Proto3 wire types (3 bits, values 0-5):
 *   0: Varint (int32, int64, uint32, uint64, sint32, sint64, bool, enum)
 *   1: 64-bit (fixed64, sfixed64, double)
 *   2: Length-delimited (string, bytes, embedded messages, packed repeated)
 *   3: Start group (deprecated)
 *   4: End group (deprecated)
 *   5: 32-bit (fixed32, sfixed32, float)
 */
typedef enum {
    WIRE_VARINT = 0,
    WIRE_64BIT  = 1,
    WIRE_LEN    = 2,
    WIRE_32BIT  = 5
} ProtoWireType;

/* ── L5: Encoded Message ────────────────────────────────────────── */
typedef struct {
    uint8_t *data;
    size_t  size;
    size_t  capacity;
} ProtoBuffer;

/* ── L5: Runtime message field value ────────────────────────────── */
typedef struct ProtoFieldValue {
    ProtoField *field;
    union {
        int32_t  int32_val;
        int64_t  int64_val;
        uint32_t uint32_val;
        uint64_t uint64_val;
        bool     bool_val;
        float    float_val;
        double   double_val;
        char    *str_val;
        uint8_t *bytes_val;
        struct ProtoMessageValue *msg_val; /* nested message */
    } data;
    size_t bytes_len;
    struct ProtoFieldValue **repeated;
    int repeated_count;
    int repeated_cap;
    struct ProtoFieldValue *next;
} ProtoFieldValue;

/* ── L5: Runtime message ────────────────────────────────────────── */
typedef struct ProtoMessageValue {
    ProtoMessage *schema;
    ProtoFieldValue *fields;
} ProtoMessageValue;

/* ── API Declarations ────────────────────────────────────────────── */

/* L1: File lifecycle */
void   proto_init_file(ProtoFile *file);
void   proto_free_file(ProtoFile *file);

/* L2-3: Parsing */
bool   proto_parse(ProtoFile *file, const char *source);

/* L4: Validation (schema constraints) */
bool   proto_validate(ProtoFile *file);

/* L5: Code generation (C header) */
bool   proto_generate_c_header(ProtoFile *file, const char *filename);

/* L5: Binary serialization (Wire Format) */
ProtoBuffer      *proto_buffer_new(void);
void              proto_buffer_free(ProtoBuffer *buf);
bool              proto_buffer_write_varint(ProtoBuffer *buf, uint64_t value);
bool              proto_buffer_write_field(ProtoBuffer *buf, int field_number,
                                           ProtoWireType wire_type, ProtoBuffer *payload);
bool              proto_buffer_write_tag(ProtoBuffer *buf, int field_number,
                                         ProtoWireType wire_type);
bool              proto_buffer_write_int32(ProtoBuffer *buf, int field_number, int32_t value);
bool              proto_buffer_write_int64(ProtoBuffer *buf, int field_number, int64_t value);
bool              proto_buffer_write_string(ProtoBuffer *buf, int field_number, const char *value);
bool              proto_buffer_write_bool(ProtoBuffer *buf, int field_number, bool value);
bool              proto_buffer_write_float(ProtoBuffer *buf, int field_number, float value);
bool              proto_buffer_write_double(ProtoBuffer *buf, int field_number, double value);

/* L5: Binary deserialization */
bool              proto_buffer_read_varint(const uint8_t *data, size_t size,
                                           size_t *offset, uint64_t *value);
bool              proto_buffer_read_field(const uint8_t *data, size_t size,
                                          size_t *offset, int *field_number,
                                          ProtoWireType *wire_type);
bool              proto_buffer_read_int32(const uint8_t *data, size_t size,
                                          size_t *offset, int field_number, int32_t *value);
bool              proto_buffer_read_string(const uint8_t *data, size_t size,
                                           size_t *offset, const char **value, size_t *len);

/* L5: Message serialization/deserialization */
bool              proto_serialize_message(ProtoFile *schema, const char *msg_name,
                                          ProtoMessageValue *msg, ProtoBuffer *out);
bool              proto_deserialize_message(ProtoFile *schema, const char *msg_name,
                                            const uint8_t *data, size_t size,
                                            ProtoMessageValue *msg);

/* L7: JSON output */
void              proto_print_json(ProtoFile *file, ProtoMessageValue *msg);

/* L4: Print schema */
void              proto_print_schema(ProtoFile *file);

/* Lookup */
ProtoMessage  *proto_find_message(ProtoFile *file, const char *name);
ProtoEnum     *proto_find_enum(ProtoFile *file, const char *name);
const char    *proto_field_type_name(ProtoFieldType t);
const char    *proto_label_name(ProtoLabel l);
const char    *proto_ctype_name(ProtoFieldType t);
ProtoWireType  proto_wire_type(ProtoFieldType t);

/* L5: Runtime value management */
ProtoMessageValue *proto_msg_new(void);
void               proto_msg_free(ProtoMessageValue *msg);
ProtoFieldValue   *proto_msg_set_int32(ProtoMessageValue *msg, const char *field, int32_t val);
ProtoFieldValue   *proto_msg_set_string(ProtoMessageValue *msg, const char *field, const char *val);

#endif
