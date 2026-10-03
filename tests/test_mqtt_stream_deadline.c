#define main mqtt_adapter_main
#include "mqtt_host/port.c"
#undef main
#include <sys/socket.h>
#include <signal.h>
static const unsigned char *piece(void *arg,size_t offset,size_t *length){static unsigned char data[1024];(void)arg;(void)offset;*length=sizeof(data);return data;}
int main(void){
    int sockets[2],size=1024,rc;uint32_t before,elapsed;unsigned char buffer[128];mqtt_client c;
    signal(SIGPIPE,SIG_IGN);assert(!socketpair(AF_UNIX,SOCK_STREAM,0,sockets));setsockopt(sockets[0],SOL_SOCKET,SO_SNDBUF,&size,sizeof(size));
    memset(&c,0,sizeof(c));c.sock=sockets[0];c.isconnected=1;c.buf=buffer;c.buf_size=sizeof(buffer);c.mqtt_lock=rt_mutex_create("test",0);
    before=rt_tick_get();rc=paho_mqtt_publish_stream(&c,"snapshot",8*1024*1024,piece,NULL,200);elapsed=rt_tick_get()-before;
    assert(rc==PAHO_FAILURE && !c.isconnected && elapsed>=200 && elapsed<1000);
    close(sockets[0]);close(sockets[1]);rt_mutex_delete(c.mqtt_lock);
    printf("PASS actual Paho streaming deadline: nonreading peer failed in%ums; incomplete PUBLISH socket shut down, connection invalidated\n",elapsed);return 0;
}
