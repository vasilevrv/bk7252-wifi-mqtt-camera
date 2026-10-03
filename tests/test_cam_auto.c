/* Exercise the firmware's actual credential codec, retry policy and EasyFlash implementation. */
#include "cam_wifi_config.h"
#include "cam_mqtt_config.h"
#include <easyflash.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
static unsigned char flash_mem[4096];
static unsigned erases, writes;
static int fail_erase, fail_write;
static const ef_env defaults[] = {{"user", "user"}};
EfErrCode ef_port_init(const ef_env **out, size_t *n) { *out = defaults; *n = 1; return EF_NO_ERR; }
EfErrCode ef_port_read(uint32_t addr, uint32_t *buf, size_t size) {
    assert(addr >= EF_START_ADDR && (size_t)(addr - EF_START_ADDR) + size <= sizeof(flash_mem));
    memcpy(buf, flash_mem + (addr - EF_START_ADDR), size); return EF_NO_ERR;
}
EfErrCode ef_port_erase(uint32_t addr, size_t size) {
    (void)size; assert(addr == EF_START_ADDR);
    ++erases; if (fail_erase) { fail_erase = 0; return EF_ERASE_ERR; }
    memset(flash_mem, 255, sizeof(flash_mem)); return EF_NO_ERR;
}
EfErrCode ef_port_write(uint32_t addr, const uint32_t *buf, size_t size) {
    ++writes; if (fail_write) { fail_write = 0; return EF_WRITE_ERR; }
    assert(addr == EF_START_ADDR && size <= sizeof(flash_mem));
    memcpy(flash_mem, buf, size); return EF_NO_ERR;
}
void ef_port_env_lock(void) {}
void ef_port_env_unlock(void) {}
void ef_log_debug(const char *file, long line, const char *fmt, ...) { (void)file; (void)line; (void)fmt; }
void ef_log_info(const char *fmt, ...) { (void)fmt; }
void ef_print(const char *fmt, ...) { (void)fmt; }
static void parse_ok(const char *text, const char *ssid, const char *password) {
    struct cam_wifi_config c, decoded; char encoded[CAM_WIFI_RECORD_MAX];
    assert(!cam_wifi_parse(text, strlen(text), &c));
    assert(!strcmp(c.ssid, ssid) && !strcmp(c.password, password));
    assert(!cam_wifi_encode(&c, encoded));
    assert(!cam_wifi_decode(encoded, &decoded));
    assert(!memcmp(&c, &decoded, sizeof(c)));
}
static void test_codec(void) {
    struct cam_wifi_config c = {{"old"}, {"password"}}, old = c;
    const char *bad[] = {"", "ssid=a", "ssid=\npassword=password", "ssid=a\npassword=short", "ssid=a\nssid=b\npassword=password", "SSID=a\npassword=password", "ssid=a\npassword=pass\rword", "ssid=a\npassword=password\nunknown=yes"};
    const char *records[] = {"", "02:6162:70617373776f7264", "01:00:", "01:61:0", "01:zz:", "01:61::"};
    size_t i; char text[600], record[CAM_WIFI_RECORD_MAX];
    parse_ok("ssid=home\npassword=password\n", "home", "password");
    parse_ok("\xef\xbb\xbf# setup\r\nssid= space #= wifi \r\npassword= #= secret \r\n", " space #= wifi ", " #= secret ");
    parse_ok("ssid=open\npassword=", "open", "");
    for (i = 0; i < sizeof(bad)/sizeof(*bad); ++i) {
        assert(cam_wifi_parse(bad[i], strlen(bad[i]), &c)); assert(!memcmp(&c, &old, sizeof(c)));
    }
    for (i = 0; i < sizeof(records)/sizeof(*records); ++i) assert(cam_wifi_decode(records[i], &c));
    assert(cam_wifi_parse("ssid=a\0\npassword=password", 26, &c));
    memset(c.ssid, 's', 32); c.ssid[32] = 0; memset(c.password, 'p', 63); c.password[63] = 0;
    assert(!cam_wifi_encode(&c, record)); assert(strlen(record) == 194);
    assert(!cam_wifi_decode(record, &old)); assert(!memcmp(&c, &old, sizeof(c)));
    snprintf(text, sizeof(text), "ssid=%ss\npassword=password", c.ssid); assert(cam_wifi_parse(text, strlen(text), &old));
    snprintf(text, sizeof(text), "ssid=a\npassword=%sp", c.password); assert(cam_wifi_parse(text, strlen(text), &old));
    memset(text, 'a', sizeof(text)); assert(cam_wifi_parse(text, 513, &old));
    /* Untrusted SD and stored-record lengths under ASan/UBSan. */
    srand(7);
    for (i = 0; i < 20000; ++i) {
        size_t j, n = rand() % 513;
        for (j = 0; j < n; ++j) text[j] = rand() % 256;
        cam_wifi_parse(text, n, &old);
        n = rand() % (CAM_WIFI_RECORD_MAX - 1);
        for (j = 0; j < n; ++j) record[j] = "0123456789abcdef:x"[rand()%18];
        record[n] = 0; cam_wifi_decode(record, &old);
    }
    puts("PASS: SD parsing, limits, whitespace, empty password, record validation, 20000 mutations");
}
static void test_mqtt_codec(void) {
    struct cam_mqtt_config c, d; char record[CAM_MQTT_RECORD_MAX]; size_t i;
    const char *valid[] = {"host=192.168.40.1", "host=mqtt.local\nport=1884\nusername=u\npassword= p#= \nclient_id=cam_1\ntopic_base=cameras/cam1", "enabled=0", "\xef\xbb\xbfhost=localhost\r\nport=65535\r\n"};
    const char *bad[] = {"", "host=", "host=tcp://localhost", "host=localhost\nport=0", "host=localhost\nport=65536", "host=localhost\nport=-1", "host=localhost\nhost=x", "host=localhost\npassword=p", "host=localhost\nclient_id=bad id", "host=localhost\ntopic_base=x/#", "enabled=2", "unknown=1"};
    for(i=0;i<sizeof(valid)/sizeof(*valid);++i) {
        assert(!cam_mqtt_parse(valid[i],strlen(valid[i]),&c));
        assert(!cam_mqtt_encode(&c,record)); assert(!cam_mqtt_decode(record,&d));
        assert(!memcmp(&c,&d,sizeof(c)));
    }
    for(i=0;i<sizeof(bad)/sizeof(*bad);++i)assert(cam_mqtt_parse(bad[i],strlen(bad[i]),&c));
    memset(&c,0,sizeof(c)); c.enabled=1;c.port=65535;
    memset(c.host,'h',63);memset(c.username,'u',31);memset(c.password,'p',63);
    memset(c.client_id,'i',31);memset(c.topic_base,'t',63);
    assert(!cam_mqtt_encode(&c,record));assert(!cam_mqtt_decode(record,&d));assert(!memcmp(&c,&d,sizeof(c)));
    for(i=0;i<20000;++i) {
        char text[513];size_t j,n=rand()%513;
        for(j=0;j<n;++j)text[j]=rand()%256;
        cam_mqtt_parse(text,n,&d);
        n=rand()%(CAM_MQTT_RECORD_MAX-1);
        for(j=0;j<n;++j)record[j]="0123456789abcdef:x"[rand()%18];
        record[n]=0;cam_mqtt_decode(record,&d);
    }
    puts("PASS: MQTT config defaults, auth fields, disable, hostname/port/topic validation, maximum fields, 20000 mutations");
}
static void test_policy(void) {
    struct cam_auto_policy p = {0};
    assert(!cam_auto_step(&p, 0, 20000, 0, 0, 0, 0));
    assert(cam_auto_step(&p, 1, 20000, 1, 1, 0, 0) == CAM_AUTO_JOIN);
    assert(!cam_auto_step(&p, 20000, 20000, 1, 0, 0, 0));
    assert(cam_auto_step(&p, 20001, 20000, 1, 0, 0, 0) == CAM_AUTO_JOIN);
    assert(cam_auto_step(&p, 20002, 20000, 1, 0, 1, 0) == CAM_AUTO_RTSP);
    assert(!cam_auto_step(&p, 20003, 20000, 1, 0, 1, 0));
    assert(cam_auto_step(&p, 40002, 20000, 1, 0, 1, 0) == CAM_AUTO_RTSP);
    assert(!cam_auto_step(&p, 40003, 20000, 1, 0, 1, 1));
    assert(!cam_auto_step(&p, 41000, 20000, 1, 0, 0, 1));
    assert(!cam_auto_step(&p, 60999, 20000, 1, 0, 0, 1));
    assert(cam_auto_step(&p, 61000, 20000, 1, 0, 0, 1) == CAM_AUTO_JOIN);
    assert(!cam_auto_step(&p, 61001, 20000, 1, 0, 1, 1));
    assert(cam_auto_step(&p, 61002, 20000, 1, 1, 1, 1) == CAM_AUTO_JOIN);
    memset(&p, 0, sizeof(p));
    assert(cam_auto_step(&p, UINT32_MAX-10000, 20000, 1, 0, 0, 0) == CAM_AUTO_JOIN);
    assert(!cam_auto_step(&p, 9998, 20000, 1, 0, 0, 0));
    assert(cam_auto_step(&p, 9999, 20000, 1, 0, 0, 0) == CAM_AUTO_JOIN);
    puts("PASS: initial join, exact 20s retries, DHCP gate, RTSP retry, Wi-Fi loss, SD override, tick wrap");
}
static void test_flash(void) {
    struct cam_wifi_config c; char record[CAM_WIFI_RECORD_MAX]; unsigned before;
    memset(flash_mem, 255, sizeof(flash_mem)); assert(easyflash_init() == EF_NO_ERR);
    assert(!cam_wifi_parse("ssid=first\npassword=password1", 29, &c));
    assert(!cam_wifi_encode(&c, record));
    assert(ef_set_env("cam_wifi_v1", record) == EF_NO_ERR); assert(ef_save_env() == EF_NO_ERR);
    assert(ef_set_env("other", "keep me") == EF_NO_ERR); assert(ef_save_env() == EF_NO_ERR);
    before = erases;
    assert(ef_load_env() == EF_NO_ERR); assert(!strcmp(ef_get_env("cam_wifi_v1"), record));
    assert(!strcmp(ef_get_env("user"), "user"));
    assert(ef_set_env("cam_wifi_v1", record) == EF_NO_ERR); assert(ef_save_env() == EF_NO_ERR);
    assert(erases == before); /* Reinserted SD does not wear flash. */
    strcpy(c.ssid, "second"); assert(!cam_wifi_encode(&c, record));
    assert(ef_set_env("cam_wifi_v1", record) == EF_NO_ERR);
    fail_write = 1; assert(ef_save_env() == EF_WRITE_ERR); before = writes;
    assert(ef_save_env() == EF_NO_ERR); assert(writes == before + 1);
    assert(ef_load_env() == EF_NO_ERR); assert(!strcmp(ef_get_env("cam_wifi_v1"), record));
    assert(!strcmp(ef_get_env("other"), "keep me"));
    strcpy(c.ssid, "third"); assert(!cam_wifi_encode(&c, record));
    assert(ef_set_env("cam_wifi_v1", record) == EF_NO_ERR);
    fail_erase = 1; assert(ef_save_env() == EF_ERASE_ERR); assert(ef_save_env() == EF_NO_ERR);
    assert(ef_load_env() == EF_NO_ERR); assert(!strcmp(ef_get_env("cam_wifi_v1"), record));
    {
        struct cam_mqtt_config m, decoded; char mqtt_record[CAM_MQTT_RECORD_MAX];
        memset(&m,0,sizeof(m));m.enabled=1;m.port=1883;
        memset(m.host,'h',63);memset(m.username,'u',31);memset(m.password,'p',63);
        memset(m.client_id,'i',31);memset(m.topic_base,'t',63);
        assert(!cam_mqtt_encode(&m,mqtt_record));
        assert(ef_set_env("cam_mqtt_v1",mqtt_record)==EF_NO_ERR);assert(ef_save_env()==EF_NO_ERR);
        assert(ef_load_env()==EF_NO_ERR);assert(!cam_mqtt_decode(ef_get_env("cam_mqtt_v1"),&decoded));
        assert(!memcmp(&m,&decoded,sizeof(m)));assert(!strcmp(ef_get_env("cam_wifi_v1"),record));
        assert(!strcmp(ef_get_env("other"),"keep me"));
    }
    /* Corrupt metadata must be rejected before an underflowing data read. */
    { uint32_t end = EF_START_ADDR; memcpy(flash_mem, &end, 4); }
    assert(ef_load_env() == EF_NO_ERR); assert(ef_get_env("cam_wifi_v1") == NULL);
    puts("PASS: actual EasyFlash load/save, unchanged-write avoidance, failed erase/write retries, corrupted metadata");
}
int main(void) { test_codec(); test_mqtt_codec(); test_policy(); test_flash(); return 0; }
