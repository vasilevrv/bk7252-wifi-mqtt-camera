"""Exercise the actual C server via POSIX adapters and the installed VLC."""
import socket,subprocess,time,sys,struct,signal
from pathlib import Path
root=Path(__file__).resolve().parents[2]
work=root/'.build-tools/rtsp-tests'; logs=work/'host.log'
fixture=Path(sys.argv[1]).resolve() if len(sys.argv)>1 else work/'gradient.jpg'
src=fixture.read_bytes();pos=2;restart=0
while pos<len(src):
    assert src[pos]==255
    while src[pos]==255:pos+=1
    marker=src[pos];pos+=1;length=int.from_bytes(src[pos:pos+2],'big');data=src[pos+2:pos+length];pos+=length
    if marker==0xc0:
        height=int.from_bytes(data[1:3],'big');width=int.from_bytes(data[3:5],'big');typ=1 if data[7]==0x22 else 0
    if marker==0xdd:restart=int.from_bytes(data,'big')
    if marker==0xda:prefix_end=pos;break
if restart:typ+=64
expected=src[:src.rfind(b'\xff\xd9')+2]
host=subprocess.Popen([str(work/'rtsp_host'),str(fixture)],stdout=logs.open('w'),stderr=subprocess.STDOUT)
class Client:
    def __init__(self):self.s=socket.create_connection(('127.0.0.1',8554),3);self.s.settimeout(3);self.buf=b''
    def more(self):
        p=self.s.recv(4096)
        if not p:raise EOFError
        self.buf+=p
    def item(self):
        while len(self.buf)<4:self.more()
        if self.buf[0]==36:
            n=int.from_bytes(self.buf[2:4],'big')+4
            while len(self.buf)<n:self.more()
            p,self.buf=self.buf[:n],self.buf[n:];return ('packet',p[1],p[4:])
        while b'\r\n\r\n' not in self.buf:self.more()
        header,rest=self.buf.split(b'\r\n\r\n',1);lines=header.decode().split('\r\n');fields={}
        for line in lines[1:]:
            k,v=line.split(':',1);fields[k.lower()]=v.strip()
        n=len(header)+4+int(fields.get('content-length',0))
        while len(self.buf)<n:self.more()
        body=self.buf[len(header)+4:n];self.buf=self.buf[n:]
        return ('response',int(lines[0].split()[1]),fields,body)
    def response(self,code,cseq):
        while True:
            item=self.item()
            if item[0]=='response':
                assert item[1]==code,item
                assert item[2]['cseq']==str(cseq);return item
    def request(self,method,seq,headers='',path='/stream',body=b'',fragment=False):
        p=(f'{method} rtsp://127.0.0.1:8554{path} RTSP/1.0\r\nCSeq: {seq}\r\n{headers}Content-Length: {len(body)}\r\n\r\n').encode()+body
        if fragment:
            for i in range(0,len(p),3):self.s.sendall(p[i:i+3]);time.sleep(.001)
        else:self.s.sendall(p)
        return p
try:
    for _ in range(30):
        try:c=Client();break
        except OSError:time.sleep(.1)
    else:raise RuntimeError('host did not listen')
    c.request('OPTIONS',1,fragment=True);c.response(200,1)
    c.request('DESCRIBE',2);r=c.response(200,2)
    assert b'a=rtpmap:26 JPEG/90000' in r[3] and r[2]['content-base'].endswith('/stream/')
    for transport in ['RTP/AVP;unicast;client_port=9000-9001',
                      'RTP/AVP/UDP;unicast;client_port=9000-9001',
                      'RTP/AVP;multicast;client_port=9000-9001',
                      'RTP/AVP;unicast;client_port=9001-9002']:
        c.request('SETUP',3,'Transport: '+transport+'\r\n',path='/stream/track0');c.response(461,3)
    c.request('SETUP',4,'Transport: RTP/AVP/TCP;unicast;interleaved=4-5\r\n',path='/stream/track0')
    r=c.response(200,4);session=r[2]['session'].split(';')[0]
    c.request('PLAY',5,'Session: incorrect\r\n');c.response(454,5)
    sh=f'Session: {session}\r\n'
    c.request('PLAY',6,sh);c.response(200,6)
    scan=b'';quant=None;lastseq=None;timestamp=None
    while True:
        kind,channel,p=c.item();assert kind=='packet'
        if channel==5:
            assert p[1]==200 and p[29]==202;continue
        assert channel==4
        seq=int.from_bytes(p[2:4],'big');ts=int.from_bytes(p[4:8],'big')
        assert p[0]==128 and p[1]&127==26 and p[16:20]==bytes([typ,255,width//8,height//8])
        offset=int.from_bytes(p[13:16],'big');assert offset==len(scan)
        if lastseq is not None:assert seq==(lastseq+1)&65535 and ts==timestamp
        lastseq=seq;timestamp=ts;start=20 # RTP(12)+JPEG(8)
        if restart:
            assert p[20:24]==struct.pack('!HH',restart,65535);start+=4
        if not offset:
            assert p[start:start+4]==b'\0\0\0\x80';quant=p[start+4:start+132];start+=132
        scan+=p[start:]
        if p[1]&128:break
    # Reuse parsed header boundaries, including hardware fill prefixes.
    reconstructed=src[:prefix_end]+scan+b'\xff\xd9'
    assert reconstructed==expected # Independently received RTP recovered the exact JPEG.
    (work/'rtp-reassembled.jpg').write_bytes(reconstructed)
    subprocess.run(['sips','-s','format','png',str(work/'rtp-reassembled.jpg'),'--out',str(work/'rtp-reassembled.png')],check=True,stdout=subprocess.DEVNULL)
    c.request('PAUSE',7,sh);c.response(200,7)
    # Coalesced binary RTCP + two requests, first carrying a body containing NUL.
    body=b'probe\x00value'
    req=(f'GET_PARAMETER rtsp://127.0.0.1:8554/stream RTSP/1.0\r\nCSeq: 8\r\n{sh}Content-Length: {len(body)}\r\n\r\n').encode()+body
    req+=b'OPTIONS * RTSP/1.0\r\nCSeq: 9\r\n\r\n'
    rr=b'\x80\xc9\x00\x01\x00\x00\x00\x01'
    c.s.sendall(b'$\x05'+struct.pack('!H',len(rr))+rr+req)
    c.response(200,8);c.response(200,9)
    c.request('PLAY',10,sh);c.response(200,10)
    c.request('TEARDOWN',11,sh);c.response(200,11);c.s.close()
    # Reconnection with malformed lengths and duplicate headers cannot wedge listener.
    for headers in ['CSeq: 12\r\n','Content-Length: 9999999\r\n']:
        c=Client();p=('OPTIONS * RTSP/1.0\r\nCSeq: 12\r\n'+headers+'\r\n').encode();c.s.sendall(p)
        try:c.item()
        except (EOFError,ConnectionResetError):pass
        c.s.close()
    c=Client();c.request('OPTIONS',13);c.response(200,13);c.s.close()
    print('PASS RTSP: split/coalesced requests, SDP, invalid transport rejection, channels 4/5, Session validation, PLAY/PAUSE/TEARDOWN, binary RTCP/body, reconnect, exact JPEG reconstruction and native decoder.')
    vlc=Path('/Applications/VLC.app/Contents/MacOS/VLC')
    if vlc.exists():
        for explicit_tcp in [True,False]:
            cmd=[str(vlc),'-I','dummy','--no-audio','--vout','dummy','--rtsp-tcp','--run-time=7','--play-and-exit','--video-filter=scene','--scene-ratio=20','--scene-path='+str(work),'--scene-prefix=vlc-rtsp','--scene-format=png','-vv','rtsp://127.0.0.1:8554/stream']
            if not explicit_tcp:cmd.remove('--rtsp-tcp')
            for image in work.glob('vlc-rtsp*.png'):image.unlink()
            with (work/'vlc.log').open('w') as out:
                player=subprocess.Popen(cmd,stdout=out,stderr=subprocess.STDOUT)
                try:player.wait(timeout=13)
                except subprocess.TimeoutExpired:
                    player.terminate()
                    try:player.wait(timeout=3)
                    except subprocess.TimeoutExpired:player.kill();player.wait()
            images=list(work.glob('vlc-rtsp*.png'))
            assert len(images)>5,'VLC did not sustain decoding; inspect vlc.log'
            text=(work/'vlc.log').read_text(errors='replace')
            assert 'Timestamp conversion failed' not in text and 'Could not convert timestamp' not in text
            assert 'hasBeenSynchronizedUsingRTCP()' in text
            assert 'RTP/AVP/TCP' in text, 'VLC did not negotiate TCP'
            if not explicit_tcp:assert '461 Unsupported Transport' in text, 'default VLC did not exercise UDP rejection'
            print('PASS VLC '+('explicit TCP' if explicit_tcp else 'default UDP-to-TCP fallback')+': decoded',len(images),'PNG frame(s); RTCP compound SR/SDES sent during 7s playback.')
finally:
    host.send_signal(signal.SIGTERM)
    try:host.wait(timeout=7)
    except subprocess.TimeoutExpired:host.kill();host.wait()
