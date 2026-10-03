#ifndef VIDEO_BUFFER_HOST_H
#define VIDEO_BUFFER_HOST_H
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#define CFG_USE_APP_DEMO_VIDEO_TRANSFER 1
#define CFG_USE_CAMERA_INTF 1
#define APP_DEMO_CFG_USE_VIDEO_BUFFER 1
#define CFG_GENERAL_DMA 0
#define GLOBAL_INT_DECLARATION()
#define GLOBAL_INT_DISABLE() ((void)0)
#define GLOBAL_INT_RESTORE() ((void)0)
#define os_printf printf
#define os_malloc malloc
#define os_free free
#define os_memcpy memcpy
#define kNoErr 0
#define BEKEN_WAIT_FOREVER UINT32_MAX
#define TVIDEO_OPEN_SCCB 1
#define TVIDEO_SND_INTF 3
typedef uint8_t UINT8;typedef uint32_t UINT32;
typedef int *beken_semaphore_t;
typedef struct video_buffer_stats_st {UINT32 started,valid,overflows,invalid,gaps,timeouts,max_frame_len;} VIDEO_BUFFER_STATS_ST;
typedef struct {UINT8 *ptk_ptr;UINT32 frame_id,is_eof,frame_len;} TV_HDR_PARAM_ST,*TV_HDR_PARAM_PTR;
typedef struct {UINT32 open_type,send_type,pkt_header_size;int(*send_func)(UINT8 *,UINT32);void(*start_cb)(void);void(*end_cb)(void);void(*add_pkt_header)(TV_HDR_PARAM_PTR);} TVIDEO_SETUP_DESC_ST;
int rtos_init_semaphore(beken_semaphore_t *,int);
int rtos_deinit_semaphore(beken_semaphore_t *);
int rtos_get_semaphore(beken_semaphore_t *,UINT32);
int rtos_set_semaphore(beken_semaphore_t *);
int video_transfer_init(TVIDEO_SETUP_DESC_ST *);
int video_transfer_deinit(void);
UINT32 video_buffer_read_frame_timeout(UINT8 *,UINT32,UINT32);
#endif
