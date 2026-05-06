#!/usr/bin/env bash
# Summarize a gem5 full-system stochsuite run from m5out/ (no sudo).
#
# Usage:
#   GEM5_OUT=/path/to/m5out DISK_IMG=/path/to/stochsuite-base.img \
#       ./check_gem5_run.sh

set -euo pipefail

GEM5_OUT="${GEM5_OUT:-$PWD/../gem5/m5out}"
DISK_IMG="${DISK_IMG:-$PWD/../disk_images/stochsuite-base.img}"
# Byte offset to root partition (GPT: sda2 starts at sector 4096 on x86-ubuntu-24.04-img)
PART_OFF="${PART_OFF:-2097152}"

echo "=== 1) Serial (last 25 lines): $GEM5_OUT/board.pc.com_1.device ==="
if [[ -f "$GEM5_OUT/board.pc.com_1.device" ]]; then
    tail -n 25 "$GEM5_OUT/board.pc.com_1.device"
else
    echo "(missing)"
fi

echo
echo "=== 2) Latest readfile_* (host copy of readfile_contents) ==="
shopt -s nullglob
rf=( "$GEM5_OUT"/readfile_* )
if ((${#rf[@]})); then
    latest=$(ls -t "${rf[@]}" | head -1)
    echo "File: $latest"
    cat "$latest"
    echo
else
    echo "(none)"
fi

echo
echo "=== 3) stats.txt — sim time + KVM vs Timing cycles ==="
if [[ -f "$GEM5_OUT/stats.txt" ]]; then
    grep -E '^simSeconds|board\.processor\.(start|switch)\.core\.numCycles' \
        "$GEM5_OUT/stats.txt" || true
else
    echo "(missing)"
fi

echo
echo "=== 4) branch.log (line count + head) ==="
if [[ -f "$GEM5_OUT/branch.log" ]]; then
    wc -l "$GEM5_OUT/branch.log"
    head -n 5 "$GEM5_OUT/branch.log"
else
    echo "(missing)"
fi

echo
echo "=== 5) Disk image — search for pi_gem5 in rootfs partition (first 4 GiB) ==="
if [[ -f "$DISK_IMG" ]]; then
    python3 - "$DISK_IMG" "$PART_OFF" << 'PY'
import sys
path, off = sys.argv[1], int(sys.argv[2])
needle = b"pi_gem5"
chunksz = 64 * 1024 * 1024
pos = 0
found = None
with open(path, "rb") as f:
    while pos < 4 * 1024**3:
        f.seek(off + pos)
        chunk = f.read(chunksz)
        if not chunk:
            break
        i = chunk.find(needle)
        if i != -1:
            found = off + pos + i
            break
        pos += len(chunk)
if found is not None:
    print(f"FOUND pi_gem5 at byte offset {found} (guest file likely on image)")
else:
    print("NOT FOUND in first 4 GiB of partition — repopulate disk image")
PY
    echo
    echo "For full ls -l /home/gem5/stochsuite run (needs sudo):"
    echo "  sudo $STOCHSUITE_HOME/tools/populate_disk_image.sh will mount the image;"
    echo "  or manually: sudo losetup -fP $DISK_IMG && sudo mount /dev/loopNp2 /mnt && ls -la /mnt/home/gem5/stochsuite"
else
    echo "(disk image missing: $DISK_IMG)"
fi
