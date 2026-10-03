"""Independent local MQTT wire check of actual C wrapper/Paho with POSIX RT adapters."""
from pathlib import Path
import json
import os
import socket
import subprocess
import threading
import time
root = Path(__file__).resolve().parents[2]
work = root / '.build-tools/mqtt-tests'
exe = work / 'mqtt_host'

def exact(sock, size):
    data = bytearray()
    while len(data) < size:
        chunk = sock.recv(size-len(data))
        if not chunk:
            raise EOFError
        data.extend(chunk)
    return bytes(data)

def packet(sock):
    first = exact(sock,1)[0]
    remaining = 0
    shift = 0
    while True:
        b = exact(sock,1)[0]
        remaining |= (b & 127) << shift
        if not b & 128:
            break
        shift += 7
        assert shift <= 21
    return first, exact(sock,remaining)

def string(data, offset):
    size = int.from_bytes(data[offset:offset+2], 'big')
    end = offset + size + 2
    assert end <= len(data)
    return data[offset+2:end].decode(), end

def check_connect(data, auth):
    name,o = string(data,0)
    assert name == 'MQTT' and data[o] == 4
    flags = data[o+1]
    assert flags & 2 and flags & 4 and flags & 32
    assert ((flags >> 3) & 3) == 0
    assert int.from_bytes(data[o+2:o+4],'big') == 30
    client,o = string(data,o+4)
    assert client == 'bk7252-0004b2087e6c'
    topic,o = string(data,o)
    message,o = string(data,o)
    assert topic == 'bk7252-0004b2087e6c/availability' and message == 'offline'
    if auth:
        assert flags & 128 and flags & 64
        username,o = string(data,o)
        password,o = string(data,o)
        assert username == 'camera' and password == 'test-password'
    else:
        assert not flags & 192
    assert o == len(data)

def scenario(auth, custom=False):
    server=socket.socket();server.bind(('127.0.0.1',0));server.listen(4);server.settimeout(.1)
    port=server.getsockname()[1]
    base=('x'*50+'"'+chr(92)+'y'*11) if custom else 'bk7252-0004b2087e6c'
    cfg=work / ('custom.conf' if custom else ('auth.conf' if auth else 'anonymous.conf'))
    cfg.write_text(f'host=127.0.0.1\nport={port}\n'+('username=camera\npassword=test-password\n' if auth else '')+(f'topic_base={base}\n' if custom else ''))
    original=(root/'.build-tools/rtsp-tests/gradient.jpg').read_bytes()
    padding=b'Z'*40000
    jpeg=original[:2]+b'\xff\xfe'+(len(padding)+2).to_bytes(2,'big')+padding+original[2:]
    assert len(jpeg)<49152
    fixture=work/'snapshot-fixture.jpg';fixture.write_bytes(jpeg)
    errors=[];connects=[];published=[];images=[];configs=set();stop=threading.Event()
    def broker():
        try:
            while not stop.is_set():
                try:s,_=server.accept()
                except socket.timeout:continue
                with s:
                    s.settimeout(1)
                    kind,data=packet(s);assert kind==0x10
                    if not custom:check_connect(data,auth)
                    else:
                        name,o=string(data,0);assert name=='MQTT';flags=data[o+1];assert flags&32
                        client,o=string(data,o+4);topic,o=string(data,o);will,o=string(data,o)
                        assert topic==base+'/availability' and will=='offline'
                    connects.append(time.monotonic())
                    if auth and len(connects)==1:s.sendall(b'\x20\x02\x00\x05');continue
                    s.sendall(b'\x20\x02\x00\x00')
                    while not stop.is_set():
                        try:kind,data=packet(s)
                        except socket.timeout:continue
                        except EOFError:break
                        if kind==0xc0:s.sendall(b'\xd0\x00');continue
                        if kind==0xe0:break
                        assert kind==0x31,hex(kind)
                        topic,o=string(data,0);payload=data[o:];published.append(topic)
                        if topic==base+'/availability':assert payload==b'online'
                        elif topic==base+'/state':
                            state=json.loads(payload);assert state['ip']=='192.168.40.126'
                            assert state['rtsp']=='rtsp://192.168.40.126:8554/stream'
                            assert state['telnet_port']==23 and state['snapshot_interval_s']==10
                            assert state['width']==640 and state['height']==480
                        elif topic==base+'/snapshot':
                            assert payload==jpeg,'JPEG corrupted or SDK trailer present'
                            images.append((len(connects),time.monotonic()))
                            assert len(configs)==4
                            if len(images)==1:break # abrupt disconnect after a complete image
                        else:
                            obj=json.loads(payload);assert obj['availability_topic']==base+'/availability'
                            assert obj['device']['identifiers']==['bk7252-0004b2087e6c']
                            assert topic.startswith('homeassistant/') and topic.endswith('/config')
                            if '/camera/' in topic:
                                assert obj['topic']==base+'/snapshot' and obj['encoding']==''
                                assert obj['json_attributes_topic']==base+'/state'
                            else:assert obj['state_topic']==base+'/state'
                            configs.add(topic)
        except Exception as exc:errors.append(repr(exc))
    thread=threading.Thread(target=broker,daemon=True);thread.start()
    log=work/(cfg.stem+'.log')
    try:
        with log.open('w') as out:
            subprocess.run([str(exe),str(cfg),str(fixture)],stdout=out,stderr=subprocess.STDOUT,
                env=dict(os.environ,UBSAN_OPTIONS='halt_on_error=1',ASAN_OPTIONS='detect_leaks=0'),timeout=22,check=True)
    finally:stop.set();thread.join(2);server.close()
    assert not errors,errors
    assert len(connects)>=(3 if auth else 2),connects
    assert len(images)>=3,images
    same=[t for conn,t in images if conn==images[-1][0]]
    assert len(same)>=2 and same[1]-same[0]>=10,same
    text=log.read_text();assert text.count('worker started')==1,text
    assert 'runtime error:' not in text and 'ERROR: AddressSanitizer' not in text,text
    assert 'connected=1' in text,text
    return f'PASS MQTT {cfg.stem}: auth/LWT/reconnect;4 retained HA discovery configs; exact{len(jpeg)}-byte raw JPEG without trailer;10s cadence; JSON escaping; single worker'

if __name__=='__main__':
    print(scenario(False))
    print(scenario(True))
    print(scenario(False,True))
