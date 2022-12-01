#!/bin/bash

if [ $# -ne 2 ]
  then
    echo -e "Syntax: \n  $0 [image_dir] [num_dev]\n\nExample: \n  $0 /home/psd/qemu/vm1/devices 4"
    exit 1
fi

image_dir=$1
num_dev=$(($2 - 1))
zns_size=$((128 * 1024 * 1024 * 1024))
cns_size=$((4 * 1024 * 1024))

mkdir -p $image_dir

if [ `ls -1 $image_dir/zns.*.raw  |awk 'END{print NR}'` -eq $num_dev ]; then
	echo ZNS images already exists.
	exit 1
fi

if [ ! -f $image_dir/zns.0.raw ]; then
	echo Creating device zns.0.raw and cns.0.raw;
	truncate -s $zns_size $image_dir/zns.0.raw;
	truncate -s $cns_size $image_dir/cns.0.raw;

	count=$(expr $zns_size / 10 / 1024 / 1024)
	dd if=/dev/zero of=$image_dir/zns.0.raw bs=10M count=$count
	count=$(expr $cns_size / 1 / 1024 / 1024)
	dd if=/dev/zero of=$image_dir/cns.0.raw bs=1M count=$count
fi

if [ $num_dev -gt 1 ]; then
	for i in $(seq 1 $num_dev);
	do
		echo Creating zns device zns.$i.raw;
		cp $image_dir/zns.0.raw $image_dir/zns.$i.raw
	done
fi

