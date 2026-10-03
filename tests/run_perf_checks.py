"""Pool ownership checks + RTSP/decoder regression checks for version09."""
from pathlib import Path
import os, subprocess, sys
root=Path(__file__).resolve().parents[2];tests=Path(__file__).resolve().parent
src=root/'beken7252-opencam/bdk_rtt/test';work=root/'.build-tools/perf-tests'
work.mkdir(parents=True,exist_ok=True)
subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I',str(src),str(tests/'test_frame_pool.c'),str(src/'cam_frame_pool.c'),'-o',str(work/'test_frame_pool')],check=True)
with (work/'pool.log').open('w') as log:
    subprocess.run([str(work/'test_frame_pool')],stdout=log,stderr=subprocess.STDOUT,env=dict(os.environ,UBSAN_OPTIONS='halt_on_error=1'),check=True)
print((work/'pool.log').read_text(),end='')
subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Werror','-Wno-unused-parameter','-fsanitize=address,undefined','-I',str(tests),str(tests/'test_video_buffer.c'),'-o',str(work/'test_video_buffer')],check=True)
with (work/'video-buffer.log').open('w') as log:
    subprocess.run([str(work/'test_video_buffer')],stdout=log,stderr=subprocess.STDOUT,env=dict(os.environ,UBSAN_OPTIONS='halt_on_error=1'),check=True)
print((work/'video-buffer.log').read_text(),end='')
subprocess.run([sys.executable,str(tests/'run_rtsp_checks.py')],check=True)
