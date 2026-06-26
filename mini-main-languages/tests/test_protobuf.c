/* test_protobuf.c */
#include "protobuf_lang.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

static int passed=0,failed=0;
#define T(n,e) do{ passed++; if(!(e)){ printf("FAIL: %s\n",n); failed++; passed--; } }while(0)

int main(void){
    printf("\n=== Protobuf/IDL Tests ===\n\n");

    /* L1: Init */
    ProtoFile f; proto_init_file(&f);
    T("init syntax", strcmp(f.syntax,"proto3")==0);

    /* L2: Parse schema */
    const char *src=
        "syntax = \"proto3\";\n"
        "package test;\n"
        "message Person {\n"
        "    optional string name = 1;\n"
        "    optional int32  id   = 2;\n"
        "}\n"
        "enum Color { RED=0; GREEN=1; BLUE=2; }\n";
    T("parse protobuf", proto_parse(&f,src));

    /* L4: Validation */
    T("validate schema", proto_validate(&f));

    /* L5: Code generation */
    T("gen c header", proto_generate_c_header(&f,"_test_gen.h"));

    /* L5: Lookup */
    ProtoMessage *m=proto_find_message(&f,"Person");
    T("find Person", m!=NULL);
    ProtoEnum *e=proto_find_enum(&f,"Color");
    T("find Color", e!=NULL);

    /* L5: Wire type */
    T("int32 wire varint", proto_wire_type(PF_INT32)==WIRE_VARINT);
    T("string wire len", proto_wire_type(PF_STRING)==WIRE_LEN);
    T("float wire 32bit", proto_wire_type(PF_FLOAT)==WIRE_32BIT);
    T("double wire 64bit", proto_wire_type(PF_DOUBLE)==WIRE_64BIT);

    /* L5: Varint encoding */
    ProtoBuffer *buf=proto_buffer_new();
    T("buf new", buf!=NULL);
    T("write varint 1", proto_buffer_write_varint(buf,1));
    T("write varint 300", proto_buffer_write_varint(buf,300));
    size_t off=0;
    uint64_t v;
    T("read varint 1", proto_buffer_read_varint(buf->data,buf->size,&off,&v) && v==1);
    T("read varint 300", proto_buffer_read_varint(buf->data,buf->size,&off,&v) && v==300);
    
    /* L5: Field serialization */
    ProtoBuffer *buf2=proto_buffer_new();
    T("write int32", proto_buffer_write_int32(buf2,1,42));
    T("write string", proto_buffer_write_string(buf2,2,"hello"));
    T("write bool", proto_buffer_write_bool(buf2,3,true));

    /* L5: Deserialize from wire bytes */
    int32_t i32;
    size_t off2=0;
    T("read int32 field", proto_buffer_read_int32(buf2->data,buf2->size,&off2,1,&i32) && i32==42);

    /* L5: Message serialize */
    ProtoMessageValue *msg=proto_msg_new();
    msg->schema=m;
    proto_msg_set_int32(msg,"id",123);
    proto_msg_set_string(msg,"name","Alice");
    ProtoBuffer *msg_buf=proto_buffer_new();
    T("serialize msg", proto_serialize_message(&f,"Person",msg,msg_buf));
    T("msg has bytes", msg_buf->size > 0);

    /* L5: Deserialize */
    ProtoMessageValue *msg2=proto_msg_new();
    T("deserialize msg", proto_deserialize_message(&f,"Person",msg_buf->data,msg_buf->size,msg2));
    T("msg2 has fields", msg2->fields != NULL);

    proto_msg_free(msg);
    proto_msg_free(msg2);
    proto_buffer_free(buf);
    proto_buffer_free(buf2);
    proto_buffer_free(msg_buf);
    proto_free_file(&f);
    remove("_test_gen.h");

    printf("\n=== Results: %d passed, %d failed ===\n\n", passed, failed);
    return failed>0?1:0;
}
