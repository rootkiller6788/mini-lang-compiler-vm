#include "protobuf_lang.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

void proto_init_file(ProtoFile *file) {
    strcpy(file->syntax, "proto3");
    file->package[0] = '\0';
    file->messages = NULL;
    file->enums = NULL;
}

static void skip_ws(const char *s, int *pos) {
    while (s[*pos] && isspace((unsigned char)s[*pos])) (*pos)++;
}

static char *read_ident(const char *s, int *pos, char *buf) {
    int i = 0; skip_ws(s, pos);
    while (isalnum((unsigned char)s[*pos]) || s[*pos] == '_' || s[*pos] == '.') {
        if (i < 127) buf[i++] = s[*pos]; (*pos)++;
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

const char *proto_field_type_name(ProtoFieldType t) {
    switch (t) {
        case PF_INT32:  return "int32";  case PF_INT64:  return "int64";
        case PF_STRING: return "string"; case PF_BOOL:   return "bool";
        case PF_FLOAT:  return "float";  case PF_DOUBLE: return "double";
        case PF_ENUM:   return "enum";   case PF_MESSAGE:return "message";
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
        case PF_INT32: case PF_ENUM: return "int32_t";
        case PF_INT64: return "int64_t";
        case PF_STRING: return "char*";
        case PF_BOOL: return "bool";
        case PF_FLOAT: return "float";
        case PF_DOUBLE: return "double";
        default: return "void*";
    }
}

static ProtoFieldType type_from_string(const char *s) {
    if (strcmp(s, "int32") == 0) return PF_INT32;
    if (strcmp(s, "int64") == 0) return PF_INT64;
    if (strcmp(s, "string") == 0) return PF_STRING;
    if (strcmp(s, "bool") == 0) return PF_BOOL;
    if (strcmp(s, "float") == 0) return PF_FLOAT;
    if (strcmp(s, "double") == 0) return PF_DOUBLE;
    return PF_MESSAGE;
}

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
            read_ident(source, &pos, msg->name);
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
                    strcpy(buf2, buf);
                }

                ProtoField *f = (ProtoField *)calloc(1, sizeof(ProtoField));
                strcpy(f->name, buf2);
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
            read_ident(source, &pos, e->name);
            e->values = NULL;
            e->next = file->enums;
            file->enums = e;
            match(source, &pos, "{");
            while (source[pos] && source[pos] != '}') {
                skip_ws(source, &pos);
                if (source[pos] == '}' || source[pos] == '\0') break;
                ProtoEnumValue *ev = (ProtoEnumValue *)calloc(1, sizeof(ProtoEnumValue));
                read_ident(source, &pos, ev->name);
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

bool proto_validate(ProtoFile *file) {
    for (ProtoMessage *m = file->messages; m; m = m->next) {
        bool seen[2048] = {false};
        for (ProtoField *f = m->fields; f; f = f->next) {
            if (f->field_number <= 0 || f->field_number >= 536870912)
                return false;
            if (f->field_number < 2048 && seen[f->field_number])
                return false;
            if (f->field_number < 2048)
                seen[f->field_number] = true;
            for (ProtoField *g = f->next; g; g = g->next)
                if (strcmp(f->name, g->name) == 0)
                    return false;
        }
    }
    for (ProtoEnum *e = file->enums; e; e = e->next) {
        for (ProtoEnumValue *v = e->values; v; v = v->next)
            for (ProtoEnumValue *w = v->next; w; w = w->next)
                if (strcmp(v->name, w->name) == 0)
                    return false;
    }
    return true;
}

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
            if (f->label == PROTO_REPEATED)
                fprintf(out, "    %s *%s;\n    int %s_count;\n", ct, f->name, f->name);
            else
                fprintf(out, "    %s %s;\n", ct, f->name);
        }
        fprintf(out, "} %s;\n\n", m->name);
    }

    fprintf(out, "#endif\n");
    fclose(out);
    return true;
}

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
