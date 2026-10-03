#include "cam_rtp_jpeg.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(int argc, char **argv) {
    FILE *f; unsigned char *p,*q; long n,i; struct cam_jpeg j;
    if(argc!=2)return 2;
    f=fopen(argv[1],"rb");if(!f)return 3;
    fseek(f,0,SEEK_END);n=ftell(f);rewind(f);p=malloc(n);q=malloc(n);
    if(!p||!q)return 4;
    if(fread(p,1,n,f)!=(size_t)n)return 5;fclose(f);
    for(i=0;i<n;++i)cam_jpeg_parse(p,i,&j);
    srand(9);
    for(i=0;i<20000;++i) {
        size_t at=rand()%(size_t)n,len=rand()%(size_t)n;
        memcpy(q,p,n);q[at]=rand()&255;
        cam_jpeg_parse(q,len,&j);
    }
    free(p);free(q);puts("PASS ASan/UBSan: all truncations and 20000 mutated/truncated JPEG inputs.");return 0;
}
