#!/bin/bash

# Example:
#
# 1) sudo mount /data/diskimages/rootfs-arm64.img /mnt/tst
#
# 2a) example invocation 1, multiline shell script
#   on_chroot /mnt << EOF
#     whoami
#   EOF
#
# 2b) example invocation 2, one line command
#   on_chroot /mnt -c "whoami"
#
# 2b) example invocation 3, interactive bash
#   on_chroot /mnt -c "bash"
#   
# 3) sudo ./imagetool.sh -u /mnt/tst
#

sdir=`dirname $(readlink -f $0)`
rootdir=`dirname $sdir`
parentdir=`dirname $rootdir`

echo sdir ${sdir}
echo rootdir ${rootdir}
echo parentdir ${parentdir}

username=${USER}

export ROOTFS_DIR=$1
shift 1

if [ -z "${ROOTFS_DIR}" ]; then
    echo Usage "$0 <rootfs-dir> commands..."
    exit 1
fi

rootfs_parentdir="${ROOTFS_DIR}${parentdir}"
echo rootfs_parentdir ${rootfs_parentdir}

if ! mount | grep -q "$(realpath "${ROOTFS_DIR}"/proc)"; then
    sudo mount -t proc proc "${ROOTFS_DIR}/proc"
fi

if ! mount | grep -q "$(realpath "${ROOTFS_DIR}"/dev)"; then
    sudo mount --bind /dev "${ROOTFS_DIR}/dev"
fi

if ! mount | grep -q "$(realpath "${ROOTFS_DIR}"/dev/pts)"; then
    sudo mount --bind /dev/pts "${ROOTFS_DIR}/dev/pts"
fi

if ! mount | grep -q "$(realpath "${ROOTFS_DIR}"/sys)"; then
    sudo mount --bind /sys "${ROOTFS_DIR}/sys"
fi

if ! mount | grep -q "${rootfs_parentdir}"; then
    sudo mkdir -p "${rootfs_parentdir}"
    sudo mount --bind ${parentdir} "${rootfs_parentdir}"
fi

# sudo chroot ${ROOTFS_DIR}/ /bin/bash "$@"
sudo /sbin/capsh --user=$username --drop=cap_setfcap "--chroot=${ROOTFS_DIR}/" -- -e "$@"
