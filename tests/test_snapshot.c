#include "mqtt_host/rtthread.h"
#include <pthread.h>
#include <assert.h>
#include <stdatomic.h>
static pthread_mutex_t lock=PTHREAD_MUTEX_INITIALIZER;
static int live_allocs,fail_after=2;
static void *snapshot_alloc(size_t size){void *p;if(fail_after==0)return NULL;if(fail_after>0)--fail_after;p=malloc(size);if(p)++live_allocs;return p;}
static void snapshot_free(void *p){if(p){--live_allocs;free(p);}}
#undef rt_malloc
#undef rt_free
#define rt_malloc snapshot_alloc
#define rt_free snapshot_free
void rt_enter_critical(void){pthread_mutex_lock(&lock);}
void rt_exit_critical(void){pthread_mutex_unlock(&lock);}
#include "../bdk_rtt/test/cam_snapshot.c"
static atomic_int stop;
static void *producer(void *arg){
    uint8_t frame[CAM_SNAPSHOT_MAX+5];unsigned count=0;(void)arg;
    while(!atomic_load(&stop)){
        memset(frame,++count&255,sizeof(frame));frame[0]=255;frame[1]=216;
        frame[CAM_SNAPSHOT_MAX-2]=255;frame[CAM_SNAPSHOT_MAX-1]=217;
        cam_snapshot_offer(frame,sizeof(frame),count);
    }return NULL;
}
int main(void){
    pthread_t thread;unsigned i;uint32_t tick;
    assert(cam_snapshot_enable()==-1 && live_allocs==0);
    fail_after=-1;assert(!cam_snapshot_enable() && live_allocs==4);
    assert(!pthread_create(&thread,NULL,producer,NULL));
    for(i=0;i<2000;++i){
        size_t length,offset=0;unsigned char expected;
        cam_snapshot_request();while(!(length=cam_snapshot_take(&tick)))usleep(10);
        assert(length==CAM_SNAPSHOT_MAX);expected=tick&255;
        while(offset<length){
            size_t n=0,j;const uint8_t *p=cam_snapshot_piece(offset,&n);
            assert(p && n && n<=CAM_SNAPSHOT_CHUNK);
            for(j=0;j<n;++j){size_t pos=offset+j;unsigned char want=expected;
                if(pos==0 || pos==length-2)want=255;
                if(pos==1)want=216;if(pos==length-1)want=217;
                assert(p[j]==want);
            }
            /* Repeated request cannot permit overwrite during transmission. */
            cam_snapshot_request();if(offset==0)usleep(20);offset+=n;
        }
        cam_snapshot_release();
    }
    atomic_store(&stop,1);pthread_join(thread,NULL);
    puts("PASS snapshot: failed allocation cleanup;2000 concurrent full48KiB copies/reads; immutable leased payload; all4 chunk boundaries; repeated requests cannot overwrite reading image");return 0;
}
