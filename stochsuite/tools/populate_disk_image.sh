#!/usr/bin/env bash
# populate_disk_image.sh
#
# Mount a gem5 x86 disk image's first partition, copy the six stochsuite
# workload binaries into /home/gem5/stochsuite/ (renamed with a `_gem5`
# suffix so they don't collide with the source `.o` filenames), and
# clean up.
#
# We use Linux's `losetup --partscan` (`-P`) directly instead of the
# older `gem5/util/gem5img.py`, which detaches and reattaches the same
# loop device in quick succession and races against udev on modern
# systems ("Device or resource busy" errors).
#
# Usage:
#   sudo ./populate_disk_image.sh \
#       /path/to/disk.img \
#       /path/to/gem5 \
#       /path/to/stochsuite \
#       [/mnt/gem5img]
#
# Requires sudo (for losetup + mount). Requires `losetup` from
# util-linux >= 2.21 (any modern distro).

set -euo pipefail

if [[ $# -lt 3 || $# -gt 4 ]]; then
    echo "usage: $0 DISK_IMAGE GEM5_HOME STOCHSUITE_HOME [MOUNTPOINT]" >&2
    exit 1
fi

DISK_IMG="$1"
GEM5_HOME="$2"
STOCHSUITE_HOME="$3"
MOUNTPOINT="${4:-/mnt/gem5img}"

APPS_DIR="$STOCHSUITE_HOME/apps"

if [[ ! -f "$DISK_IMG" ]]; then
    echo "error: disk image not found at $DISK_IMG" >&2
    exit 1
fi
if [[ ! -d "$GEM5_HOME" ]]; then
    echo "error: gem5 directory not found at $GEM5_HOME" >&2
    exit 1
fi
if [[ ! -d "$APPS_DIR" ]]; then
    echo "error: stochsuite apps directory not found at $APPS_DIR" >&2
    exit 1
fi

WORKLOADS=(pi dop dropout multinomial photon tailwag)
for w in "${WORKLOADS[@]}"; do
    if [[ ! -f "$APPS_DIR/${w}.o" ]]; then
        echo "error: missing $APPS_DIR/${w}.o (run 'make' in $APPS_DIR first)" >&2
        exit 1
    fi
done

mkdir -p "$MOUNTPOINT"

LOOPDEV=""
MOUNTED=0

cleanup() {
    if (( MOUNTED )); then
        umount "$MOUNTPOINT" || true
        MOUNTED=0
    fi
    if [[ -n "$LOOPDEV" ]] && losetup "$LOOPDEV" >/dev/null 2>&1; then
        losetup -d "$LOOPDEV" || true
    fi
}
trap cleanup EXIT

# `--partscan` (-P) makes the kernel scan the partition table and create
# /dev/loopNp1, /dev/loopNp2, ... so we can mount partitions directly
# without the detach/reattach-with-offset trick that gem5img.py uses.
LOOPDEV=$(losetup --find --partscan --show "$DISK_IMG")
echo "attached $DISK_IMG to $LOOPDEV"

# Wait for udev to create the partition device nodes.
udevadm settle

# Pick the right partition. gem5 x86 images are typically GPT with a
# tiny BIOS boot partition first and the actual rootfs second, but
# older/MBR images often have the rootfs on partition 1. Auto-detect
# the first partition that holds a real Linux filesystem.
PART=""
for candidate in "${LOOPDEV}"p*; do
    [[ -b "$candidate" ]] || continue
    fstype=$(blkid -o value -s TYPE "$candidate" 2>/dev/null || true)
    case "$fstype" in
        ext2|ext3|ext4|xfs|btrfs|f2fs)
            PART="$candidate"
            echo "found rootfs on $PART (TYPE=$fstype)"
            break
            ;;
    esac
done
if [[ -z "$PART" ]]; then
    echo "error: no Linux filesystem partition found on $DISK_IMG" >&2
    echo "  partitions seen:" >&2
    for c in "${LOOPDEV}"p*; do
        [[ -b "$c" ]] || continue
        printf "    %s -> %s\n" "$c" \
            "$(blkid -o value -s TYPE "$c" 2>/dev/null || echo unknown)" >&2
    done
    exit 1
fi

mount "$PART" "$MOUNTPOINT"
MOUNTED=1
echo "mounted $PART at $MOUNTPOINT"

TARGET_DIR="$MOUNTPOINT/home/gem5/stochsuite"
mkdir -p "$TARGET_DIR"

for w in "${WORKLOADS[@]}"; do
    cp "$APPS_DIR/${w}.o" "$TARGET_DIR/${w}_gem5"
    chmod 0755 "$TARGET_DIR/${w}_gem5"
    echo "installed $TARGET_DIR/${w}_gem5"
done

sync
umount "$MOUNTPOINT"
MOUNTED=0
losetup -d "$LOOPDEV"
LOOPDEV=""
trap - EXIT

echo "disk image $DISK_IMG now contains the six stochsuite workloads"
