/* Contextual UI policy must never rewrite stored routes or DSP parameters. */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
#include <assert.h>

static void engine(unsigned e) { set_engine_of(TSEL,e);apply_preset_to(TSEL,0);TSEL->engine=TSEL->eng_req; }
static void sweep(void)
{
    track_t *t=TSEL;
    int16_t before[P_COUNT];memcpy(before,t->p,sizeof before);
    for(unsigned d=0;d<MD_N;d++) {
        if(mod_dst_offer(t,d)) {
            assert(mod_dst_ok(t,d));assert(strcmp(mod_dst_name(t,d),"N/A"));
            if(d>=MD_E1&&d<MD_CLIP)assert(!strcmp(mod_dst_name(t,d),track_desc(t,P_E0+d-MD_E1)->label));
        }
        for(int sign=-1;sign<=1;sign+=2) {
            int n=mod_dst_turn(t,d,sign);
            assert(n==(int)d || (mod_dst_offer(t,n) && (sign>0?n>(int)d:n<(int)d)));
        }
    }
    int d=0,count=0;
    while(d<MD_N-1){int n=mod_dst_turn(t,d,1);assert(n>d&&++count<MD_N);d=n;}
    assert(mod_dst_turn(t,31,100000)==31&&mod_dst_turn(t,0,-100000)==0);
    assert(!memcmp(before,t->p,sizeof before));
}
int main(void)
{
    ui_power_on();unsigned configurations=0;
    for(unsigned e=0;e<NENGINES;e++)if(eng_ok(e)) {
        engine(e);
        for(int a=0;a<=ENGINES[e]->edit[0].max;a++) {
            TSEL->p[P_E0]=a;
            if(e==ENGI_LOFI)for(int b=5;b<7;b++){TSEL->p[P_E1]=b;sweep();configurations++;}
            else if(e==ENGI_INPUT)for(int b=0;b<2;b++){TSEL->p[P_E3]=b;sweep();configurations++;}
            else if(e==ENGI_VOICE)for(int b=0;b<LG_N;b++){TSEL->p[P_VLANG]=b;sweep();configurations++;}
            else{sweep();configurations++;}
        }
    }
    engine(ENGI_DRUM);assert(!mod_dst_offer(TSEL,MD_SHP)&&!mod_dst_offer(TSEL,MD_PITCH)&&!mod_dst_offer(TSEL,MD_CUT));
    assert(mod_dst_offer(TSEL,MD_FCUT)&&mod_dst_offer(TSEL,MD_E1)&&!strcmp(mod_dst_name(TSEL,MD_E1),"MORPH"));
    engine(ENGI_ADD);assert(!mod_dst_offer(TSEL,MD_CUT)&&!strcmp(mod_dst_name(TSEL,MD_SHP),"COMB"));
    engine(ENGI_INPUT);TSEL->p[P_E3]=0;assert(!mod_dst_offer(TSEL,MD_AMP)&&!mod_dst_offer(TSEL,MD_E1+5));
    TSEL->p[P_E3]=1;assert(mod_dst_offer(TSEL,MD_AMP)&&mod_dst_offer(TSEL,MD_E1+5));
    engine(ENGI_STRING);TSEL->p[P_E0]=SX_FDBK;
    assert(mod_dst_offer(TSEL,MD_E1+3)&&!strcmp(mod_dst_name(TSEL,MD_E1+3),"MOVE"));
    engine(ENGI_LOFI);TSEL->p[P_E1]=RW_BYTE;assert(!strcmp(mod_dst_name(TSEL,MD_E1+2),"ALGO"));
    TSEL->p[P_E1]=RW_FLOAT;assert(mod_dst_offer(TSEL,MD_E1+4)&&!strcmp(mod_dst_name(TSEL,MD_E1+4),"MOTION"));   /* (V10.6d: MATH, no chips) */
    engine(ENGI_FM6);assert(!mod_dst_offer(TSEL,MD_E1+7)&&!mod_dst_offer(TSEL,MD_SHP));
    assert(!strcmp(mod_dst_name(TSEL,MD_CUT),"MOD")&&!strcmp(mod_dst_name(TSEL,MD_SHP),"FB+MOD"));
    engine(ENGI_ANALOG);TSEL->p[P_E0]=7;TSEL->p[P_E3]=0;macro_v[0]=127;
    TSEL->p[P_M1SRC]=MS_M1;TSEL->p[P_M1DST]=MD_E1+3;TSEL->p[P_M1AMT]=-64;
    mod_begin(TSEL);assert(TSEL->p[P_E3]==-24);mod_end(TSEL);assert(TSEL->p[P_E3]==0);
    engine(ENGI_ANALOG);TSEL->p[P_M1SRC]=MS_T1;TSEL->p[P_M1DST]=MD_SHP;TSEL->p[P_M1AMT]=35;
    project_capture(&proj_scratch);assert(proj_pack(&proj_wire,&proj_scratch));
    assert(proj_import(&proj_scratch,&proj_wire,sizeof proj_wire));assert(proj_scratch.t[0].p[P_M1DST]==MD_SHP);
    /* Legacy no-op destinations are not silently converted on an engine switch. */
    TSEL->eng_req=ENGI_DRUM;assert(TSEL->p[P_M1DST]==MD_SHP&&!strcmp(mod_dst_name(TSEL,MD_SHP),"N/A"));
    engine(ENGI_DRUM);for(unsigned k=0;k<NTRK;k++)for(unsigned j=0;j<NMSLOT;j++)trk[k].p[P_M1SRC+3*j]=0;
    for(unsigned n=0;n<40;n++){macro_dice(3);for(unsigned k=0;k<NTRK;k++)for(unsigned j=0;j<NMSLOT;j++)if(trk[k].p[P_M1SRC+3*j])assert(mod_dst_offer(&trk[k],trk[k].p[P_M1DST+3*j]));macro_dice(2);}
    nav_put(SEC_LFO,"MOD");TSEL->p[P_M1DST]=MD_CLIP;frame();screen_save("build/song-check","matrix-context");
    printf("matrix context: %u mode configurations; navigation, aliases, legacy projects and DICE passed\n",configurations);
    return 0;
}
