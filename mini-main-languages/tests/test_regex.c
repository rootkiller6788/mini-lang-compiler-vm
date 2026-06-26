/* test_regex.c */
#include "regex_lang.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

static int passed=0,failed=0;
#define T(n,e) do{ passed++; if(!(e)){ printf("FAIL: %s\n",n); failed++; passed--; } }while(0)

int main(void){
    printf("\n=== Regex Engine Tests ===\n\n");

    /* L1: Parse simple char */
    { int pos=0; RegexNode *n=regex_parse("a",&pos); T("parse a", n!=NULL && n->type==RN_CHAR && n->ch=='a'); regex_free_node(n); }

    /* L1: Parse dot */
    { int pos=0; RegexNode *n=regex_parse(".",&pos); T("parse .", n!=NULL && n->type==RN_DOT); regex_free_node(n); }

    /* L1: Parse alternation */
    { int pos=0; RegexNode *n=regex_parse("a|b",&pos); T("parse alt", n!=NULL && n->type==RN_UNION); regex_free_node(n); }

    /* L1: Parse star */
    { int pos=0; RegexNode *n=regex_parse("a*",&pos); T("parse star", n!=NULL && n->type==RN_STAR); regex_free_node(n); }

    /* L2: Compile pattern */
    RegexPattern *pat=regex_compile("a"); T("compile a", pat!=NULL); T("nfa states", pat->nfa.state_count > 1); T("nfa start", pat->nfa.start_state >= 0); T("nfa accept", pat->nfa.accept_state >= 0);

    /* L3: NFA match */
    T("match a", regex_match_nfa(pat,"a"));
    T("not match b", !regex_match_nfa(pat,"b"));
    int mlen;
    T("match len a in abc", regex_match_nfa_len(pat,"abc",&mlen) && mlen==1);
    regex_free_pattern(pat);

    /* L4: Nullable */
    { int pos=0; RegexNode *n=regex_parse("a*",&pos); T("a* nullable", regex_nullable(n)); regex_free_node(n); }
    { int pos=0; RegexNode *n=regex_parse("a",&pos); T("a not nullable", !regex_nullable(n)); regex_free_node(n); }

    /* L4: Derivative */
    { int pos=0; RegexNode *n=regex_parse("abc",&pos); RegexNode *d=regex_derivative(n,'a'); T("D_a(abc) not null", d!=NULL); regex_free_node(d); regex_free_node(n); }

    /* L5: DFA conversion */
    { int pos=0; RegexNode *n=regex_parse("a(b|c)*",&pos); NFA nfa=regex_build_nfa(n); DFA dfa=regex_nfa_to_dfa(&nfa); T("dfa states", dfa.num_states > 0); regex_free_node(n); }

    /* L5: DFA match */
    { int pos=0; RegexNode *n=regex_parse("abc",&pos); NFA nfa=regex_build_nfa(n); DFA dfa=regex_nfa_to_dfa(&nfa); T("dfa match abc", regex_match_dfa(&dfa,"abc")); T("dfa not match abd", !regex_match_dfa(&dfa,"abd")); regex_free_node(n); }

    /* L7: Search */
    pat=regex_compile("bc"); RegexMatch m; T("search bc in abc", regex_search(pat,"abc",&m) && m.start==1 && m.end==3); regex_free_pattern(pat);

    /* L7: match_all */
    pat=regex_compile("a"); int count=regex_match_all(pat,"abacada",NULL,NULL); T("match_all a count", count >= 3); regex_free_pattern(pat);

    printf("\n=== Results: %d passed, %d failed ===\n\n", passed, failed);
    return failed>0?1:0;
}
