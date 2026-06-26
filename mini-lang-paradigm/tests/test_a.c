#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "fp_closure.h"
#include "oop_vtable.h"
static int spk=0;
static void* as(void* s, void** a) {(void)s;(void)a;spk++;return NULL;}
static void* ds(void* s, void** a) {(void)s;(void)a;spk+=10;return NULL;}
static void* addf(void** a) {int* r=malloc(sizeof(int));*r=*(int*)a[0]+*(int*)a[1];return r;}
static void* fah(void* a,void* v){*(int*)a+=*(int*)v;return a;}
static bool iep(void* v){return (*(int*)v%2)==0;}
int main(void) {
    printf("A"); fflush(stdout);
    Class* an=class_create("Animal",sizeof(Object));
    class_add_method(an,"speak",0,as);
    Object* ao=object_create(an); object_call_virtual(ao,"speak",NULL);
    class_destroy(an); object_destroy(ao);
    printf("B"); fflush(stdout);
    FPClosure* ad=fp_closure_create(addf,2,0); int x=3,y=5;
    int* s=(int*)fp_apply(ad,(void*[]){&x,&y}); free(s); fp_closure_destroy(ad);
    printf("C"); fflush(stdout);
    int vs[]={1,2,3}; FPList* l=NULL;
    for(int i=2;i>=0;i--) l=fp_cons(&vs[i],l);
    int z=0; fp_foldl(fah,&z,l); fp_list_destroy(l);
    printf(" DONE=%d\n",z);
    return 0;
}
