# BK7252 Wi-Fi MQTT Camera

A focused camera firmware based on [Daniel Doña's beken7252-opencam](https://github.com/daniel-dona/beken7252-opencam). Thank you to Daniel for the original OpenCam project, camera support and hardware documentation, and to the Beken SDK, RT-Thread and library contributors whose work makes this firmware possible. Upstream authorship and component license notices are retained.

The tested hardware is **BK7252UQN68 with an HI704 camera sensor**. The firmware runs **RT-Thread 3.1**, using the existing Beken SDK. It does not use FreeRTOS.

## What is included

- Automatic Wi-Fi connection with DHCP and reconnect attempts every 20 seconds.
- VGA 640×480 JPEG video over RTSP at `rtsp://<camera-ip>:8554/stream`.
- RTP/JPEG and RTCP interleaved over **TCP**, with one simultaneous RTSP client. VLC's default UDP request is rejected with `461 Unsupported Transport`, allowing fallback to TCP.
- HI704 exposure tuning, bounded frame buffers and network diagnostics.
- Optional MQTT with automatic reconnection and Home Assistant Discovery: JPEG snapshots every 10 seconds, IP address, RTSP URL and source FPS.
- Wi-Fi and MQTT settings stored in internal flash; SD is only needed for initial provisioning or changing settings.
- UART/MSH and Telnet on port 23 for configuration status and diagnostics.

The user tested OpenCam14 on 2026-10-03 and confirmed normal operation with no freezes. Earlier controlled TCP measurements reached approximately 14–15 FPS. Actual FPS and latency depend on lighting, scene detail and network conditions.

## What is not included

No audio recording/playback, H.264/H.265 encoding, cloud service, FTP storage server or UDP video transport. RTSP and Telnet have no authentication; MQTT uses TCP without TLS. The legacy HTTP camera code remains in the SDK, but the autostart path starts RTSP rather than HTTP. This is not a complete removal of all vendor SDK components: dependencies of the closed Wi-Fi library remain.

JPEG frames are limited to 48 KiB; larger frames may be dropped. The 48-pin BK7252 variant and other board layouts have not been validated. HI704 is the tested sensor; existing GC0311 and GC0328C drivers remain available in the SDK but have not been validated with this release.

## Provisioning

Copy [sd/wifi.conf.example](sd/wifi.conf.example) and [sd/mqtt.conf.example](sd/mqtt.conf.example) to the SD card as `wifi.conf` and `mqtt.conf`. Replace the example values without quotes, insert the card and reboot.

```ini
# wifi.conf
ssid=YOUR_WIFI_SSID
password=YOUR_WIFI_PASSWORD
```

Wi-Fi passwords must contain 8–63 characters; an empty `password=` selects an open network.

```ini
# mqtt.conf
enabled=1
host=192.168.1.10
port=1883
username=YOUR_MQTT_USERNAME
password=YOUR_MQTT_PASSWORD
client_id=
topic_base=
```

Keys must start at the beginning of each line. Empty `client_id` and `topic_base` derive unique values from the device MAC. Explicit identifiers must differ between cameras. Set `enabled=0` to disable MQTT. For an anonymous broker, omit or leave both `username` and `password` empty.

At startup, valid SD settings override saved settings and persist in EasyFlash. The camera then works without an SD card. Wi-Fi changes on a mounted SD card are checked every 20 seconds; MQTT settings are read at startup. If SD was absent at boot, reboot after inserting it. Invalid configuration does not replace previous settings. `/sd/init.msh` is not required by this autostart implementation.

Useful shell commands:

```text
wifi w0 status
ifconfig
cam_auto status
cam_mqtt
rtsp_cam status
cam_probe
```

Stop the camera before using `cam_probe`. A working HI704 on the tested board responds on I2C2 at address `0x30`, with register `0x04 = 0x96`.

## SDK layout

The SDK remains an upstream submodule pointing to [daniel-dona/bdk_rtt](https://github.com/daniel-dona/bdk_rtt), pinned to commit `f666dcc9250c5b9a881110d008b443e6f4768db0`. This repository carries our changes as [patches/opencam14-sdk.patch](patches/opencam14-sdk.patch), not a separate published SDK fork. The patch includes the added camera application files as well as modifications to existing SDK files.

The build fetches the SDK if necessary, verifies its exact base revision and patch checksum, and applies the patch. Subsequent builds verify the already-applied patch. Unexpected SDK edits cause an error; the script does not reset or overwrite them. Submodule working-tree changes from the patch are hidden from the parent repository's normal status display; inspect them with `git -C bdk_rtt status`.

Individual components retain their upstream licenses. This repository does not assign a new blanket license to the vendor SDK or its binaries. Firmware binaries are generated locally and are not included in the current tracked project tree.

## Build

Requirements: Git, **Python 2.7**, Python 3 and **ARM GCC 5.4-2016q3**. SCons 3.1.2 is already included. The original toolchain bundled with the upstream project targets Linux; on macOS use the matching macOS toolchain. Its x86_64 binaries require Rosetta on Apple Silicon.

Clone this OpenCam fork, then build:

```sh
git clone --recurse-submodules https://github.com/vasilevrv/bk7252-wifi-mqtt-camera.git beken7252-opencam
cd beken7252-opencam
PYTHON2=/path/to/python2.7 \
RTT_EXEC_PATH=/path/to/gcc-arm-none-eabi-5_4-2016q3/bin \
./build_local.sh
```

On the existing development Mac, tools are detected from the adjacent `.build-tools` directory, so `./build_local.sh` is sufficient. Override `PYTHON2`, `PYTHON3`, `RTT_EXEC_PATH` or `JOBS` as needed. The SDK target name `bk7251` also applies to this BK7252 build.

Output files:

```text
build-local/build.log
build-local/rtthread.bin
build-local/opencam_app_crc.bin
```

The script separately adds the chip's CRC blocks because the SDK's Linux packaging commands do not work directly on macOS. The verified OpenCam14 packed image has SHA-256 `5fd6147e9ca40bf3e515ae9ce4accf329ed5c609accde22c6db7f3b2ea1951e3`. Rebuilds with the existing development toolchain reproduced it byte-for-byte; other environments may produce different binaries.

## Flash and connect

Keep a full factory backup and close UART monitors before flashing. Using the existing workspace flasher, from this repository:

```sh
../bk7252-cam/tools/flasher/uartprogram \
  -p /dev/tty.wchusbserial2110 --segment app \
  build-local/opencam_app_crc.bin
```

The flasher is a separate tool, not bundled here. Enter the bootloader using the usual power cycle, write only the application and reboot. The physical application address is `0x11000`; the verified OpenCam14 erase range ends at `0xd7000`. EasyFlash settings start at `0x1fe000`. Do not erase the entire flash when retaining settings.

Open `rtsp://<camera-ip>:8554/stream` in VLC. For a smaller playback buffer on macOS:

```sh
/Applications/VLC.app/Contents/MacOS/VLC \
  --rtsp-tcp --network-caching=200 rtsp://<camera-ip>:8554/stream
```

## Checks

Apply the SDK patch first with `python3 scripts/prepare_sdk.py`. From the parent workspace, with the checkout named `beken7252-opencam`:

```sh
python3 beken7252-opencam/tests/run_auto_checks.py
python3 beken7252-opencam/tests/run_ha_checks.py
python3 beken7252-opencam/tests/measure_rtsp.py <camera-ip> --seconds 30
```

The host suite uses a system C compiler, ASan/UBSan, Python 3, macOS `sips`, local test sockets and optionally installed VLC. Configuration/EasyFlash, snapshot ownership, HI704 exposure rollback, JPEG fuzzing, RTSP lifecycle and decoding, MQTT Discovery, snapshot cadence and reconnect checks passed. Tests currently use the checkout name shown above and store results in the adjacent `.build-tools` directory.
