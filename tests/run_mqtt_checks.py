"""Compile the actual firmware MQTT wrapper + SDK Paho and test local wire exchange."""
from pathlib import Path
import subprocess
import sys
root = Path(__file__).resolve().parents[2]
sdk = root / 'beken7252-opencam/bdk_rtt'
tests = Path(__file__).resolve().parent
work = root / '.build-tools/mqtt-tests'
work.mkdir(parents=True, exist_ok=True)
args = ['cc', '-std=c99', '-D_DEFAULT_SOURCE', '-DCAM_MQTT_HOST', '-fsanitize=address,undefined']
for path in (tests/'mqtt_host', sdk/'test', sdk/'packages/pahomqtt/MQTTClient-RT', sdk/'packages/pahomqtt/MQTTPacket/src', sdk/'packages/EasyFlash/inc', sdk/'packages/EasyFlash/port'):
    args += ['-I', str(path)]
args += [str(tests/'mqtt_host/port.c'), str(sdk/'test/cam_mqtt.c'), str(sdk/'test/cam_mqtt_config.c'), str(sdk/'test/cam_snapshot.c'), str(sdk/'packages/pahomqtt/MQTTClient-RT/mqtt_client.c')]
args += [str(p) for p in sorted((sdk/'packages/pahomqtt/MQTTPacket/src').glob('*.c'))]
args += ['-pthread', '-o', str(work/'mqtt_host')]
with (work/'compile.log').open('w') as log:
    subprocess.run(args, stdout=log, stderr=subprocess.STDOUT, check=True)
with (work/'integration.log').open('w') as log:
    subprocess.run([sys.executable, str(tests/'test_mqtt_server.py')], stdout=log, stderr=subprocess.STDOUT, check=True)
print((work/'integration.log').read_text(), end='')
