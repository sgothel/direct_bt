#!/bin/bash

#
# sudo mkdir -p /mnt/arm64
# sudo mount /data/diskimages/RaspiArm64LightDev-1.3-rw-deb11-arm64-litexdev.root-ext4.img /mnt/arm64
# scripts/build-preset-cross.sh /mnt/arm64 debug-clang
#
# sudo mkdir -p /mnt/arm32
# sudo mount /data/diskimages/RaspiArm32LightDev-1.2-rw-deb10-armhf-litexdev.root-ext4.img /mnt/arm32
# scripts/build-preset-cross.sh /mnt/arm32 debug-clang
#

sdir=`dirname $(readlink -f $0)`
rootdir=`dirname $sdir`
parentdir=`dirname $rootdir`

echo sdir ${sdir}
echo rootdir ${rootdir}
echo parentdir ${parentdir}

export ROOTFS_DIR=$1
shift 1
export preset_name=$1
shift 1

if [ -z "${ROOTFS_DIR}" -o -z "${preset_name}" ]; then
    echo Usage "$0 <rootfs-dir> <preset_name-name>"
    exit 1
fi

${sdir}/on_chroot.sh ${ROOTFS_DIR} << EOF
    sh ${sdir}/build-preset.sh ${preset_name}
EOF
