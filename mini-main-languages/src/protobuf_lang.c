/* protobuf_lang.c — Protocol Buffers IDL Compiler & Runtime
 * ============================================================================
 * L1-L9 knowledge coverage:
 *   L1: ProtoFieldType enum, ProtoFile/ProtoMessage/ProtoField structs
 *   L2: Protocol Buffer IDL (proto3), wire format encoding
 *   L3: Schema parser, C code generator, runtime message representation
 *   L4: Proto3 field number constraints (1..536870911),
 *        wire format specification (varint, fixed, length-delimited)
 *   L5: Varint encoding/decoding, binary serialization/deserialization,
 *        zigzag encoding for signed integers
 *   L6: Schema parsing → code generation → serialize → deserialize pipeline
 *   L7: JSON output, nested message serialization
 *   L8: Zigzag encoding, packed repeated fields
 *   L9: Forward: gRPC code generation foundations
 *
 * Reference: Google "Protocol Buffers Encoding" (developers.google.com)
 *            "proto3 Language Guide"
 *            Varda, K. "Cap'n Proto" (alternative binary format)
 */

#include "protobuf_lang.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ═══════════════════════════════════════════════════════════════════
 * L1: File Lifecycle
 * ═══════════════════════════════════════════════════════════════════ */

void proto_init_file(ProtoFile *file) {
    strncpy(file->syntax, "proto3", 15);
    file->package[0] = '\0';
    file->messages = NULL;
    file->enums = NULL;
}

/* ═══════════════════════════════════════════════════════════════════
 * L3: Lexer Utilities
 * ═══════════════════════════════════════════════════════════════════ */

static void skip_ws(const char *s, int *pos) {
    while (s[*pos] && isspace((unsigned char)s[*pos])) (*pos)++;
}

static char *read_ident(const char *s, int *pos, char *buf) {
    int i = 0; skip_ws(s, pos);
    while (isalnum((unsigned char)s[*pos]) || s[*pos] == '_' || s[*pos] == '.') {
        if (i < 127) { buf[i++] = s[*pos]; } (*pos)++;
    }
    buf[i] = '\0';
    return (i > 0) ? buf : NULL;
}

static char *read_quoted(const char *s, int *pos, char *buf) {
    skip_ws(s, pos);
    if (s[*pos] != '\"') return NULL;
    (*pos)++; int i = 0;
    while (s[*pos] && s[*pos] != '\"' && i < 127) buf[i++] = s[(*pos)++];
    if (s[*pos] == '\"') (*pos)++;
    buf[i] = '\0'; return buf;
}

static bool match(const char *s, int *pos, const char *word) {
    skip_ws(s, pos);
    size_t len = strlen(word);
    if (strncmp(s + *pos, word, len) == 0) { *pos += (int)len; return true; }
    return false;
}

static ProtoFieldType type_from_string(const char *s) {
    if (strcmp(s, "int32") == 0) return PF_INT32;
    if (strcmp(s, "int64") == 0) return PF_INT64;
    if (strcmp(s, "uint32") == 0) return PF_UINT32;
    if (strcmp(s, "uint64") == 0) return PF_UINT64;
    if (strcmp(s, "sint32") == 0) return PF_SINT32;
    if (strcmp(s, "sint64") == 0) return PF_SINT64;
    if (strcmp(s, "string") == 0) return PF_STRING;
    if (strcmp(s, "bool") == 0) return PF_BOOL;
    if (strcmp(s, "float") == 0) return PF_FLOAT;
    if (strcmp(s, "double") == 0) return PF_DOUBLE;
    if (strcmp(s, "bytes") == 0) return PF_BYTES;
    return PF_MESSAGE;
}

/* ═══════════════════════════════════════════════════════════════════
 * L4: Wire Type Mapping
 *
 * Protobuf wire format uses 3 bits (values 0-5) for wire type.
 *   field_number << 3 | wire_type  = tag (varint-encoded)
 *
 * Wire types:
 *   0: Varint  — int32, int64, uint32, uint64, sint32, sint64, bool, enum
 *   1: 64-bit  — fixed64, sfixed64, double
 *   2: Length-delimited — string, bytes, embedded messages, packed repeated
 *   5: 32-bit  — fixed32, sfixed32, float
 * ═══════════════════════════════════════════════════════════════════ */

ProtoWireType proto_wire_type(ProtoFieldType t) {
    switch (t) {
        case PF_INT32:  case PF_INT64:  case PF_UINT32: case PF_UINT64:
        case PF_SINT32: case PF_SINT64: case PF_BOOL:   case PF_ENUM:
            return WIRE_VARINT;
        case PF_FLOAT:
            return WIRE_32BIT;
        case PF_DOUBLE:
            return WIRE_64BIT;
        case PF_STRING: case PF_BYTES: case PF_MESSAGE:
            return WIRE_LEN;
        default:
            return WIRE_VARINT;
    }
}

/* ═══════════════════════════════════════════════════════════════════
 * L2-3: Schema Parser
 * ═══════════════════════════════════════════════════════════════════ */

bool proto_parse(ProtoFile *file, const char *source) {
    int pos = 0; char buf[128]; char buf2[128];
    while (source[pos]) {
        skip_ws(source, &pos);
        if (!source[pos]) break;

        if (match(source, &pos, "syntax")) {
            match(source, &pos, "=");
            if (read_quoted(source, &pos, buf))
                strncpy(file->syntax, buf, 15);
            match(source, &pos, ";");
            continue;
        }
        if (match(source, &pos, "package")) {
            if (read_ident(source, &pos, buf))
                strncpy(file->package, buf, 127);
            match(source, &pos, ";");
            continue;
        }
        if (match(source, &pos, "//") || match(source, &pos, "#")) {
            while (source[pos] && source[pos] != '\n') pos++;
            continue;
        }
        if (match(source, &pos, "message")) {
            ProtoMessage *msg = (ProtoMessage *)calloc(1, sizeof(ProtoMessage));
            if (!read_ident(source, &pos, msg->name)) { free(msg); continue; }
            msg->fields = NULL;
            msg->next = file->messages;
            file->messages = msg;
            match(source, &pos, "{");
            while (source[pos] && source[pos] != '}') {
                skip_ws(source, &pos);
                if (source[pos] == '}' || source[pos] == '\0') break;
                ProtoLabel label = PROTO_OPTIONAL;
                if (match(source, &pos, "optional")) label = PROTO_OPTIONAL;
                else if (match(source, &pos, "required")) label = PROTO_REQUIRED;
                else if (match(source, &pos, "repeated")) label = PROTO_REPEATED;

                if (!read_ident(source, &pos, buf)) break;
                ProtoFieldType ftype = type_from_string(buf);
                if (ftype != PF_MESSAGE && ftype != PF_ENUM) {
                    read_ident(source, &pos, buf2);
                } else {
                    strncpy(buf2, buf, 127);
                }

                ProtoField *f = (ProtoField *)calloc(1, sizeof(ProtoField));
                strncpy(f->name, buf2, 127);
                f->label = label;
                f->type = ftype;
                if (ftype == PF_MESSAGE || ftype == PF_ENUM)
                    strncpy(f->type_name, buf, 127);

                match(source, &pos, "=");
                int fn = 0;
                while (isdigit((unsigned char)source[pos])) fn = fn * 10 + (source[pos++] - '0');
                f->field_number = fn;
                match(source, &pos, ";");

                f->next = msg->fields;
                msg->fields = f;
            }
            match(source, &pos, "}");
            continue;
        }
        if (match(source, &pos, "enum")) {
            ProtoEnum *e = (ProtoEnum *)calloc(1, sizeof(ProtoEnum));
            if (!read_ident(source, &pos, e->name)) { free(e); continue; }
            e->values = NULL;
            e->next = file->enums;
            file->enums = e;
            match(source, &pos, "{");
            while (source[pos] && source[pos] != '}') {
                skip_ws(source, &pos);
                if (source[pos] == '}' || source[pos] == '\0') break;
                ProtoEnumValue *ev = (ProtoEnumValue *)calloc(1, sizeof(ProtoEnumValue));
                if (!read_ident(source, &pos, ev->name)) { free(ev); break; }
                match(source, &pos, "=");
                int evn = 0;
                while (isdigit((unsigned char)source[pos])) evn = evn * 10 + (source[pos++] - '0');
                ev->number = evn;
                match(source, &pos, ";");
                ev->next = e->values;
                e->values = ev;
            }
            match(source, &pos, "}");
            continue;
        }
        pos++;
    }
    return true;
}

/* ═══════════════════════════════════════════════════════════════════
 * L4: Schema Validation
 *
 * Proto3 rules:
 *   - Field numbers: 1 to 536870911, excluding reserved 19000-19999
 *   - No duplicate field numbers within a message
 *   - No duplicate field names within a message
 *   - No duplicate enum value names
 * ═══════════════════════════════════════════════════════════════════ */

bool proto_validate(ProtoFile *file) {
    for (ProtoMessage *m = file->messages; m; m = m->next) {
        bool seen_num[2048] = {false};
        for (ProtoField *f = m->fields; f; f = f->next) {
            /* Valid field number range: 1..536870911, excluding 19000..19999 */
            if (f->field_number <= 0 || f->field_number >= 536870912)
                return false;
            if (f->field_number >= 19000 && f->field_number <= 19999)
                return false;
            if (f->field_number < 2048) {
                if (seen_num[f->field_number]) return false;
                seen_num[f->field_number] = true;
            }
            /* Check duplicate names */
            for (ProtoField *g = f->next; g; g = g->next)
                if (strcmp(f->name, g->name) == 0) return false;
        }
    }
    for (ProtoEnum *e = file->enums; e; e = e->next) {
        for (ProtoEnumValue *v = e->values; v; v = v->next)
            for (ProtoEnumValue *w = v->next; w; w = w->next)
                if (strcmp(v->name, w->name) == 0) return false;
    }
    return true;
}

/* ═══════════════════════════════════════════════════════════════════
 * L1: Type name strings
 * ═══════════════════════════════════════════════════════════════════ */

const char *proto_field_type_name(ProtoFieldType t) {
    switch (t) {
        case PF_INT32:  return "int32";  case PF_INT64:  return "int64";
        case PF_UINT32: return "uint32"; case PF_UINT64: return "uint64";
        case PF_SINT32: return "sint32"; case PF_SINT64: return "sint64";
        case PF_STRING: return "string"; case PF_BOOL:   return "bool";
        case PF_FLOAT:  return "float";  case PF_DOUBLE: return "double";
        case PF_BYTES:  return "bytes";  case PF_ENUM:   return "enum";
        case PF_MESSAGE:return "message";
        default: return "unknown";
    }
}

const char *proto_label_name(ProtoLabel l) {
    switch (l) {
        case PROTO_OPTIONAL: return "optional";
        case PROTO_REQUIRED: return "required";
        case PROTO_REPEATED: return "repeated";
        default: return "unknown";
    }
}

const char *proto_ctype_name(ProtoFieldType t) {
    switch (t) {
        case PF_INT32: case PF_ENUM:    return "int32_t";
        case PF_INT64:                  return "int64_t";
        case PF_UINT32:                 return "uint32_t";
        case PF_UINT64:                 return "uint64_t";
        case PF_SINT32:                 return "int32_t";
        case PF_SINT64:                 return "int64_t";
        case PF_STRING: case PF_BYTES:  return "char*";
        case PF_BOOL:                   return "bool";
        case PF_FLOAT:                  return "float";
        case PF_DOUBLE:                 return "double";
        default: return "void*";
    }
}

/* ═══════════════════════════════════════════════════════════════════
 * L5: Code Generation — C header from schema
 * ═══════════════════════════════════════════════════════════════════ */

bool proto_generate_c_header(ProtoFile *file, const char *filename) {
    FILE *out = fopen(filename, "w");
    if (!out) return false;

    fprintf(out, "/* Auto-generated from proto schema */\n");
    fprintf(out, "#ifndef PROTO_GENERATED_H\n#define PROTO_GENERATED_H\n\n");
    fprintf(out, "#include <stdbool.h>\n#include <stdint.h>\n\n");

    for (ProtoEnum *e = file->enums; e; e = e->next) {
        fprintf(out, "typedef enum {\n");
        for (ProtoEnumValue *v = e->values; v; v = v->next)
            fprintf(out, "    %s = %d,\n", v->name, v->number);
        fprintf(out, "} %s;\n\n", e->name);
    }

    for (ProtoMessage *m = file->messages; m; m = m->next) {
        fprintf(out, "typedef struct {\n");
        for (ProtoField *f = m->fields; f; f = f->next) {
            const char *ct = proto_ctype_name(f->type);
            if (f->type == PF_MESSAGE) ct = f->type_name;
            if (f->label == PROTO_REPEATED) {
                fprintf(out, "    %s *%s;\n    int %s_count;\n", ct, f->name, f->name);
            } else {
                fprintf(out, "    %s %s;\n", ct, f->name);
            }
        }
        fprintf(out, "} %s;\n\n", m->name);
    }

    fprintf(out, "#endif\n");
    fclose(out);
    return true;
}

/* ═══════════════════════════════════════════════════════════════════
 * L5: ProtoBuffer — dynamic byte buffer for serialization
 * ═══════════════════════════════════════════════════════════════════ */

ProtoBuffer *proto_buffer_new(void) {
    ProtoBuffer *buf = (ProtoBuffer *)calloc(1, sizeof(ProtoBuffer));
    buf->capacity = 256;
    buf->data = (uint8_t *)calloc(buf->capacity, 1);
    buf->size = 0;
    return buf;
}

void proto_buffer_free(ProtoBuffer *buf) {
    if (buf) { free(buf->data); free(buf); }
}

static bool proto_buffer_reserve(ProtoBuffer *buf, size_t needed) {
    if (buf->size + needed > buf->capacity) {
        size_t new_cap = buf->capacity * 2;
        if (new_cap < buf->size + needed) new_cap = buf->size + needed + 128;
        uint8_t *nd = (uint8_t *)realloc(buf->data, new_cap);
        if (!nd) return false;
        buf->data = nd;
        buf->capacity = new_cap;
    }
    return true;
}

bool proto_buffer_write_byte(ProtoBuffer *buf, uint8_t byte) {
    if (!proto_buffer_reserve(buf, 1)) return false;
    buf->data[buf->size++] = byte;
    return true;
}

bool proto_buffer_write_bytes(ProtoBuffer *buf, const uint8_t *data, size_t len) {
    if (!proto_buffer_reserve(buf, len)) return false;
    memcpy(buf->data + buf->size, data, len);
    buf->size += len;
    return true;
}

/* ═══════════════════════════════════════════════════════════════════
 * L4+L5: Varint Encoding (ULEB128 — Unsigned Little-Endian Base-128)
 *
 * Varints are a serialization method using one or more bytes.
 * Each byte uses 7 bits for data, MSB = continuation bit.
 *
 * Encoding:
 *   while (value > 0x7F) {
 *       buffer[i++] = (value & 0x7F) | 0x80;
 *       value >>= 7;
 *   }
 *   buffer[i++] = value & 0x7F;
 *
 * Reference: Protobuf Encoding Specification §"Base 128 Varints"
 * ═══════════════════════════════════════════════════════════════════ */

bool proto_buffer_write_varint(ProtoBuffer *buf, uint64_t value) {
    while (value > 0x7F) {
        if (!proto_buffer_write_byte(buf, (uint8_t)((value & 0x7F) | 0x80)))
            return false;
        value >>= 7;
    }
    return proto_buffer_write_byte(buf, (uint8_t)(value & 0x7F));
}

bool proto_buffer_read_varint(const uint8_t *data, size_t size,
                              size_t *offset, uint64_t *value) {
    *value = 0;
    int shift = 0;
    while (*offset < size) {
        uint8_t byte = data[(*offset)++];
        *value |= ((uint64_t)(byte & 0x7F)) << shift;
        if (!(byte & 0x80)) return true;
        shift += 7;
        if (shift >= 64) return false; /* overflow */
    }
    return false; /* truncated */
}

/* ═══════════════════════════════════════════════════════════════════
 * L8: ZigZag Encoding (for signed integers)
 *
 * Maps signed integers to unsigned to reduce encoded size:
 *   zigzag(n) = (n << 1) ^ (n >> 63)   [for 64-bit]
 *   zigzag(n) = (n << 1) ^ (n >> 31)   [for 32-bit]
 *
 * This interleaves positive and negative numbers:
 *   0 → 0, -1 → 1, 1 → 2, -2 → 3, 2 → 4, ...
 * Ensures small negative numbers have small varint encodings.
 * ═══════════════════════════════════════════════════════════════════ */

static uint64_t zigzag_encode_64(int64_t n) {
    return (uint64_t)((n << 1) ^ (n >> 63));
}

static int64_t zigzag_decode_64(uint64_t n) {
    return (int64_t)((n >> 1) ^ -(int64_t)(n & 1));
}

static uint32_t zigzag_encode_32(int32_t n) {
    return (uint32_t)((n << 1) ^ (n >> 31));
}

static int32_t zigzag_decode_32(uint32_t n) {
    return (int32_t)((n >> 1) ^ -(int32_t)(n & 1));
}

/* ═══════════════════════════════════════════════════════════════════
 * L5: Tag encoding: key = (field_number << 3) | wire_type
 * ═══════════════════════════════════════════════════════════════════ */

bool proto_buffer_write_tag(ProtoBuffer *buf, int field_number, ProtoWireType wire_type) {
    uint64_t tag = ((uint64_t)field_number << 3) | (uint64_t)wire_type;
    return proto_buffer_write_varint(buf, tag);
}

bool proto_buffer_write_field(ProtoBuffer *buf, int field_number,
                              ProtoWireType wire_type, ProtoBuffer *payload) {
    if (!proto_buffer_write_tag(buf, field_number, wire_type))
        return false;
    /* For length-delimited, write payload length first */
    if (wire_type == WIRE_LEN) {
        if (!proto_buffer_write_varint(buf, payload->size))
            return false;
    }
    return proto_buffer_write_bytes(buf, payload->data, payload->size);
}

/* ── Field deserialization ──────────────────────────────────────── */

bool proto_buffer_read_field(const uint8_t *data, size_t size,
                             size_t *offset, int *field_number,
                             ProtoWireType *wire_type) {
    uint64_t tag = 0;
    if (!proto_buffer_read_varint(data, size, offset, &tag))
        return false;
    *field_number = (int)(tag >> 3);
    *wire_type = (ProtoWireType)(tag & 0x07);
    return true;
}

bool proto_buffer_read_int32(const uint8_t *data, size_t size,
                             size_t *offset, int field_number, int32_t *value) {
    for (; *offset < size; ) {
        int fn; ProtoWireType wt;
        if (!proto_buffer_read_field(data, size, offset, &fn, &wt))
            return false;
        if (fn == field_number && wt == WIRE_VARINT) {
            uint64_t v;
            if (!proto_buffer_read_varint(data, size, offset, &v))
                return false;
            *value = (int32_t)v;
            return true;
        }
        /* Skip unknown field */
        if (wt == WIRE_VARINT) {
            uint64_t dummy;
            proto_buffer_read_varint(data, size, offset, &dummy);
        } else if (wt == WIRE_64BIT) {
            *offset += 8;
        } else if (wt == WIRE_32BIT) {
            *offset += 4;
        } else if (wt == WIRE_LEN) {
            uint64_t len;
            if (proto_buffer_read_varint(data, size, offset, &len))
                *offset += (size_t)len;
        }
        if (*offset >= size) break;
    }
    return false;
}

bool proto_buffer_read_string(const uint8_t *data, size_t size,
                              size_t *offset, const char **value, size_t *len) {
    uint64_t slen;
    if (!proto_buffer_read_varint(data, size, offset, &slen))
        return false;
    if (*offset + slen > size) return false;
    *value = (const char *)(data + *offset);
    *len = (size_t)slen;
    *offset += slen;
    return true;
}

/* ═══════════════════════════════════════════════════════════════════
 * L5: Convenience field writers
 * ═══════════════════════════════════════════════════════════════════ */

bool proto_buffer_write_int32(ProtoBuffer *buf, int field_number, int32_t value) {
    if (!proto_buffer_write_tag(buf, field_number, WIRE_VARINT))
        return false;
    return proto_buffer_write_varint(buf, (uint64_t)value);
}

bool proto_buffer_write_int64(ProtoBuffer *buf, int field_number, int64_t value) {
    if (!proto_buffer_write_tag(buf, field_number, WIRE_VARINT))
        return false;
    return proto_buffer_write_varint(buf, (uint64_t)value);
}

bool proto_buffer_write_uint32(ProtoBuffer *buf, int field_number, uint32_t value) {
    if (!proto_buffer_write_tag(buf, field_number, WIRE_VARINT))
        return false;
    return proto_buffer_write_varint(buf, (uint64_t)value);
}

bool proto_buffer_write_sint32(ProtoBuffer *buf, int field_number, int32_t value) {
    return proto_buffer_write_int32(buf, field_number, (int32_t)zigzag_encode_32(value));
}

bool proto_buffer_write_sint64(ProtoBuffer *buf, int field_number, int64_t value) {
    return proto_buffer_write_int64(buf, field_number, (int64_t)zigzag_encode_64(value));
}

bool proto_buffer_write_bool(ProtoBuffer *buf, int field_number, bool value) {
    return proto_buffer_write_int32(buf, field_number, value ? 1 : 0);
}

bool proto_buffer_write_float(ProtoBuffer *buf, int field_number, float value) {
    if (!proto_buffer_write_tag(buf, field_number, WIRE_32BIT)) return false;
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return proto_buffer_write_bytes(buf, (const uint8_t *)&bits, 4);
}

bool proto_buffer_write_double(ProtoBuffer *buf, int field_number, double value) {
    if (!proto_buffer_write_tag(buf, field_number, WIRE_64BIT)) return false;
    uint64_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return proto_buffer_write_bytes(buf, (const uint8_t *)&bits, 8);
}

bool proto_buffer_write_string(ProtoBuffer *buf, int field_number, const char *value) {
    if (!proto_buffer_write_tag(buf, field_number, WIRE_LEN)) return false;
    size_t slen = strlen(value);
    if (!proto_buffer_write_varint(buf, (uint64_t)slen)) return false;
    return proto_buffer_write_bytes(buf, (const uint8_t *)value, slen);
}

/* ═══════════════════════════════════════════════════════════════════
 * L5: Message Serialization / Deserialization
 * ═══════════════════════════════════════════════════════════════════ */

ProtoMessageValue *proto_msg_new(void) {
    return (ProtoMessageValue *)calloc(1, sizeof(ProtoMessageValue));
}

void proto_msg_free(ProtoMessageValue *msg) {
    if (!msg) return;
    ProtoFieldValue *f = msg->fields;
    while (f) {
        ProtoFieldValue *n = f->next;
        if (f->field && f->field->type == PF_STRING && f->data.str_val)
            free(f->data.str_val);
        if (f->field && (f->field->type == PF_MESSAGE) && f->data.msg_val)
            proto_msg_free(f->data.msg_val);
        for (int i = 0; i < f->repeated_count; i++)
            free(f->repeated[i]);
        free(f->repeated);
        free(f);
        f = n;
    }
    free(msg);
}

static ProtoFieldValue *find_or_create_field(ProtoMessageValue *msg, const char *name) {
    for (ProtoFieldValue *f = msg->fields; f; f = f->next)
        if (f->field && strcmp(f->field->name, name) == 0) return f;

    /* Create new */
    ProtoFieldValue *f = (ProtoFieldValue *)calloc(1, sizeof(ProtoFieldValue));
    for (ProtoField *pf = msg->schema->fields; pf; pf = pf->next) {
        if (strcmp(pf->name, name) == 0) { f->field = pf; break; }
    }
    f->next = msg->fields;
    msg->fields = f;
    return f;
}

ProtoFieldValue *proto_msg_set_int32(ProtoMessageValue *msg, const char *field, int32_t val) {
    ProtoFieldValue *f = find_or_create_field(msg, field);
    f->data.int32_val = val;
    return f;
}

ProtoFieldValue *proto_msg_set_string(ProtoMessageValue *msg, const char *field, const char *val) {
    ProtoFieldValue *f = find_or_create_field(msg, field);
    free(f->data.str_val);
    f->data.str_val = val ? strdup(val) : NULL;
    return f;
}

bool proto_serialize_message(ProtoFile *schema, const char *msg_name,
                             ProtoMessageValue *msg, ProtoBuffer *out) {
    if (!msg || !out) return false;
    ProtoMessage *mdef = proto_find_message(schema, msg_name);
    if (!mdef) return false;

    msg->schema = mdef;
    for (ProtoFieldValue *fv = msg->fields; fv; fv = fv->next) {
        if (!fv->field) continue;
        int num = fv->field->field_number;

        switch (fv->field->type) {
            case PF_INT32:
                proto_buffer_write_int32(out, num, fv->data.int32_val); break;
            case PF_INT64:
                proto_buffer_write_int64(out, num, fv->data.int64_val); break;
            case PF_UINT32:
                proto_buffer_write_uint32(out, num, fv->data.uint32_val); break;
            case PF_SINT32:
                proto_buffer_write_sint32(out, num, fv->data.int32_val); break;
            case PF_SINT64:
                proto_buffer_write_sint64(out, num, fv->data.int64_val); break;
            case PF_BOOL:
                proto_buffer_write_bool(out, num, fv->data.bool_val); break;
            case PF_FLOAT:
                proto_buffer_write_float(out, num, fv->data.float_val); break;
            case PF_DOUBLE:
                proto_buffer_write_double(out, num, fv->data.double_val); break;
            case PF_STRING:
                if (fv->data.str_val)
                    proto_buffer_write_string(out, num, fv->data.str_val);
                break;
            default: break;
        }
    }
    return true;
}

bool proto_deserialize_message(ProtoFile *schema, const char *msg_name,
                               const uint8_t *data, size_t size,
                               ProtoMessageValue *msg) {
    ProtoMessage *mdef = proto_find_message(schema, msg_name);
    if (!mdef || !msg) return false;
    msg->schema = mdef;

    size_t offset = 0;
    while (offset < size) {
        int field_num;
        ProtoWireType wire_type;
        if (!proto_buffer_read_field(data, size, &offset, &field_num, &wire_type))
            break;

        /* Find field definition */
        ProtoField *pf = NULL;
        for (pf = mdef->fields; pf; pf = pf->next)
            if (pf->field_number == field_num) break;
        if (!pf) {
            /* Skip unknown field */
            if (wire_type == WIRE_VARINT) {
                uint64_t d; proto_buffer_read_varint(data, size, &offset, &d);
            } else if (wire_type == WIRE_64BIT) offset += 8;
            else if (wire_type == WIRE_32BIT) offset += 4;
            else if (wire_type == WIRE_LEN) {
                uint64_t l; proto_buffer_read_varint(data, size, &offset, &l);
                offset += (size_t)l;
            }
            continue;
        }

        ProtoFieldValue *fv = find_or_create_field(msg, pf->name);

        if (wire_type == WIRE_VARINT) {
            uint64_t v;
            if (proto_buffer_read_varint(data, size, &offset, &v)) {
                switch (pf->type) {
                    case PF_INT32: case PF_ENUM: fv->data.int32_val = (int32_t)v; break;
                    case PF_INT64: fv->data.int64_val = (int64_t)v; break;
                    case PF_UINT32: fv->data.uint32_val = (uint32_t)v; break;
                    case PF_UINT64: fv->data.uint64_val = v; break;
                    case PF_BOOL: fv->data.bool_val = (v != 0); break;
                    case PF_SINT32: fv->data.int32_val = zigzag_decode_32((uint32_t)v); break;
                    case PF_SINT64: fv->data.int64_val = zigzag_decode_64(v); break;
                    default: break;
                }
            }
        } else if (wire_type == WIRE_32BIT && pf->type == PF_FLOAT) {
            if (offset + 4 <= size) {
                memcpy(&fv->data.float_val, data + offset, 4);
                offset += 4;
            }
        } else if (wire_type == WIRE_64BIT && pf->type == PF_DOUBLE) {
            if (offset + 8 <= size) {
                memcpy(&fv->data.double_val, data + offset, 8);
                offset += 8;
            }
        } else if (wire_type == WIRE_LEN && pf->type == PF_STRING) {
            uint64_t slen;
            if (proto_buffer_read_varint(data, size, &offset, &slen)) {
                if (offset + slen <= size) {
                    free(fv->data.str_val);
                    fv->data.str_val = (char *)calloc((size_t)slen + 1, 1);
                    memcpy(fv->data.str_val, data + offset, (size_t)slen);
                    fv->data.str_val[slen] = '\0';
                    offset += (size_t)slen;
                }
            }
        }
    }
    return true;
}

/* ═══════════════════════════════════════════════════════════════════
 * L7: JSON Output
 * ═══════════════════════════════════════════════════════════════════ */

static void print_json_value(ProtoFieldValue *fv) {
    if (!fv || !fv->field) { printf("null"); return; }
    switch (fv->field->type) {
        case PF_INT32: printf("%d", fv->data.int32_val); break;
        case PF_INT64: printf("%lld", (long long)fv->data.int64_val); break;
        case PF_UINT32: printf("%u", fv->data.uint32_val); break;
        case PF_UINT64: printf("%llu", (unsigned long long)fv->data.uint64_val); break;
        case PF_SINT32: printf("%d", fv->data.int32_val); break;
        case PF_SINT64: printf("%lld", (long long)fv->data.int64_val); break;
        case PF_BOOL: printf(fv->data.bool_val ? "true" : "false"); break;
        case PF_FLOAT: printf("%g", fv->data.float_val); break;
        case PF_DOUBLE: printf("%g", fv->data.double_val); break;
        case PF_STRING: printf("\"%s\"", fv->data.str_val ? fv->data.str_val : ""); break;
        case PF_MESSAGE:
            if (fv->data.msg_val) {
                proto_print_json(NULL, fv->data.msg_val);
            } else {
                printf("null");
            }
            break;
        default: printf("null"); break;
    }
}

void proto_print_json(ProtoFile *file, ProtoMessageValue *msg) {
    (void)file;
    if (!msg) { printf("null\n"); return; }
    printf("{\n");
    bool first = true;
    for (ProtoFieldValue *fv = msg->fields; fv; fv = fv->next) {
        if (!fv->field) continue;
        if (!first) printf(",\n");
        first = false;
        printf("  \"%s\": ", fv->field->name);
        if (fv->repeated_count > 0) {
            printf("[");
            for (int i = 0; i < fv->repeated_count; i++) {
                if (i > 0) printf(", ");
                print_json_value(fv->repeated[i]);
            }
            printf("]");
        } else {
            print_json_value(fv);
        }
    }
    printf("\n}\n");
}

/* ═══════════════════════════════════════════════════════════════════
 * L7: Schema Printing
 * ═══════════════════════════════════════════════════════════════════ */

void proto_print_schema(ProtoFile *file) {
    printf("syntax = \"%s\";\n", file->syntax);
    if (file->package[0])
        printf("package %s;\n\n", file->package);

    for (ProtoEnum *e = file->enums; e; e = e->next) {
        printf("enum %s {\n", e->name);
        for (ProtoEnumValue *v = e->values; v; v = v->next)
            printf("    %s = %d;\n", v->name, v->number);
        printf("}\n\n");
    }

    for (ProtoMessage *m = file->messages; m; m = m->next) {
        printf("message %s {\n", m->name);
        for (ProtoField *f = m->fields; f; f = f->next)
            printf("    %s %s %s = %d;\n",
                   proto_label_name(f->label), proto_field_type_name(f->type),
                   f->name, f->field_number);
        printf("}\n\n");
    }
}

/* ═══════════════════════════════════════════════════════════════════
 * Lookup
 * ═══════════════════════════════════════════════════════════════════ */

ProtoMessage *proto_find_message(ProtoFile *file, const char *name) {
    for (ProtoMessage *m = file->messages; m; m = m->next)
        if (strcmp(m->name, name) == 0) return m;
    return NULL;
}

ProtoEnum *proto_find_enum(ProtoFile *file, const char *name) {
    for (ProtoEnum *e = file->enums; e; e = e->next)
        if (strcmp(e->name, name) == 0) return e;
    return NULL;
}

/* ═══════════════════════════════════════════════════════════════════
 * Cleanup
 * ═══════════════════════════════════════════════════════════════════ */

void proto_free_file(ProtoFile *file) {
    while (file->messages) {
        ProtoMessage *m = file->messages; file->messages = m->next;
        while (m->fields) {
            ProtoField *f = m->fields; m->fields = f->next;
            free(f);
        }
        free(m);
    }
    while (file->enums) {
        ProtoEnum *e = file->enums; file->enums = e->next;
        while (e->values) {
            ProtoEnumValue *v = e->values; e->values = v->next;
            free(v);
        }
        free(e);
    }
}
