#!/bin/bash
# Please run this script as root.

SYSTEM=`uname -s`

if [ -f /etc/debian_version ]; then
    if [ ! -f gost_qemu.tar.gz ]; then
        # Additional dependencies for QEMU
        sudo apt install -y libvirt-clients libpixman-1-dev libglib2.0-dev qemu-kvm tigervnc-viewer sshpass uml-utilities ovmf
        echo "Install QEMU 6.2.5 (GOST)"
        sshpass -p "psd" scp root@10.1.1.12:/psdData/util/gost_qemu.tar.gz .
        tar xvfz gost_qemu.tar.gz
        # Build and Install QEMU
        cd gost_qemu
        ./configure
        make -j ; make install
    fi
else
    echo "Unknown system type."
	exit 1
fi
