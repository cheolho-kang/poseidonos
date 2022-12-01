#!/bin/bash

if [ $# -ne 2 ]
  then
    echo -e "Syntax: \n  $0 [image_dir] [vm_image]\n\nExample: \n  $0 /home/psd/qemu-zns/qemu/qemu-6.2.0/build pbssd-ubuntu2004.qcow2"
    exit 1
fi

use_local_server="y"
image_dir=$1
vmimg=$2
install_img="ubuntu-20.04.2-live-server-amd64.iso"

if [ ! -f ${image_dir}/$vmimg ]; then
    if [ $use_local_server = "y" ]; then
        echo "Download ${vmimg}"
        sshpass -p "psd" scp root@10.1.1.12:/psdData/util/$vmimg ${image_dir}/
    else
        echo "Install $install_img"
        qemu-img create -f qcow2 ${image_dir}/${vmimg} 128G
        qemu-system-x86_64 \
            -m 128G \
            -enable-kvm \
            -drive if=virtio,file=${image_dir}/${vmimg},cache=none \
            -drive if=pflash,format=raw,readonly=yes,file=/usr/share/OVMF/OVMF_CODE.fd \
            -drive if=pflash,format=raw,file=/usr/share/OVMF/OVMF_VARS.fd \
            -net nic -device virtio-net,netdev=net0 -netdev tap,id=net0,vhost=on,script=qemu-ifup-br,downscript=qemu-ifdown-br \
            -cdrom ${install_img}
        sshpass -p "psd" scp root@10.1.1.12:/psdData/util/pbssd-ubuntu2004.qcow2 .
        qemu-system-x86_64 -m 64G -enable-kvm -drive if=virtio,file=${vmimg},cache=none -cdrom ${install_img}
    fi
fi

