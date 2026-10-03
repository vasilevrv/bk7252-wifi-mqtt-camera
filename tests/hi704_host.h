#ifndef HI704_HOST_H
#define HI704_HOST_H
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef uint32_t UINT32;typedef uint8_t UINT8;typedef int DD_HANDLE;
struct hi704_i2c {int salve_id;};typedef struct {struct hi704_i2c *i2c_cfg;} camera_sensor_t;
#define HI704_DEV_ID 0x30
#define HI704_DEV_CHIPID 0x96
#define os_printf printf
#define rt_kprintf printf
#define MSH_CMD_EXPORT(a,b)
uint8_t camera_intf_sccb_write2(uint8_t,uint8_t,uint8_t *,uint8_t);
uint8_t camera_intf_sccb_read2(uint8_t,uint8_t,uint8_t *,uint8_t);
void camera_intf_sccb_write(uint8_t,uint8_t);
extern const unsigned char hi704_sensor_init_sequence[704][2];
#endif
