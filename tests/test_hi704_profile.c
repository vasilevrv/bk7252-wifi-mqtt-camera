#define CAM_HI704_HOST
#include "../bdk_rtt/beken378/func/camera_intf/sensors/hi704.c"
#include "../bdk_rtt/beken378/func/camera_intf/sensors/hi704_regs.c"
#include <assert.h>
static unsigned char page,regs[256][256];static int fail_once;
void camera_intf_sccb_write(uint8_t reg,uint8_t value){if(reg==3)page=value;else regs[page][reg]=value;}
uint8_t camera_intf_sccb_write2(uint8_t dev,uint8_t reg,uint8_t *value,uint8_t len){assert(dev==HI704_DEV_ID && len==1);if(reg==0x89 && fail_once){fail_once=0;return 1;}camera_intf_sccb_write(reg,*value);return 0;}
uint8_t camera_intf_sccb_read2(uint8_t dev,uint8_t reg,uint8_t *value,uint8_t len){assert(dev==HI704_DEV_ID && len==1);*value=regs[page][reg];return 0;}
static uint32_t limit(void){return ((uint32_t)regs[32][0x88]<<16)|((uint32_t)regs[32][0x89]<<8)|regs[32][0x8a];}
int main(void){
    struct hi704_i2c bus={0};camera_sensor_t sensor={&bus};char *fast[]={"hi704_fps","fast"},*stock[]={"hi704_fps","stock"};
    hi704_sensor_init(0,0,&sensor);assert(limit()==90000 && page==0 && regs[32][0x10]==0x9c && hi704_profile_verified);
    assert(!hi704_fps(2,stock));assert(limit()==180000 && page==0);
    fail_once=1;assert(hi704_fps(2,fast)==-1);assert(limit()==180000 && hi704_profile_verified);
    assert(!hi704_fps(2,fast));assert(limit()==90000);
    puts("PASS actual HI704 profile: stock init then verified90000 fast limit; exact180000 stock rollback; failed register write restores prior profile; AE reenabled and page0 restored");return 0;
}
