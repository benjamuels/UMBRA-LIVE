#include <stdint.h>
struct track;
static void excite_turn(struct track *, const int16_t *, int32_t);
#define EDITOR_TEST_NO_MAIN 1
#include "editor_test.c"
#include <assert.h>
static const char *dst_name(unsigned id,unsigned value)
{
 uint8_t a[]={0,id};request(ED_DESC,a,2);
 const char *p=(const char*)host_wire+14;
 p+=strlen(p)+1;p+=strlen(p)+1;
 for(unsigned i=0;i<value;i++)p+=strlen(p)+1;
 assert((const uint8_t*)p<host_wire+host_wire_n);return p;
}
int main(void)
{
 reset();set_engine_of(TSEL,ENGI_DRUM);TSEL->engine=TSEL->eng_req;
 assert(!strcmp(dst_name(P_M1DST,MD_SHP),"OFF"));
 assert(!strcmp(dst_name(P_M1DST,MD_E1),"T:MORPH"));
 assert(!strcmp(dst_name(P_M1DST,MD_CLIP),"M:CLIP"));
 assert(!strcmp(dst_name(P_M1DST,MD_FCUT),"T:F.CUT"));
 /* Raw SET remains compatible with imported old routes, even if hidden now. */
 uint8_t set[]={0,P_M1DST,(8192+MD_SHP)&127,(8192+MD_SHP)>>7};request(ED_SET,set,4);
 assert(TSEL->p[P_M1DST]==MD_SHP&&!strcmp(dst_name(P_M1DST,MD_SHP),"V:N/A"));
 set_engine_of(TSEL,ENGI_INPUT);TSEL->engine=TSEL->eng_req;TSEL->p[P_E3]=0;TSEL->p[P_M1DST]=0;
 assert(!strcmp(dst_name(P_M1DST,MD_AMP),"OFF"));
 TSEL->p[P_E3]=1;assert(!strcmp(dst_name(P_M1DST,MD_AMP),"V:AMP"));
 set_engine_of(TSEL,ENGI_STRING);TSEL->engine=TSEL->eng_req;TSEL->p[P_E0]=SX_FDBK;
 assert(!strcmp(dst_name(P_M1DST,MD_E1+3),"T:MOVE"));
 puts("matrix editor: contextual names, scope, hidden options and raw legacy SET passed");return 0;
}

/* This protocol test writes only matrix fields; engine knob handling is exercised by UI tests. */
static void excite_turn(struct track *t, const int16_t *vp, int32_t old) { (void)old; assert(vp >= &t->p[P_M1SRC] && vp <= &t->p[P_M4AMT]); }
