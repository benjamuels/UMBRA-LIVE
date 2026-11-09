/* SPDX-License-Identifier: GPL-3.0-only */
static int split_storage_test(void)
{
    int bad=0; uint32_t sig;
    const uint8_t conf[8]={1,13,0,1,255,1,9,60};
    reset();memset(split_cfg,0,sizeof split_cfg);
    memcpy(split_cfg,conf,sizeof conf);
    bad += check("SPLIT: write actual NOR flash", !project_save(0));
    memset(split_cfg,0,sizeof split_cfg); project_load(0);transport_req=0;
    bad += check("SPLIT: project restores both keyboards and octaves", !memcmp(split_cfg,conf,sizeof conf));
    sig=autosave_sig();split_cfg[0]=0;
    bad += check("SPLIT: edits change autosave signature",sig!=autosave_sig());
    memset(split_cfg,0,sizeof split_cfg);project_capture(&proj_scratch);
    bad += check("SPLIT: legacy payload without KS decodes OFF",proj_pack(&proj_wire,&proj_scratch) && proj_import(&proj_scratch,&proj_wire,sizeof proj_wire) && !proj_scratch.split[0]);
    memcpy(split_cfg,conf,sizeof conf);project_capture(&proj_scratch);
    proj_scratch.split[2]=NTRK;
    bad += check("SPLIT: invalid destinations refuse serialization", !proj_pack(&proj_wire,&proj_scratch));
    memset(split_cfg,0,sizeof split_cfg);
    return bad;
}
