#ifndef CAM_RTSP_HOST_H
#define CAM_RTSP_HOST_H
#include <stdint.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
ssize_t cam_host_send(int,const void *,size_t,int);
#define send cam_host_send
#define RT_TICK_PER_SECOND 1000
#define RT_NULL NULL
#define RT_EOK 0
#define RT_WAITING_FOREVER -1
#define RT_IPC_FLAG_FIFO 0
struct host_mutex;struct host_sem;
typedef struct host_mutex *rt_mutex_t;
typedef struct host_sem *rt_sem_t;
rt_mutex_t rt_mutex_create(const char *,int);
int rt_mutex_take(rt_mutex_t,int);
int rt_mutex_release(rt_mutex_t);
int rt_mutex_delete(rt_mutex_t);
rt_sem_t rt_sem_create(const char *,unsigned,int);
int rt_sem_take(rt_sem_t,int);
int rt_sem_release(rt_sem_t);
int rt_sem_delete(rt_sem_t);
void rt_thread_delay(unsigned);
unsigned rt_tick_from_millisecond(unsigned);
struct host_thread;
typedef struct host_thread *rt_thread_t;
uint32_t rt_tick_get(void);
void rt_kprintf(const char *format, ...);
rt_thread_t rt_thread_find(const char *name);
rt_thread_t rt_thread_create(const char *name, void (*entry)(void *), void *arg,
                            unsigned stack, unsigned priority, unsigned slice);
int rt_thread_startup(rt_thread_t thread);
void rt_thread_delete(rt_thread_t thread);
typedef struct video_buffer_stats_st { uint32_t started,valid,overflows,invalid,gaps,timeouts,max_frame_len; } VIDEO_BUFFER_STATS_ST;
void video_buffer_get_stats(VIDEO_BUFFER_STATS_ST *stats);
typedef struct video_transfer_stats_st { uint32_t frames,node_drops,queue_drops; } VIDEO_TRANSFER_STATS_ST;
void video_transfer_get_stats(VIDEO_TRANSFER_STATS_ST *stats);
int video_buffer_open(void);
uint32_t video_buffer_read_frame_timeout(uint8_t *data, uint32_t capacity, uint32_t timeout);
#endif
