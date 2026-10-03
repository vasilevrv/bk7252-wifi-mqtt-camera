#include "rtthread.h"
#include "mqtt_client.h"
#include <easyflash.h>
#define O_RDONLY 0
#define NIOCTL_GADDR 1
typedef void *rt_device_t;
struct dfs_fd { FILE *file; };
rt_device_t rt_device_find(const char *);
int rt_device_control(rt_device_t,int,void *);
int dfs_file_open(struct dfs_fd *,const char *,int);
int dfs_file_read(struct dfs_fd *,void *,unsigned);
int dfs_file_close(struct dfs_fd *);
