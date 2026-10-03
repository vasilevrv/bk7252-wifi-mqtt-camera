"""Build actual credential/retry/EasyFlash code with a simulated flash port."""
from pathlib import Path
import os
import subprocess
root = Path(__file__).resolve().parents[2]
sdk = root / 'beken7252-opencam/bdk_rtt'
work = root / '.build-tools/auto-tests'
work.mkdir(parents=True, exist_ok=True)
source = sdk / 'test'
ef = sdk / 'packages/EasyFlash'
args = ['cc', '-std=c99', '-Wall', '-Wextra', '-Werror', '-fsanitize=address,undefined']
for path in (source, ef / 'inc', ef / 'port'):
    args += ['-I', str(path)]
args += [str(Path(__file__).with_name('test_cam_auto.c')), str(source / 'cam_wifi_config.c'), str(source / 'cam_mqtt_config.c')]
args += [str(ef / 'src' / name) for name in ('easyflash.c', 'ef_env.c', 'ef_utils.c')]
args += ['-o', str(work / 'test_cam_auto')]
subprocess.run(args, check=True)
env = dict(os.environ, UBSAN_OPTIONS='halt_on_error=1')
with (work / 'checks.log').open('w') as log:
    subprocess.run([str(work / 'test_cam_auto')], stdout=log, stderr=subprocess.STDOUT, env=env, check=True)
print((work / 'checks.log').read_text(), end='')
