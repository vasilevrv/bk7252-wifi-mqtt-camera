/* Execute the real SDK assembler/read function with deterministic packet
 * arrival inside its semaphore wait. No duplicated model of the receiver. */
#define CAM_VIDEO_BUFFER_HOST
#include "../bdk_rtt/beken378/app/video_work/video_buffer.c"
#include <assert.h>
static void (*arrive)(void);
static unsigned waits;
static const UINT8 image[]={0xff,0xd8,0xff,0xd9,0,4,0,0,0};
static UINT8 old_slot[32],new_slot[32],saved[32];
int rtos_init_semaphore(beken_semaphore_t *s,int n){(void)n;*s=calloc(1,sizeof(int));return *s?0:-1;}
int rtos_deinit_semaphore(beken_semaphore_t *s){free(*s);*s=NULL;return 0;}
int rtos_set_semaphore(beken_semaphore_t *s){**s=1;return 0;}
int rtos_get_semaphore(beken_semaphore_t *s,UINT32 timeout){
    if(!**s && timeout){++waits;if(arrive)arrive();}
    if(**s){**s=0;return 0;}return -1;
}
int video_transfer_init(TVIDEO_SETUP_DESC_ST *s){(void)s;return 0;}
int video_transfer_deinit(void){return 0;}
static void pkt(unsigned id,unsigned seq,unsigned eof,const UINT8 *p,unsigned n){
    UINT8 data[100];assert(n<=96);data[0]=id;data[1]=eof;data[2]=0;data[3]=seq;memcpy(data+4,p,n);
    assert(video_buffer_recv_video_data(data,n+4)==(int)n+4);
}
static void partial(void){pkt(1,1,0,image,2);}
static void resume_then_good(void){
    /* Old timed-out frame continuation must neither write its old slot nor
     * complete a frame in the new slot. */
    pkt(1,2,1,image+2,sizeof(image)-2);
    assert(!memcmp(old_slot,saved,sizeof(saved)));
    assert(g_vbuf->frame_len==0);
    pkt(2,1,1,image,sizeof(image));
    /* A subsequent SOF cannot overwrite a completed, unclaimed frame. */
    UINT8 bad[9]={0};pkt(3,1,1,bad,sizeof(bad));
    assert(!memcmp(new_slot,image,sizeof(image)));
}
static void overflow(void){UINT8 big[40]={0};pkt(4,1,0,big,40);pkt(5,1,1,image,sizeof(image));}
static void invalid(void){UINT8 bad[9]={0};pkt(6,1,1,bad,9);}
static void gap(void){pkt(7,1,0,image,2);pkt(7,3,1,image+2,7);}
static void tiny(void){pkt(8,1,1,image,2);}
static void good(void){pkt(9,1,1,image,sizeof(image));}
int main(void){
    assert(!video_buffer_open());memset(old_slot,0xa5,sizeof(old_slot));
    arrive=partial;assert(!video_buffer_read_frame_timeout(old_slot,sizeof(old_slot),200));
    assert(g_vbuf->buf_ptr==NULL && g_vbuf->buf_base==NULL && g_vbuf->start_buf==BUF_STA_DONE);
    memcpy(saved,old_slot,sizeof(saved));memset(new_slot,0x5a,sizeof(new_slot));
    arrive=resume_then_good;assert(video_buffer_read_frame_timeout(new_slot,sizeof(new_slot),200)==sizeof(image));
    assert(!memcmp(old_slot,saved,sizeof(saved)));
    arrive=overflow;assert(!video_buffer_read_frame_timeout(new_slot,sizeof(new_slot),200));
    arrive=invalid;assert(!video_buffer_read_frame_timeout(new_slot,sizeof(new_slot),200));
    arrive=gap;assert(!video_buffer_read_frame_timeout(new_slot,sizeof(new_slot),200));
    arrive=tiny;assert(!video_buffer_read_frame_timeout(new_slot,sizeof(new_slot),200));
    arrive=good;assert(video_buffer_read_frame_timeout(new_slot,sizeof(new_slot),200)==sizeof(image));
    VIDEO_BUFFER_STATS_ST stats;video_buffer_get_stats(&stats);
    assert(stats.valid==2 && stats.overflows==1 && stats.invalid==2 && stats.gaps==1 && stats.timeouts==1);
    assert(stats.max_frame_len==sizeof(image) && waits==7);
    assert(!video_buffer_close());
    puts("PASS actual SDK video_buffer: timeout/rearm ignores stale packets; immutable completed buffer; overflow/gap/invalid wake immediately; recovery and exact counters");
    return 0;
}
