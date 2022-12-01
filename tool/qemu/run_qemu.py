#!/usr/bin/env python3

import os
import sys
import subprocess
import argparse
import re
from uuid import getnode

qemu_dir = "/opt/qemu"
vm_name = "vm"
device_dir = "devices"
image_dir = "images"
vm_img = "pbssd-ubuntu2004_legacy.qcow2"
# vm_img = "pbssd-ubuntu2004_uefi.qcow2"
num_device = 4

###########################
need_install_qemu = False
need_make_qemu_image = False
need_make_device_image = False
###########################
logical_block_size = 4096
physical_block_size = 4096
zone_size = "4G"
zone_capacity = "4G"
descr_ext_size = 128
max_open = 14
max_active = 14
zrwa_size = 2*1024*1024
zrwa_flush_granurity = 128*1024
###########################


def install_qemu():
    subprocess.call(f"./install_qemu.sh", shell=True)


def make_qemu_image(vm_dir):
    dest_dir = vm_dir + "/" + image_dir
    subprocess.call(
        f"./create_qemu_image.sh {dest_dir} {vm_img}", shell=True)


def make_device_image(vm_dir):
    dest_dir = vm_dir + "/" + device_dir
    subprocess.call(f"./create_device.sh {dest_dir} 4", shell=True)


def start_qemu(vm_dir):
    dest_device = vm_dir + "/" + device_dir
    dest_image = vm_dir + "/" + image_dir

    host_mac_address = ':'.join(re.findall('..', '%012x' % getnode())[0:-1])
    host_mac_address += ":%0.2X" % args.id
    qemu_command = f"qemu-system-x86_64 "
    qemu_command += "-m 32G "
    qemu_command += "-enable-kvm "
    # qemu_command += "-drive if=pflash,format=raw,readonly=yes,file=/usr/share/OVMF/OVMF_CODE.fd "
    # qemu_command += "-drive if=pflash,format=raw,file=/usr/share/OVMF/OVMF_VARS.fd "
    qemu_command += f"-drive if=virtio,file={dest_image}/{vm_img},cache=none "
    qemu_command += "-smp 12 "
    qemu_command += "-cpu Icelake-Server "
    qemu_command += "-rtc base=utc,driftfix=slew "
    qemu_command += "-global ICH9-LPC.disable_s3=1 "
    qemu_command += "-global ICH9-LPC.disable_s4=1 "
    qemu_command += "-boot strict=on "
    qemu_command += "-machine q35,accel=kvm,kernel-irqchip=split "
    qemu_command += "-object rng-random,id=objrng0,filename=/dev/urandom "
    qemu_command += "-msg timestamp=on "
    qemu_command += "-device intel-iommu "
    qemu_command += "-net user "
    qemu_command += f"-net nic -device virtio-net,netdev=net0,mac={host_mac_address} -netdev tap,id=net0,vhost=on,script=qemu-ifup-br,downscript=qemu-ifdown-br "
    qemu_command += "-device pcie-root-port,port=0x10,chassis=1,id=pci.1,bus=pcie.0,multifunction=on,addr=0x4 "

    for index in range(0, num_device):
        qemu_command += f"-device nvme,id=nvme-ctrl-{index},serial=poszns{index} "
        qemu_command += f"-drive file={dest_device}/zns.{index}.raw,format=raw,if=none,id=nvm-{index}1 "
        qemu_command += f"-device nvme-ns,drive=nvm-{index}1,logical_block_size={logical_block_size},physical_block_size={physical_block_size},zoned=true,zoned.zone_size={zone_size},zoned.zone_capacity={zone_capacity},zoned.descr_ext_size={descr_ext_size},zoned.max_open={max_open},zoned.max_active={max_active} "
        qemu_command += f"-drive file={dest_device}/cns.{index}.raw,format=raw,if=none,id=nvm-{index}2 "
        qemu_command += f"-device nvme-ns,drive=nvm-{index}2,logical_block_size={logical_block_size},physical_block_size={physical_block_size} "
    for index in range(0, num_device):
        qemu_command += f"-device nvme,id=nvme-ctrl-tlc{index},serial=postlc{index} "
        qemu_command += f"-drive file={dest_device}/tlc.{index}.raw,format=raw,if=none,id=tlc-{index}1 "
        qemu_command += f"-device nvme-ns,drive=tlc-{index}1,logical_block_size={logical_block_size},physical_block_size={physical_block_size} "

    subprocess.call(qemu_command, shell=True)


def parse_arguments(args):
    parser = argparse.ArgumentParser(description='code formatter')
    parser.add_argument('-i', '--id', default='', type=int, help='VM id')
    args = parser.parse_args()

    return args


def print_help():
    print("Usage: run_qemu.py -i <vm_name>")


if __name__ == "__main__":
    args = parse_arguments(sys.argv)
    if args.id == '':
        print_help()
        exit
    else:
        vm_name += str(args.id)
        if(need_install_qemu is True):
            install_qemu()
        if(need_make_qemu_image is True):
            make_qemu_image((qemu_dir + "/" + vm_name))
        if(need_make_device_image is True):
            make_device_image((qemu_dir + "/" + vm_name))

    start_qemu((qemu_dir + "/" + vm_name))
