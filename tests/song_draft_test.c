/* Generated RAM workspace and explicit LOOP/SONG saves against real NOR model. */
#define PERSISTENCE_TEST_NO_MAIN 1
#include "persistence_test.c"
#include <assert.h>
static uint8_t old_flash[8*ST_SECTOR];
static song_index_t saved_index;
static project_store_t draft_before[4];
static uint32_t object(unsigned n,unsigned s){return n?OBJ_SONG1+(n-1)*4+s:OBJ_PROJECT0+s;}
static void draft_music(unsigned note)
{
 for(unsigned s=0;s<4;s++) {
  trk[0].step[0]=(step_t){{note+s},1,ST_NOTE,0,100};section_store(s);
 }
 chain_defaults(&chain_config);chain_config.count=4;live.mode=1;
 for(unsigned s=0;s<4;s++)chain_config.row[s]=(chain_row_t){s,1};
 dice_secs=15;section_load(0);
}
static int saved_note(unsigned n,unsigned s)
{
 int z=st_load(object(n,s),&proj_wire,sizeof proj_wire);
 if(z<=0)return -1;
 assert(proj_import(&proj_scratch,&proj_wire,sizeof proj_wire));return proj_scratch.t[0].step[0].note[0];
}
int main(void)
{
 song_fixture();
 memcpy(draft_before,proj_slot,sizeof draft_before);
 erase_error=1;
 assert(song_draft_begin()&&!song_draft&&!memcmp(draft_before,proj_slot,sizeof draft_before));
 erase_error=0;
 transport_req=1;assert(song_draft_begin()&&!song_draft);transport_req=0;
 puts("draft: failed checkpoint and pending PLAY preserve the original workspace");
 assert(!song_draft_begin()&&song_draft);
 memcpy(old_flash,nor+0x97000,sizeof old_flash);
 assert(st_load(OBJ_SONGIDX,&saved_index,sizeof saved_index)==sizeof saved_index);
 unsigned writes=erases;
 draft_music(64);song_poll();fm1_ms+=100000;autosave_poll();
 assert(erases==writes&&!memcmp(old_flash,nor+0x97000,sizeof old_flash));
 for(unsigned n=0;n<4;n++)assert(saved_note(0,n)==48+(int)n);
 assert(!song_draft_begin());draft_music(72);song_poll();
 assert(erases==writes&&!memcmp(old_flash,nor+0x97000,sizeof old_flash));
 puts("draft: regeneration and idle polling never write saved projects or autosave");
 assert(!song_save_as(1,"FULL SONG"));
 for(unsigned n=0;n<4;n++)assert(saved_note(1,n)==72+(int)n);
 assert(st_load(OBJ_SONGIDX,&proj_wire,sizeof saved_index)==sizeof saved_index);
 assert(!memcmp(&((song_index_t*)&proj_wire)->rows[0],&saved_index.rows[0],sizeof(chain_config_t)));
 assert(!memcmp(old_flash,nor+0x97000,sizeof old_flash));
 assert(song_draft&&chain_config.count==4);
 puts("draft: saving a full song elsewhere preserves the original project and current draft");
 song_save_loop=1;assert(!section_load(2));assert(!song_save_as(2,"JUST LOOP"));
 assert(saved_note(2,0)==74);for(unsigned n=1;n<4;n++)assert(saved_note(2,n)==-1);
 assert(song_idx.rows[2].count==0&&chain_config.count==4&&song_draft);
 puts("draft: LOOP writes current music to A, empties B-D and saves no arrangement");
 song_save_loop=0;assert(!song_save_as(0,"OVERWRITE"));
 for(unsigned n=0;n<4;n++)assert(saved_note(0,n)==(n==2?74:72+(int)n));
 assert(song_draft&&chain_config.count==4);
 memcpy(draft_before,proj_slot,sizeof draft_before);
 erase_error=1;assert(song_save_as(3,"FAIL")&&song_draft&&!memcmp(draft_before,proj_slot,sizeof draft_before));erase_error=0;
 assert(!song_save_as(3,"RETRY"));
 puts("draft: same-project overwrite is explicit; failed saves retain the draft for retry");
 assert(!song_erase_parts(1,0));for(unsigned n=0;n<4;n++)assert(saved_note(1,n)==-1);
 assert(song_draft&&!memcmp(draft_before,proj_slot,sizeof draft_before));
 assert(!song_erase_parts(song_cur,0)&&song_draft&&chain_config.count==4);
 assert(!memcmp(draft_before,proj_slot,sizeof draft_before));
 for(unsigned n=0;n<4;n++)assert(saved_note(0,n)==-1);
 song.playing=1;assert(song_select(3)&&song_draft);song.playing=0;
 writes=erases;assert(!project_erase(1));assert(!project_used(1)&&erases==writes);
 for(unsigned r=0;r<chain_config.count;r++)assert(chain_config.row[r].slot!=1);
 puts("erase: deleting either saved project preserves the draft; draft part deletion is RAM-only");
 assert(!song_select(3)&&!song_draft&&song_cur==3&&chain_config.count==4);
 assert(!song_erase_parts(3,0)&&!song_draft&&!chain_config.count);
 for(unsigned n=0;n<4;n++)assert(!project_used(n)&&saved_note(3,n)==-1);
 assert(seq_is_empty(&trk[0]));
 song_idx.rows[0].count=4; song_index_load(); song_index_boot();assert(!chain_config.count);
 puts("erase: deleting the open project survives reload and clears its live sequence and arrangement");
 song_fixture();song_save_loop=1;assert(!song_save_as(0,"LOOP IN PLACE"));
 assert(saved_note(0,0)==90&&!chain_config.count&&live.cur==0);
 for(unsigned k=1;k<4;k++)assert(!project_used(k)&&saved_note(0,k)==-1);
 trk[0].step[0].note[0]=12;ui_prefs|=PREF_RESTORE_ON;
 song_index_load();song_index_boot();
 assert(autosave_boot(1)&&trk[0].step[0].note[0]==90&&!live.mode&&!chain_config.count);
 puts("save: in-place LOOP also updates RESTORE LAST; reboot cannot restore stale music");
 song_fixture();assert(!song_draft_begin());draft_music(65);
 assert(!song_save_as(song_cur,"DRAFT BOOT"));
 song_draft=0;trk[0].step[0].note[0]=12;ui_prefs|=PREF_RESTORE_ON;
 song_index_load();song_index_boot();
 assert(autosave_boot(1)&&trk[0].step[0].note[0]==65&&live.mode&&chain_config.count==4);
 puts("save: in-place draft SONG restores its music and arrangement after reboot");
 song_fixture();assert(!song_draft_begin());draft_music(65);
 assert(!chain_prepare());events_block(CTL);song_poll();
 events_block(chain_bar());song_poll();assert(chain.running&&live.cur==1);
 seq_stop();song_poll();assert(live.cur==0&&trk[0].step[0].note[0]==65);
 assert(!song_save_as(1,"AFTER PLAY"));
 for(unsigned k=0;k<4;k++)assert(saved_note(1,k)==65+(int)k);
 puts("save: PLAY then STOP then SONG save preserves every distinct section");
 song_fixture();memcpy(draft_before,proj_slot,sizeof draft_before);erase_error=1;
 assert(project_erase(0)&&!memcmp(draft_before,proj_slot,sizeof draft_before));erase_error=0;
 assert(!project_erase(1));assert(!project_used(1));
 for(unsigned r=0;r<chain_config.count;r++)for(unsigned t=0;t<NTRK;t++)assert(row_src(r,t,chain_config.row[r].slot)!=1);
 puts("erase: failed writes keep RAM; deleting a source part removes dependent rows");
 return 0;
}
