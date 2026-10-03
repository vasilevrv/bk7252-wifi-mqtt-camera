/* POSIX adapter for the actual firmware MQTT wrapper and SDK Paho source. */
#include "cam_mqtt_host.h"
#include <assert.h>
#include <pthread.h>
#include <stdarg.h>
#include <time.h>
#include "cam_snapshot.h"
#include "cam_stream_info.h"
static unsigned char *fixture;static size_t fixture_size;
void cam_stream_get_info(struct cam_stream_info *s){memset(s,0,sizeof(*s));s->source_frames=rt_tick_get()/128;s->captured=s->source_frames;}
struct host_mutex { pthread_mutex_t mutex; };
struct host_thread { void (*entry)(void *); void *arg; };
static pthread_mutex_t critical = PTHREAD_MUTEX_INITIALIZER;
static const char *config_path;
static char stored[544];
uint32_t rt_tick_get(void) { struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return (uint32_t)((uint64_t)t.tv_sec*1000+t.tv_nsec/1000000); }
uint32_t rt_tick_from_millisecond(int ms) {return ms;}
void rt_thread_delay(uint32_t ticks) {
    /* Accelerate only SDK reconnect sleep: 20s -> 0.2s. Network/tick timing is real. */
    usleep((ticks==20000?200:ticks)*1000);
}
static void *worker(void *p) { struct host_thread *t=p;t->entry(t->arg);free(t);return NULL; }
rt_thread_t rt_thread_create(const char *n,void (*entry)(void *),void *arg,unsigned stack,unsigned pri,unsigned slice) {
    struct host_thread *t=malloc(sizeof(*t));(void)n;(void)stack;(void)pri;(void)slice;
    if(t){t->entry=entry;t->arg=arg;}return t;
}
int rt_thread_startup(rt_thread_t t) {pthread_t p;int r=pthread_create(&p,NULL,worker,t);if(!r)pthread_detach(p);return r;}
void rt_thread_delete(rt_thread_t t) {free(t);}
rt_mutex_t rt_mutex_create(const char *n,int flags) {rt_mutex_t m=malloc(sizeof(*m));(void)n;(void)flags;if(m)pthread_mutex_init(&m->mutex,NULL);return m;}
int rt_mutex_delete(rt_mutex_t m) {pthread_mutex_destroy(&m->mutex);free(m);return 0;}
int rt_mutex_take(rt_mutex_t m,int timeout) {(void)timeout;return pthread_mutex_lock(&m->mutex);}
int rt_mutex_release(rt_mutex_t m) {return pthread_mutex_unlock(&m->mutex);}
rt_mq_t rt_mq_create(const char *n,unsigned size,unsigned count,int flags) {(void)n;(void)size;(void)count;(void)flags;return malloc(1);}
int rt_mq_delete(rt_mq_t q) {free(q);return 0;}
int rt_mq_control(rt_mq_t q,int cmd,void *arg) {(void)q;(void)cmd;(void)arg;return 0;}
int rt_mq_send(rt_mq_t q,void *data,unsigned size) {(void)q;(void)data;(void)size;return 0;}
int rt_mq_recv(rt_mq_t q,void *data,unsigned size,int timeout) {(void)q;(void)data;(void)size;(void)timeout;return -1;}
void rt_kprintf(const char *fmt,...) {va_list v;va_start(v,fmt);vprintf(fmt,v);fflush(stdout);va_end(v);}
void rt_enter_critical(void) {pthread_mutex_lock(&critical);}
void rt_exit_critical(void) {pthread_mutex_unlock(&critical);}
rt_device_t rt_device_find(const char *name) {return !strcmp(name,"w0")?(void *)1:NULL;}
int rt_device_control(rt_device_t d,int cmd,void *p) {static unsigned char mac[]={0,4,0xb2,8,0x7e,0x6c};(void)d;(void)cmd;memcpy(p,mac,6);return 0;}
int dfs_file_open(struct dfs_fd *fd,const char *path,int flags) {(void)flags;assert(!strcmp(path,"/sd/mqtt.conf"));fd->file=fopen(config_path,"rb");return fd->file?0:-1;}
int dfs_file_read(struct dfs_fd *fd,void *data,unsigned size) {size_t n=fread(data,1,size,fd->file);return ferror(fd->file)?-1:(int)n;}
int dfs_file_close(struct dfs_fd *fd) {return fclose(fd->file);}
char *ef_get_env(const char *key) {(void)key;return stored[0]?stored:NULL;}
EfErrCode ef_set_env(const char *key,const char *value) {(void)key;assert(strlen(value)<sizeof(stored));strcpy(stored,value);return EF_NO_ERR;}
EfErrCode ef_save_env(void) {return EF_NO_ERR;}
extern void cam_mqtt_configure(int);
extern void cam_mqtt_ensure(int,const char *);
extern int cam_mqtt(int,char **);
int main(int argc,char **argv) {
    unsigned i;FILE *f;assert(argc==3);config_path=argv[1];
    f=fopen(argv[2],"rb");assert(f);fseek(f,0,SEEK_END);fixture_size=ftell(f);rewind(f);fixture=calloc(1,fixture_size+5);assert(fixture);assert(fread(fixture,1,fixture_size,f)==fixture_size);fclose(f);
    cam_mqtt_configure(1);
    cam_mqtt_ensure(0,"");usleep(100000); /* no worker/socket before DHCP */
    for(i=0;i<300;++i){cam_mqtt_ensure(1,"192.168.40.126");cam_snapshot_offer(fixture,fixture_size+5,rt_tick_get());usleep(50000);}
    cam_mqtt(1,NULL);return 0;
}
