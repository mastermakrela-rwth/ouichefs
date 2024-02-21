# Requirements

- [x] Implement a least-recently used eviction policy deleting the file with the oldest modification time.
- [x] (Optional) Implement a least-recently accessed eviction policy deleting the file with the oldest access time.
- [x] (Optional) Implement a least-recently changed eviction policy deleting the file with the oldest change time.

- [x] Implement the ability to dynamically insert eviction policies.
- [x] Implement the ability to dynamically change the eviction policy.

- [x] Implement manual triggering of the eviction process.

- Implement automatic triggering of the eviction process.
  - [x] When less than X% of the blocks are left free on the partition.
  - [x] When a directory is full and a new file is created in it.

# How to use

## Compilation (needed?)

```bash
make
```

## Basic setup

After copying your \*.ko files to the target machine, you can load the base module and the image with:

```bash
insmod ouichefs.ko
./scripts/mount.sh test.img
```

## Inserting a new eviction policy

After inserting `ouichefs.ko` there will always be a base eviction policy. You can insert new eviction policies with:

```bash
insmod wich_size.ko
insmod wich_lru.ko mode=3
```

## Changing the eviction policy

You can retrieve the available policies with:

```bash
cat /proc/ouiche/eviction
```

```bash
Following eviction policies are available:
default (does nothing)
wich_size
wich_lru        [ACTIVE]
```

By default the last inserted policy is active. You can change the active policy with:

```bash
echo -n "wich_size" > /proc/ouiche/eviction
```

```bash
ouichefs:evictions_proc_write: Received policy name: wich_size
set eviction policy to 'wich_size'
```

## Manual eviction

To manually trigger the eviction process, you need the target partion. You can retrieve the available partitions of ouichefs with:

```bash
cat /proc/ouiche/partitions
```

```bash
Following partitions use ouiche_fs:
0:/dev/loop1
```

You can manually trigger the eviction process with:

```bash
echo -n "0:/dev/loop1" > /proc/ouiche/clean
```

## Removing the module

You can remove the module with:

```bash
./scripts/umount.sh test.img
rmmod wich_size.ko
rmmod wich_lru.ko
rmmod ouichefs.ko
```
