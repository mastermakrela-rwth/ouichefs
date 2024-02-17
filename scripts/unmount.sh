#! /bin/bash

IMG=${1:-test.img}

echo "Unmounting $IMG"

MOUNT_POINT="/mnt/$IMG"

umount $MOUNT_POINT

LOOP_DEVICE=$(losetup -j $IMG | awk -F':' '{print $1}')
losetup -d $LOOP_DEVICE

rm -r $MOUNT_POINT
