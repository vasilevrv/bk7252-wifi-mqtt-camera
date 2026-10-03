"""Reproduce snapshot ownership, HI704 profile, RTSP and MQTT wire checks."""
from pathlib import Path
import os,subprocess,sys
root=Path(__file__).resolve().parents[2];tests=Path(__file__).resolve().parent;sdk=root/'beken7252-opencam/bdk_rtt';work=root/'.build-tools/perf-tests'
work.mkdir(parents=True,exist_ok=True)
flags=['cc','-std=c11','-D_DEFAULT_SOURCE','-Wall','-Wextra','-Werror','-Wno-unused-parameter','-fsanitize=address,undefined']
for name,includes in [('snapshot',[tests/'mqtt_host']),('hi704_profile',[tests])]:
    args=flags.copy()
    for inc in includes:args+=['-I',str(inc)]
    args+=[str(tests/('test_'+name+'.c')),'-pthread','-o',str(work/('test_'+name))]
    subprocess.run(args,check=True)
    subprocess.run([str(work/('test_'+name))],check=True)
subprocess.run([sys.executable,str(tests/'run_rtsp_checks.py')],check=True)
subprocess.run([sys.executable,str(tests/'run_mqtt_checks.py')],check=True)
args=['cc','-std=c99','-D_DEFAULT_SOURCE','-DCAM_MQTT_HOST','-fsanitize=address,undefined']
for inc in [tests/'mqtt_host',sdk/'test',sdk/'packages/pahomqtt/MQTTClient-RT',sdk/'packages/pahomqtt/MQTTPacket/src',sdk/'packages/EasyFlash/inc',sdk/'packages/EasyFlash/port']:args+=['-I',str(inc)]
args+=[str(tests/'test_mqtt_stream_deadline.c'),str(sdk/'test/cam_mqtt.c'),str(sdk/'test/cam_mqtt_config.c'),str(sdk/'test/cam_snapshot.c'),str(sdk/'packages/pahomqtt/MQTTClient-RT/mqtt_client.c')]
args+=[str(p) for p in (sdk/'packages/pahomqtt/MQTTPacket/src').glob('*.c')]+['-pthread','-o',str(work/'test_mqtt_stream_deadline')]
subprocess.run(args,check=True);subprocess.run([str(work/'test_mqtt_stream_deadline')],check=True)
