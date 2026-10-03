#include "cam_rtsp_host.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#undef send
ssize_t cam_host_send(int fd,const void *data,size_t length,int flags) {
    const unsigned char *p=data;
    static unsigned udp_calls;
    int type=0;socklen_t n=sizeof(type);
    if(length>=12 && (p[1]&127)==26 && !getsockopt(fd,SOL_SOCKET,SO_TYPE,&type,&n) && type==SOCK_DGRAM) {
        const char *delay=getenv("CAM_HOST_UDP_DELAY_MS"),*fail=getenv("CAM_HOST_UDP_FAIL_AT");
        ++udp_calls;
        if(delay){unsigned ms=(unsigned)atoi(delay);struct timespec t={ms/1000,(long)(ms%1000)*1000000L};nanosleep(&t,NULL);}
        if(fail && udp_calls==(unsigned)atoi(fail)){errno=ENOBUFS;return -1;}
    }
    return send(fd,data,length,flags);
}
struct host_thread { void (*entry)(void *); void *arg; pthread_t thread; };
static uint8_t *jpeg;
static size_t jpeg_size;
static volatile sig_atomic_t quitting;
static struct timespec origin;
static pthread_mutex_t capture_critical=PTHREAD_MUTEX_INITIALIZER;
extern int rtsp_cam(int argc, char **argv);
static void quit(int sig) { (void)sig; quitting = 1; }
uint32_t rt_tick_get(void) {
    struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t);
    return (uint32_t)((t.tv_sec-origin.tv_sec)*1000+(t.tv_nsec-origin.tv_nsec)/1000000);
}
void rt_kprintf(const char *fmt, ...) {
    va_list args; va_start(args,fmt); vprintf(fmt,args); va_end(args); fflush(stdout);
}
rt_thread_t rt_thread_find(const char *name) { (void)name; return NULL; }
rt_thread_t rt_thread_create(const char *name, void (*entry)(void *), void *arg,
                            unsigned stack,unsigned priority,unsigned slice) {
    rt_thread_t t=calloc(1,sizeof(*t));
    (void)name;(void)stack;(void)priority;(void)slice;
    if(t) { t->entry=entry;t->arg=arg; } return t;
}
static void *start(void *arg) { rt_thread_t t=arg;t->entry(t->arg); free(t);return NULL; }
int rt_thread_startup(rt_thread_t t) { return pthread_create(&t->thread,NULL,start,t); }
void rt_thread_delete(rt_thread_t t) { free(t); }
struct host_mutex {pthread_mutex_t mutex;};
struct host_sem {pthread_mutex_t mutex;pthread_cond_t cond;unsigned count;};
rt_mutex_t rt_mutex_create(const char *name,int flags){rt_mutex_t m=malloc(sizeof(*m));(void)name;(void)flags;if(m)pthread_mutex_init(&m->mutex,NULL);return m;}
int rt_mutex_take(rt_mutex_t m,int timeout){(void)timeout;return pthread_mutex_lock(&m->mutex);}
int rt_mutex_release(rt_mutex_t m){return pthread_mutex_unlock(&m->mutex);}
int rt_mutex_delete(rt_mutex_t m){pthread_mutex_destroy(&m->mutex);free(m);return 0;}
rt_sem_t rt_sem_create(const char *name,unsigned count,int flags){rt_sem_t s=malloc(sizeof(*s));(void)name;(void)flags;if(s){pthread_mutex_init(&s->mutex,NULL);pthread_cond_init(&s->cond,NULL);s->count=count;}return s;}
int rt_sem_take(rt_sem_t s,int timeout){
    int result=0;struct timespec end;clock_gettime(CLOCK_REALTIME,&end);
    if(timeout>0){end.tv_sec+=timeout/1000;end.tv_nsec+=(timeout%1000)*1000000L;if(end.tv_nsec>=1000000000L){++end.tv_sec;end.tv_nsec-=1000000000L;}}
    pthread_mutex_lock(&s->mutex);
    while(!s->count && !result){
        if(timeout==0){result=ETIMEDOUT;break;}
        if(timeout<0)result=pthread_cond_wait(&s->cond,&s->mutex);
        else result=pthread_cond_timedwait(&s->cond,&s->mutex,&end);
    }
    if(!result)--s->count;
    pthread_mutex_unlock(&s->mutex);return result?-1:0;
}
int rt_sem_release(rt_sem_t s){pthread_mutex_lock(&s->mutex);++s->count;pthread_cond_signal(&s->cond);pthread_mutex_unlock(&s->mutex);return 0;}
int rt_sem_delete(rt_sem_t s){pthread_cond_destroy(&s->cond);pthread_mutex_destroy(&s->mutex);free(s);return 0;}
void rt_enter_critical(void){pthread_mutex_lock(&capture_critical);}
void rt_exit_critical(void){pthread_mutex_unlock(&capture_critical);}
void rt_thread_delay(unsigned ticks){struct timespec t={ticks/1000,(long)(ticks%1000)*1000000L};nanosleep(&t,NULL);}
unsigned rt_tick_from_millisecond(unsigned ms){return ms;}
void video_buffer_get_stats(VIDEO_BUFFER_STATS_ST *stats) { memset(stats,0,sizeof(*stats)); }
void video_transfer_get_stats(VIDEO_TRANSFER_STATS_ST *stats) { memset(stats,0,sizeof(*stats)); }
int video_buffer_open(void) { return 0; }
uint32_t video_buffer_read_frame_timeout(uint8_t *data,uint32_t cap,uint32_t timeout) {
    struct timespec wait={0,50000000}; (void)timeout; nanosleep(&wait,NULL);
    if(jpeg_size+5>cap)return 0;
    memcpy(data,jpeg,jpeg_size); memset(data+jpeg_size,0,5); return jpeg_size+5;
}
int main(int argc,char **argv) {
    FILE *f; char *start_args[]={"rtsp_cam","start"}, *stop_args[]={"rtsp_cam","stop"};
    if(argc!=2)return 2;
    f=fopen(argv[1],"rb"); if(!f)return 3;
    fseek(f,0,SEEK_END); jpeg_size=ftell(f);rewind(f);
    jpeg=malloc(jpeg_size);if(!jpeg)return 4;
    if(fread(jpeg,1,jpeg_size,f)!=jpeg_size)return 5;fclose(f);
    clock_gettime(CLOCK_MONOTONIC,&origin);
    signal(SIGINT,quit);signal(SIGTERM,quit);signal(SIGPIPE,SIG_IGN);
    rtsp_cam(2,start_args);
    while(!quitting && rt_tick_get()<60000) { struct timespec wait={0,100000000};nanosleep(&wait,NULL); }
    {char *status[]={"rtsp_cam","status"};rtsp_cam(2,status);}
    rtsp_cam(2,stop_args); { struct timespec wait={3,0};nanosleep(&wait,NULL); }
    free(jpeg);return 0;
}
