#include "cam_frame_pool.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void invariant(struct cam_frame_pool *p) {
    int a[]={p->writing,p->ready,p->reading};unsigned i,j;
    for(i=0;i<3;++i){assert(a[i]>=-1 && a[i]<CAM_FRAME_SLOTS);for(j=i+1;j<3;++j)assert(a[i]<0 || a[i]!=a[j]);}
}
int main(void) {
    struct cam_frame_pool p;unsigned i, serial=0, data[3]={0},held=0,completed=0, token=0;
    int slot;cam_pool_init(&p);
    assert(cam_pool_take(&p)==-1);
    slot=cam_pool_reserve(&p);assert(slot>=0);assert(cam_pool_reserve(&p)==-1);
    assert(!cam_pool_commit(&p,0,0));assert(p.ready<0 && !p.captured);
    srand(17);
    for(i=0;i<1000000;++i){
        int action=rand()%4;
        if(action==0 && p.writing<0){slot=cam_pool_reserve(&p);assert(slot>=0);data[slot]=++serial;}
        else if(action==1 && p.writing>=0){int signal=cam_pool_commit(&p,100,serial);if(signal)++token;}
        else if(action==2 && p.reading<0 && token){--token;slot=cam_pool_take(&p);assert(slot>=0);held=data[slot];assert(p.tick[slot]==held);}
        else if(action==3 && p.reading>=0){assert(data[p.reading]==held);cam_pool_release(&p,p.reading);++completed;}
        invariant(&p);
        if(p.reading>=0)assert(data[p.reading]==held); /* In-flight network JPEG is immutable. */
        assert(token==(unsigned)(p.ready>=0));
        assert(p.captured==completed+p.replaced+(unsigned)(p.reading>=0)+(unsigned)(p.ready>=0));
    }
    /* Disconnect/error releases the sending slot, so a new client can consume. */
    if(p.reading>=0)cam_pool_release(&p,p.reading);
    if(p.writing>=0)cam_pool_commit(&p,0,0);
    slot=cam_pool_reserve(&p);assert(slot>=0);cam_pool_commit(&p,123,42);
    slot=cam_pool_take(&p);assert(slot>=0 && p.length[slot]==123 && p.tick[slot]==42);
    cam_pool_release(&p,slot);
    puts("PASS: 1000000 producer/consumer interleavings, immutable sending buffer, latest-frame replacement, semaphore tokens, failed capture, disconnect/reuse");
    return 0;
}
