#!/bin/bash
# Script outline to install and build kernel.
# Author: Siddhant Jajoo.

set -e
set -u

OUTDIR=/tmp/aeld
KERNEL_REPO=git://git.kernel.org/pub/scm/linux/kernel/git/stable/linux-stable.git
KERNEL_VERSION=v5.1.10
BUSYBOX_VERSION=1_33_1
FINDER_APP_DIR=$(realpath $(dirname $0))
ARCH=arm64
CROSS_COMPILE=aarch64-none-linux-gnu-

if [ $# -lt 1 ]
then
	echo "Using default directory ${OUTDIR} for output"
else
	OUTDIR=$1
	echo "Using passed directory ${OUTDIR} for output"
fi

mkdir -p ${OUTDIR}

# Check if creation is successful
if [ $? -ne 0 ]; then
	echo "Failed to create ${OUTDIR}"
	exit 1
fi

cd "$OUTDIR"
if [ ! -d "${OUTDIR}/linux-stable" ]; then
    #Clone only if the repository does not exist.
	echo "CLONING GIT LINUX STABLE VERSION ${KERNEL_VERSION} IN ${OUTDIR}"
	git clone ${KERNEL_REPO} --depth 1 --single-branch --branch ${KERNEL_VERSION}
fi
if [ ! -e ${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image ]; then
    cd linux-stable
    echo "Checking out version ${KERNEL_VERSION}"
    git checkout ${KERNEL_VERSION}

    # TODO: Add your kernel build steps here
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} mrproper  # Deep clean the kernel build tree and remove existing .config file
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} defconfig  # Configure "virt" arm dev board for QEMU
    make -j4 ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} all  # Build a kernel image for booting with QEMU
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} modules  # Build kernel modules
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} dtbs  # Build the devicetree
fi

echo "Adding the Image in outdir"
cp ${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image ${OUTDIR}

echo "Creating the staging directory for the root filesystem"
cd "$OUTDIR"
if [ -d "${OUTDIR}/rootfs" ]
then
	echo "Deleting rootfs directory at ${OUTDIR}/rootfs and starting over"
    sudo rm  -rf ${OUTDIR}/rootfs
fi

# TODO: Create necessary base directories
mkdir -p "${OUTDIR}/rootfs/bin" "${OUTDIR}/rootfs/dev" "${OUTDIR}/rootfs/etc" "${OUTDIR}/rootfs/home" "${OUTDIR}/rootfs/lib" "${OUTDIR}/rootfs/lib64" "${OUTDIR}/rootfs/proc" "${OUTDIR}/rootfs/sbin" "${OUTDIR}/rootfs/sys" "${OUTDIR}/rootfs/tmp" "${OUTDIR}/rootfs/usr" "${OUTDIR}/rootfs/var"
mkdir -p "${OUTDIR}/rootfs/usr/bin" "${OUTDIR}/rootfs/usr/lib" "${OUTDIR}/rootfs/usr/sbin"
mkdir -p "${OUTDIR}/rootfs/var/log"

cd "$OUTDIR"
if [ ! -d "${OUTDIR}/busybox" ]
then
git clone git://busybox.net/busybox.git
    cd busybox
    git checkout ${BUSYBOX_VERSION}
    # TODO:  Configure busybox
    make distclean
    make defconfig
else
    cd busybox
fi

# TODO: Make and install busybox
make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE}
make CONFIG_PREFIX="${OUTDIR}/rootfs" ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} install

echo "Library dependencies"
${CROSS_COMPILE}readelf -a "${OUTDIR}/rootfs/bin/busybox" | grep "program interpreter"
${CROSS_COMPILE}readelf -a "${OUTDIR}/rootfs/bin/busybox" | grep "Shared library"

# TODO: Add library dependencies to rootfs
sudo ln -s /home/aamir/AESD/cross-compiler/gcc-arm-10.3-2021.07-x86_64-aarch64-none-linux-gnu/aarch64-none-linux-gnu/libc/lib/ld-linux-aarch64.so.1 ${OUTDIR}/rootfs/lib
sudo ln -s /home/aamir/AESD/cross-compiler/gcc-arm-10.3-2021.07-x86_64-aarch64-none-linux-gnu/aarch64-none-linux-gnu/libc/lib64/libc.so.6 ${OUTDIR}/rootfs/lib64
sudo ln -s /home/aamir/AESD/cross-compiler/gcc-arm-10.3-2021.07-x86_64-aarch64-none-linux-gnu/aarch64-none-linux-gnu/libc/lib64/libm.so.6 ${OUTDIR}/rootfs/lib64
sudo ln -s /home/aamir/AESD/cross-compiler/gcc-arm-10.3-2021.07-x86_64-aarch64-none-linux-gnu/aarch64-none-linux-gnu/libc/lib64/libresolv.so.2 ${OUTDIR}/rootfs/lib64

# TODO: Make device nodes
sudo mknod -m 666 "${OUTDIR}/rootfs/dev/null" c 1 3  # Null device
sudo mknod -m 600 "${OUTDIR}/rootfs/dev/console" c 5 1 # Console device

# TODO: Clean and build the writer utility
cd /home/aamir/AESD/assignments-3-and-later-aasu8675/finder-app
make clean
make CROSS_COMPILE=${CROSS_COMPILE}

# TODO: Copy the finder related scripts and executables to the /home directory
# on the target rootfs
cp /home/aamir/AESD/assignments-3-and-later-aasu8675/finder-app/writer "${OUTDIR}/rootfs/home"
cp /home/aamir/AESD/assignments-3-and-later-aasu8675/finder-app/writer.sh "${OUTDIR}/rootfs/home"
cp /home/aamir/AESD/assignments-3-and-later-aasu8675/conf/username.txt "${OUTDIR}/rootfs/home"
cp /home/aamir/AESD/assignments-3-and-later-aasu8675/finder-app/finder-test.sh "${OUTDIR}/rootfs/home"
cp /home/aamir/AESD/assignments-3-and-later-aasu8675/finder-app/autorun-qemu.sh "${OUTDIR}/rootfs/home"

# TODO: Chown the root directory
cd "${OUTDIR}/rootfs"
sudo chown -R root:root *

# TODO: Create initramfs.cpio.gz
find . | cpio -H newc -ov --owner root:root > ${OUTDIR}/initramfs.cpio
cd "${OUTDIR}"
gzip -f initramfs.cpio
