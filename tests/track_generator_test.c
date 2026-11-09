/* SPDX-License-Identifier: GPL-3.0-only */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

/* Differential check against the previous period formula, including counter
 * wrap, stopped transport and S&H. This is the only changed DSP calculation. */
static int drum_motion_reference(void)
{
    int ok=1;unsigned cases=0;
    static const uint8_t q[9]={0,64,32,16,8,4,2,1,1};
    for(unsigned bpm=20;bpm<=300;bpm++) for(unsigned rate=1;rate<=8;rate++)
        for(unsigned shape=0;shape<5;shape++) for(unsigned play=0;play<2;play++) {
            song.g[G_BPM]=(int16_t)bpm;song.playing=(uint8_t)play;
            dx_mot[XM_RATE]=(uint8_t)rate;dx_mot[XM_DPTH]=127;dx_mot[XM_SHPE]=(uint8_t)shape;
            ms_clock=0xAABBCDDu;dx_mot_free=UINT32_MAX-31u;
            dx_mot_cyc=UINT32_MAX;dx_mot_rng=42;dx_mot_sh=57;
            uint32_t len=beat_samples()*q[rate]/4u;
            if(rate==8) len=beat_samples()/8u;
            if(len<64u) len=64u;
            uint32_t pos=play?ms_clock:32u,cyc=pos/len;
            uint32_t ph=(uint32_t)(((uint64_t)(pos%len)<<32)/len);
            int32_t w=shape==0?osc_sine(ph):shape==1?(ph<0x80000000u?(int32_t)(ph>>15)-32768:98303-(int32_t)(ph>>15)):
                      shape==2?(int32_t)(ph>>16)-32768:shape==3?32767-(int32_t)(ph>>16):
                      (int32_t)((42u*1664525u+1013904223u)>>16)-32768;
            dx_mot_block(64);
            ok &= dx_mot_cyc==cyc&&dx_mot_v==(w*127)>>16&&dx_mot_free==32u;
            cases++;
        }
    printf("DRUM motion: %u old-formula comparisons\n",cases);
    return ok;
}

static int verify(track_t *t,unsigned genre,unsigned role,unsigned len)
{
    static project_t before,after;
    unsigned k=trk_index(t),i,j;int ok=1;
    song.sel=(uint8_t)k;t->p[P_SLEN]=(int16_t)len;t->seq_fit=128u|16u;
    t->p[P_LEVEL]=75;t->p[P_SDIV]=5;t->p[P_REV]=17;
    for(i=len;i<NSTEP;i++) t->step[i]=(step_t){{75},1,ST_NOTE,0,96};
    motion_clear(t);motion_set_lock(t,0,P_E0,20);
    tg_config[k]=(tg_config_t){367,(uint8_t)genre,(uint8_t)role,60,70};
    project_capture(&before);
    __typeof__(gp) p=gp;__typeof__(gs) s=gs;__typeof__(gx) x=gx;ggenre_t g=gmut;uint32_t q=gq;
    char name[20];memcpy(name,gmut_name,20);
    shift_valid[0][k]=shift_valid[1][k]=1;
    t->jump_to=5;t->rev=1;
    trackgen_make();project_capture(&after);
    ok &= !shift_valid[0][k]&&!shift_valid[1][k];
    ok &= !t->jump_to&&!t->rev;
    ok &= !memcmp(&p,&gp,sizeof p)&&!memcmp(&s,&gs,sizeof s)&&!memcmp(&x,&gx,sizeof x)&&
          !memcmp(&g,&gmut,sizeof g)&&q==gq&&!memcmp(name,gmut_name,20);
    ok &= !memcmp(before.g,after.g,sizeof before.g)&&before.sel==after.sel&&
          !memcmp(&before.chain,&after.chain,sizeof before.chain)&&!memcmp(&before.motion,&after.motion,sizeof before.motion);
    for(i=0;i<NTRK;i++) {
        ok &= !memcmp(before.t[i].p,after.t[i].p,sizeof before.t[i].p);
        if(i!=k) ok &= !memcmp(before.t[i].step,after.t[i].step,sizeof before.t[i].step);
    }
    ok &= t->seq_fit==(128u|16u)&&song.playing;
    for(i=0;i<NSTEP;i++) {
        const step_t *st=&t->step[i];ok &= st->n<=4&&st->time<=ST_REST;
        if(i>=len) ok &= !memcmp(st,&before.t[k].step[i],sizeof *st);
        if(!drum_track(t)) for(j=0;j<st->n&&i<len;j++) ok &= st->note[j]==scale_snap(t,st->note[j]);
    }
    step_t first[NSTEP];memcpy(first,t->step,sizeof first);trackgen_make();
    ok &= !memcmp(first,t->step,sizeof first);
    return ok;
}
int main(void)
{
    int bad=0,ok=1;unsigned count=0;
    ui_power_on();
    bad+=check("DRUM motion period/wave is bit exact to the prior formula",drum_motion_reference());
    ui_power_on();song.playing=1;
    {
        const uint8_t *packed[]={ABOUT_PROJECT_PACK,ABOUT_CREDITS_PACK};
        const char *raw[]={ABOUT_PROJECT,ABOUT_CREDITS};
        const size_t size[]={sizeof ABOUT_PROJECT,sizeof ABOUT_CREDITS};
        char text[ABOUT_MAX];
        for(unsigned k=0;k<2;k++) {
            char *d=text;unsigned n=packed[k][0]|(unsigned)packed[k][1]<<8;
            for(unsigned i=0;i<n;i++) d=about_byte(d,text+sizeof text,packed[k][2+i]);
            ok &= d-text==(ptrdiff_t)size[k]&&!memcmp(raw[k],text,size[k]);
        }
        bad+=check("compressed ABOUT and credits retain every original byte",ok);
    }
    /* Start with an actual HOME song context, then generate into individual
     * engines while preserving that context and every other track. */
    song.playing=0;gp.genre=1;gp.form=0;gen_go();song.playing=1;
    for(unsigned e=0;e<NENGINES;e++) {
        if(!eng_ok(e)||e==ENGI_INPUT) continue;
        song.sel=1;set_engine_of(&trk[1],e);trk[1].engine=trk[1].eng_req;
        for(unsigned role=0;role<(e==ENGI_DRUM?1u:4u);role++)
            for(unsigned li=0;li<4;li++) {
                static const unsigned lens[]={1,7,18,64};
                ok &= verify(&trk[1],1u+(e+role)%NGENRE,role,lens[li]);count++;
            }
    }
    bad+=check("track GEN preserves HOME, other tracks, sounds, locks, FIT and live transport",ok);
    printf("track GEN: %u engine/role/length configurations\n",count);
    ok=1;song.sel=0;set_engine_of(&trk[0],ENGI_DRUM);trk[0].engine=ENGI_DRUM;
    for(unsigned g=0;g<=NGENRE;g++) ok &= verify(&trk[0],g,0,16);
    ok &= verify(&trk[0],GEN_EXPLORE,0,16);
    bad+=check("track GEN all drum genres and EXPLORE are bounded, deterministic and isolated",ok);
    ui_power_on();song.sel=1;set_engine_of(&trk[1],ENGI_ANALOG);trk[1].engine=ENGI_ANALOG;
    ok=1;song.playing=1;
    for(unsigned scale=0;scale<NELEM(SCALE_MASK);scale++) {
        trk[1].p[P_SCALE]=(int16_t)scale;
        for(unsigned role=0;role<4;role++) ok &= verify(&trk[1],GEN_EXPLORE,role,18);
    }
    bad+=check("track GEN respects every selected scale, including non-heptatonic scales",ok);
    ui_power_on();song.sel=1;set_engine_of(&trk[1],ENGI_ANALOG);trk[1].engine=ENGI_ANALOG;
    song.playing=1;
    static project_t before,generated,reloaded;static project_store_t stored;
    project_capture(&before);trackgen_make();project_capture(&generated);
    ok=proj_pack(&stored,&generated)&&proj_import(&reloaded,&stored,sizeof stored);
    ok &= !memcmp(reloaded.t[1].step,generated.t[1].step,sizeof generated.t[1].step);
    bad+=check("track GEN result round trips through the existing project format",ok);
    ok=!undo_do();project_capture(&reloaded);
    for(unsigned k=0;k<NTRK;k++) ok &= !memcmp(before.t[k].step,reloaded.t[k].step,sizeof before.t[k].step);
    bad+=check("track GEN existing UNDO restores all previous patterns",ok);
    chain.armed=1;project_capture(&before);trackgen_make();project_capture(&generated);
    ok=!memcmp(&before,&generated,sizeof before);chain.armed=0;
    bad+=check("track GEN cannot overwrite an armed arrangement",ok);
    song.sel=1;set_engine_of(&trk[1],ENGI_ANALOG);trk[1].engine=ENGI_ANALOG;
    song.playing=0;go_page(GR_TRACKGEN);ui.force=1;frame();
    ok=page_visible(ui.page)&&act_col()==5&&strstr(ui.col[0],"GENRE");
    unsigned previous=tg_config[1].role;edit_param(2,1);ok &= tg_config[1].role==(previous<3?previous+1:3);
    bad+=check("track GEN final SEQ page exposes genre, seed, role and energy",ok);
    set_engine_of(&trk[1],ENGI_INPUT);trk[1].engine=ENGI_INPUT;
    bad+=check("track GEN hidden for INPUT, which has no pitched sequence engine",!page_visible(ui.page));
    printf("track GEN: track_t %zu B, main stack limit 15.75 KiB; %s\n",sizeof(track_t),bad?"FAILED":"all ok");
    return bad!=0;
}
