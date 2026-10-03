#ifndef RTTHREAD_HOST_H
#define RTTHREAD_HOST_H
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#define RT_NAME_MAX 8
#define RT_TICK_PER_SECOND 1000
#define RT_NULL NULL
#define RT_EOK 0
#define RT_ERROR 1
#define RT_ENOMEM 12
#define RT_WAITING_FOREVER -1
#define RT_IPC_FLAG_FIFO 0
#define RT_IPC_CMD_RESET 0
#define rt_malloc malloc
#define rt_calloc calloc
#define rt_free free
#define rt_strdup strdup
#define rt_strlen strlen
#define rt_strncmp strncmp
#define rt_memset memset
#define rt_snprintf snprintf
#define MSH_CMD_EXPORT(fn, desc)
typedef uint32_t rt_tick_t;
typedef uint32_t rt_uint32_t;
typedef uint8_t rt_uint8_t;
struct host_thread;
struct host_mutex;
typedef struct host_thread *rt_thread_t;
typedef struct host_mutex *rt_mutex_t;
typedef void *rt_mq_t;
uint32_t rt_tick_get(void);
uint32_t rt_tick_from_millisecond(int ms);
void rt_thread_delay(uint32_t ticks);
rt_thread_t rt_thread_create(const char *,void (*)(void *),void *,unsigned,unsigned,unsigned);
int rt_thread_startup(rt_thread_t);
void rt_thread_delete(rt_thread_t);
rt_mutex_t rt_mutex_create(const char *,int);
int rt_mutex_delete(rt_mutex_t);
int rt_mutex_take(rt_mutex_t,int);
int rt_mutex_release(rt_mutex_t);
rt_mq_t rt_mq_create(const char *,unsigned,unsigned,int);
int rt_mq_delete(rt_mq_t);
int rt_mq_control(rt_mq_t,int,void *);
int rt_mq_send(rt_mq_t,void *,unsigned);
int rt_mq_recv(rt_mq_t,void *,unsigned,int);
void rt_kprintf(const char *,...);
void rt_enter_critical(void);
void rt_exit_critical(void);
#endif
