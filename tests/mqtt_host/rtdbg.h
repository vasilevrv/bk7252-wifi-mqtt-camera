#include <stdio.h>
#define LOG_E(...) do {fprintf(stderr,__VA_ARGS__);fprintf(stderr,"\n");} while(0)
#define LOG_W(...) LOG_E(__VA_ARGS__)
#define LOG_I(...) LOG_E(__VA_ARGS__)
#define LOG_D(...) do {} while(0)
