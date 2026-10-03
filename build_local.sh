#!/bin/sh
set -eu
project_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
workspace_dir=$(dirname "$project_dir")
python3_bin="${PYTHON3:-python3}"
"$python3_bin" "$project_dir/scripts/prepare_sdk.py"
if [ -d "$workspace_dir/.build-tools/gcc-arm-none-eabi-5_4-2016q3/bin" ]; then
    default_gcc="$workspace_dir/.build-tools/gcc-arm-none-eabi-5_4-2016q3/bin"
else
    default_gcc="$project_dir/gcc-arm-none-eabi-5_4-2016q3/bin"
fi
export RTT_EXEC_PATH="${RTT_EXEC_PATH:-$default_gcc}"
if [ -x "$workspace_dir/.build-tools/bin/python" ]; then
    default_python2="$workspace_dir/.build-tools/bin/python"
else
    default_python2=python2
fi
python2_bin="${PYTHON2:-$default_python2}"
[ -x "$RTT_EXEC_PATH/arm-none-eabi-gcc" ] || { echo "ARM GCC not found: $RTT_EXEC_PATH" >&2; exit 1; }
"$python2_bin" -c 'import sys; assert sys.version_info[0] == 2' || { echo 'Python 2.7 is required by this legacy SDK.' >&2; exit 1; }
output_dir="$project_dir/build-local"
mkdir -p "$output_dir"
export PATH="$(dirname "$python2_bin"):$PATH"
export PYTHONPATH="$project_dir/scons-3.1.2/engine${PYTHONPATH:+:$PYTHONPATH}"
cd "$project_dir/bdk_rtt"
"$python2_bin" "$project_dir/scons-3.1.2/script/scons" --beken=bk7251 -j "${JOBS:-8}" > "$output_dir/build.log" 2>&1
cp out/rtthread.bin "$output_dir/rtthread.bin"
"$python3_bin" "$project_dir/scripts/pack_crc.py" "$output_dir/rtthread.bin" "$output_dir/opencam_app_crc.bin"
shasum -a 256 "$output_dir/opencam_app_crc.bin"
