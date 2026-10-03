"""Measure delivered RTP/JPEG FPS, scan size and quantizers; never save image data."""
import argparse
import json
import socket
import time
import select
from collections import deque
from pathlib import Path
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('host')
parser.add_argument('--port',type=int,default=8554)
parser.add_argument('--transport',choices=['tcp','udp'],default='tcp')
parser.add_argument('--seconds',type=float,default=10)
parser.add_argument('--warmup',type=float,default=1)
parser.add_argument('--output',type=Path)
args=parser.parse_args()
assert args.seconds>0 and args.warmup>=0
s=socket.create_connection((args.host,args.port),3);s.settimeout(3);buf=b''
u=ur=None
if args.transport=='udp':
    for _ in range(50):
        u=socket.socket(socket.AF_INET,socket.SOCK_DGRAM);u.bind(('',0));port=u.getsockname()[1]
        if port&1 or port>=65535:u.close();u=None;continue
        ur=socket.socket(socket.AF_INET,socket.SOCK_DGRAM)
        try:ur.bind(('',port+1));break
        except OSError:u.close();ur.close();u=ur=None
    if u is None:raise RuntimeError('no UDP port pair')
    u.settimeout(3);peer_ip=s.getpeername()[0]

pending=deque()
def read_item():
    global buf
    while True:
        if buf.startswith(b'$'):
            if len(buf)>=4:
                n=int.from_bytes(buf[2:4],'big')
                if len(buf)>=n+4:
                    channel=buf[1];p=buf[4:n+4];buf=buf[n+4:];return channel,p
        elif b'\r\n\r\n' in buf:
            header,body=buf.split(b'\r\n\r\n',1)
            fields={}
            for line in header.decode().split('\r\n')[1:]:
                key,value=line.split(':',1);fields[key.lower()]=value.strip()
            n=int(fields.get('content-length',0))
            if len(body)>=n:
                buf=body[n:];return None,(header,fields)
        more=s.recv(65536)
        if not more:raise EOFError('camera disconnected')
        buf+=more

def item():
    return pending.popleft() if pending else read_item()

def request(method,seq,headers='',path='/stream'):
    s.sendall(f'{method} rtsp://{args.host}:{args.port}{path} RTSP/1.0\r\nCSeq: {seq}\r\n{headers}\r\n'.encode())
    while True:
        channel,data=read_item()
        if channel is None:
            if not data[0].startswith(b'RTSP/1.0 200'):
                raise RuntimeError(data[0].decode())
            return data[1]
        pending.append((channel,data))
try:
    transport='RTP/AVP/TCP;unicast;interleaved=0-1' if u is None else f'RTP/AVP;unicast;client_port={port}-{port+1}'
    headers=request('SETUP',1,'Transport: '+transport+'\r\n','/stream/track0')
    if u is not None:
        import re
        m=re.search(r'server_port=(\d+)-(\d+)',headers['transport']);assert m
        server_rtp=int(m[1]);server_rtcp=int(m[2])
        ur.connect((peer_ip,server_rtcp))

    session=headers['session'].split(';')[0]
    sh=f'Session: {session}\r\n'
    request('PLAY',2,sh)
    warmup_end=time.monotonic()+args.warmup
    first=last=None;sizes=[];wire=0;scan=0;frame_wire=0;quant_means=[];last_seq=None
    arrivals=[];capture_intervals=[];transfer_ms=[];last_stamp=None;frame_start=None
    lost_packets=discarded_frames=0;valid_frame=False;frame_open=False;frame_stamp=None
    keepalive_at=time.monotonic();hard_end=time.monotonic()+args.warmup+args.seconds+8

    while True:
        if time.monotonic()>hard_end:raise RuntimeError('not enough complete JPEG frames within measurement deadline')
        if time.monotonic()-keepalive_at>=20:
            request('GET_PARAMETER',100,sh);keepalive_at=time.monotonic()
        if u is None:channel,p=item()
        else:
            p,addr=u.recvfrom(65536)
            if addr!=(peer_ip,server_rtp):continue
            channel=0
        if channel!=0:continue
        if len(p)<20 or p[1]&127!=26:raise RuntimeError('unexpected RTP payload')
        seq=int.from_bytes(p[2:4],'big')
        if last_seq is not None and seq!=(last_seq+1)&65535:
            if u is None:raise RuntimeError('RTP sequence discontinuity')
            delta=(seq-last_seq)&65535
            if delta==0 or delta>32768:continue # Duplicate/older datagram.
            lost_packets+=delta-1;valid_frame=False
        last_seq=seq
        offset=int.from_bytes(p[13:16],'big');kind=p[16];quality=p[17]
        width,height=p[18]*8,p[19]*8
        start=20+(4 if kind in (64,65) else 0)
        stamp=int.from_bytes(p[4:8],'big')
        if offset==0:
            if frame_open:discarded_frames+=1
            valid_frame=True;frame_open=True;frame_stamp=stamp
            scan=0;frame_wire=0;means=None;frame_start=time.monotonic()
            if quality>=128:
                precision=p[start+1];qn=int.from_bytes(p[start+2:start+4],'big')
                if precision==0 and qn==128:
                    table=p[start+4:start+4+qn];means=[sum(table[:64])/64,sum(table[64:])/64]
                start+=4+qn
        if offset!=scan or stamp!=frame_stamp:
            if u is None:raise RuntimeError('RTP JPEG fragment discontinuity')
            valid_frame=False
        if valid_frame:scan+=len(p)-start;frame_wire+=len(p)+(4 if u is None else 0)
        if p[1]&128:
            frame_open=False
            if not valid_frame:discarded_frames+=1;continue
            now=time.monotonic()
            if now<warmup_end:continue
            if first is None:first=now
            stamp=int.from_bytes(p[4:8],'big')
            if last is not None:arrivals.append((now-last)*1000)
            if last_stamp is not None:capture_intervals.append(((stamp-last_stamp)&0xffffffff)/90)
            last_stamp=stamp
            transfer_ms.append((now-frame_start)*1000)
            last=now;sizes.append(scan);wire+=frame_wire
            if means is not None:quant_means.append(means)
            if now-first>=args.seconds:break
    elapsed=last-first
    result=dict(host=args.host,port=args.port,transport=args.transport,lost_packets=lost_packets,discarded_frames=discarded_frames,width=width,height=height,
                frames=len(sizes),seconds=round(elapsed,3),fps=round((len(sizes)-1)/elapsed,2),
                rtp_mbps=round(wire*8/elapsed/1e6,2),scan_bytes_min=min(sizes),
                scan_bytes_max=max(sizes),scan_bytes_mean=round(sum(sizes)/len(sizes)))
    def distribution(values):
        values=sorted(values)
        def percentile(q):return round(values[round((len(values)-1)*q)],2)
        return dict(min_ms=round(values[0],2),p50_ms=percentile(.5),p95_ms=percentile(.95),p99_ms=percentile(.99),max_ms=round(values[-1],2),over_250ms=sum(x>250 for x in values),over_500ms=sum(x>500 for x in values))
    result['arrival_intervals']=distribution(arrivals)
    result['capture_intervals']=distribution(capture_intervals)
    result['frame_transfer']=distribution(transfer_ms)
    if quant_means:
        result['quant_luma_mean']=round(sum(v[0] for v in quant_means)/len(quant_means),2)
        result['quant_chroma_mean']=round(sum(v[1] for v in quant_means)/len(quant_means),2)
    request('TEARDOWN',3,sh)
    text=json.dumps(result,indent=2)+'\n';print(text,end='')
    if args.output:args.output.write_text(text)
finally:
    s.close()
    if u is not None:u.close()
    if ur is not None:ur.close()
