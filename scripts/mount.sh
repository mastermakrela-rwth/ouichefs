#! /bin/bash

IMG=${1:-test.img}

echo "Mounting $IMG"

MOUNT_POINT="/mnt/$IMG"

mkdir $MOUNT_POINT

LOOP_DEVICE=$(losetup --find)
losetup $LOOP_DEVICE $IMG
mount -o loop $LOOP_DEVICE $MOUNT_POINT