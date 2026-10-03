"""Reproduce firmware-core checks on this Mac (cc, sips, optional installed VLC)."""
from pathlib import Path
import shutil,subprocess,sys
root=Path(__file__).resolve().parents[2]; tests=Path(__file__).resolve().parent
src=root/'beken7252-opencam/bdk_rtt/test';work=root/'.build-tools/rtsp-tests'
work.mkdir(parents=True,exist_ok=True)
def run(args,log=None):
    if log:
        with (work/log).open('w') as out:subprocess.run([str(a) for a in args],cwd=root,stdout=out,stderr=subprocess.STDOUT,check=True)
    else:subprocess.run([str(a) for a in args],cwd=root,check=True)
for p in (tests/'fixtures').glob('*.jpg'):shutil.copy2(p,work/p.name)
for kind in [0,1]:run(['sips','-s','format','png',work/f'type{kind}.jpg','--out',work/f'type{kind}.png'])
flags=['cc','-std=c99','-Wall','-Wextra','-Werror']
run(flags+['-shared','-fPIC',src/'cam_rtp_jpeg.c','-o',work/'cam_rtp_jpeg.dylib'])
run([sys.executable,tests/'test_rtp_jpeg.py',work/'cam_rtp_jpeg.dylib',work/'gradient.jpg'],'packetizer.log')
run(flags+['-fsanitize=address,undefined','-I',src,tests/'fuzz_jpeg.c',src/'cam_rtp_jpeg.c','-o',work/'fuzz_jpeg'])
run([work/'fuzz_jpeg',work/'gradient.jpg'],'sanitizer.log')
run(flags+['-fsanitize=address,undefined','-DCAM_RTSP_HOST','-I',tests,'-I',tests/'mqtt_host','-I',src,src/'cam_rtsp.c',src/'cam_frame_pool.c',src/'cam_snapshot.c',src/'cam_rtp_jpeg.c',tests/'cam_rtsp_host.c','-pthread','-o',work/'rtsp_host'])
run([sys.executable,tests/'test_rtsp_server.py'],'integration.log')
text=(work/'host.log').read_text(errors='replace')
assert 'ERROR: AddressSanitizer' not in text and 'runtime error:' not in text
for log in ['packetizer.log','sanitizer.log','integration.log']:print((work/log).read_text(),end='')
