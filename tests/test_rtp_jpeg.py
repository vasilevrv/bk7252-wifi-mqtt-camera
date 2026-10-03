"""Portable packetizer tests; paths supplied by the reproducible build script."""
import ctypes as C
import random
import struct
import sys
from pathlib import Path
class JPEG(C.Structure):
    _fields_=[('scan',C.POINTER(C.c_uint8)),('length',C.c_size_t),('quant',C.c_uint8*128),('width',C.c_uint16),('height',C.c_uint16),('restart',C.c_uint16),('type',C.c_uint8)]
lib=C.CDLL(sys.argv[1]); src=Path(sys.argv[2]).read_bytes()
lib.cam_jpeg_parse.argtypes=[C.POINTER(C.c_uint8),C.c_size_t,C.POINTER(JPEG)]
lib.cam_rtp_packet.argtypes=[C.POINTER(JPEG),C.c_size_t,C.c_uint8,C.c_uint16,C.c_uint32,C.c_uint32,C.POINTER(C.c_uint8),C.c_size_t,C.POINTER(C.c_size_t)]
lib.cam_rtp_packet.restype=C.c_size_t
def parse(data):
    buf=(C.c_uint8*len(data)).from_buffer_copy(data);j=JPEG()
    return lib.cam_jpeg_parse(buf,len(data),C.byref(j)),buf,j
result,buf,j=parse(src+b'\x00\x11\x22\x33\x44')
assert result==0 and (j.width,j.height,j.type)==(320,240,65)
scan=C.string_at(j.scan,j.length);offset=0;seq=65534;timestamp=0xf0010203;packets=[]
while offset<j.length:
    packet=(C.c_uint8*1200)();consumed=C.c_size_t()
    n=lib.cam_rtp_packet(C.byref(j),offset,4,seq,timestamp,0x12345678,packet,1200,C.byref(consumed))
    p=bytes(packet[:n]);assert n>24 and p[:2]==b'$\x04' and int.from_bytes(p[2:4],'big')==n-4
    assert p[4]==0x80 and p[5]&127==26 and int.from_bytes(p[6:8],'big')==seq
    assert int.from_bytes(p[8:12],'big')==timestamp and int.from_bytes(p[12:16],'big')==0x12345678
    assert int.from_bytes(p[17:20],'big')==offset and p[20:24]==bytes([65,255,40,30])
    start=28
    assert p[24:28]==b'\x00\x14\xff\xff'
    if offset==0:
        assert p[start:start+4]==b'\0\0\0\x80' and p[start+4:start+132]==bytes(j.quant)
        start+=132
    assert bool(p[5]&128)==(offset+consumed.value==j.length)
    assert p[start:]==scan[offset:offset+consumed.value]
    packets.append(p[start:]);offset+=consumed.value;seq=(seq+1)&65535
assert b''.join(packets)==scan
# All truncated inputs must fail; trailing vendor bytes must be ignored.
for n in range(len(src)-1):assert parse(src[:n])[0]<0
# Custom Huffman tables, unsupported sampling and progressive coding rejected.
for marker in [b'\xff\xc4',b'\xff\xc0']:
    pos=src.index(marker)
    bad=bytearray(src)
    if marker==b'\xff\xc4':bad[pos+21]^=1
    else:bad[pos+11]=0x11
    assert parse(bytes(bad))[0]<0
bad=src.replace(b'\xff\xc0',b'\xff\xc2',1);assert parse(bad)[0]<0
# DRI maps to type 65 and all packets carry the full-frame restart flags.
result,restart_buf,restart=parse(src.replace(b'\xff\xdd\x00\x04\x00\x14',b'\xff\xdd\x00\x04\x00\x10',1))
assert result==0 and restart.type==65 and restart.restart==16
packet=(C.c_uint8*1200)();consumed=C.c_size_t()
n=lib.cam_rtp_packet(C.byref(restart),0,0,1,2,3,packet,1200,C.byref(consumed))
assert n and bytes(packet[24:28])==b'\x00\x10\xff\xff'
rng=random.Random(9)
for _ in range(5000):
    data=bytearray(src);pos=rng.randrange(len(data));data[pos]=rng.randrange(256)
    parse(bytes(data))
print('PASS RTP/JPEG: quant tables, offsets, sequence wrap, 90kHz timestamp, final marker, exact scan recovery, vendor trailer, DRI, truncated/malformed frames (5000 mutations).')

for kind,height in [(0,8),(1,16)]:
    result,plain_buf,plain=parse(Path(sys.argv[2]).with_name('type%d.jpg'%kind).read_bytes())
    assert result==0 and plain.type==kind and not plain.restart and (plain.width,plain.height)==(16,height)
    packet=(C.c_uint8*1200)();consumed=C.c_size_t()
    n=lib.cam_rtp_packet(C.byref(plain),0,0,1,2,3,packet,1200,C.byref(consumed))
    assert n and packet[20]==kind and bytes(packet[24:28])==b'\0\0\0\x80' and packet[5]&128
print('PASS RTP/JPEG types 0/1 without restart markers; native decoder accepted both grayscale fixtures.')

# Hardware-style fill prefixes before table headers, SOS and EOI.
filled=src.replace(b'\xff\xdb',b'\xff\xff\xff\xdb',1).replace(b'\xff\xda',b'\xff\xff\xda',1).replace(b'\xff\xd9',b'\xff\xff\xff\xd9',1)
result,fill_buf,fill_jpeg=parse(filled+b'\0\x12\x34\x56\x78')
assert result==0 and C.string_at(fill_jpeg.scan,fill_jpeg.length).endswith(b'\xff\xff')
for n in range(1,5):assert parse(filled[:-n])[0]<0
print('PASS BK7252 marker fill: repeated FF before DQT/SOS/EOI, preserved EOI fill, truncated fill rejected.')
