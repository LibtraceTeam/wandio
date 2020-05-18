#!/bin/bash

set -x -e -o pipefail
SOURCENAME=`echo ${CI_COMMIT_REF_NAME} | cut -d '-' -f 1`
ZSTDREQ=zstd

if [ "$1" = "centos_8" ]; then
        yum install -y https://dl.fedoraproject.org/pub/epel/epel-release-latest-8.noarch.rpm

        dnf install -y 'dnf-command(config-manager)' || true
        yum config-manager --set-enabled PowerTools || true

        # XXX Temporary, until Centos updates to 8.2 where libzstd is
        # included in the base OS
        # ref: https://lists.fedoraproject.org/archives/list/epel-devel@lists.fedoraproject.org/thread/MFZCRQCULJALRIJJFSSAETSDZ4RL6GCU/
        yum install -y wget pkgconf-pkg-config
        wget -N https://archives.fedoraproject.org/pub/archive/epel/8.1/Everything/x86_64/Packages/l/libzstd-1.4.4-1.el8.x86_64.rpm && rpm -i libzstd-1.4.4-1.el8.x86_64.rpm

        wget -N https://archives.fedoraproject.org/pub/archive/epel/8.1/Everything/x86_64/Packages/l/libzstd-devel-1.4.4-1.el8.x86_64.rpm && rpm -i libzstd-devel-1.4.4-1.el8.x86_64.rpm

        wget -N https://archives.fedoraproject.org/pub/archive/epel/8.1/Everything/x86_64/Packages/z/zstd-1.4.4-1.el8.x86_64.rpm && rpm -i zstd-1.4.4-1.el8.x86_64.rpm

        ZSTDREQ=
fi

if [ "$1" = "centos_7" ]; then
        yum install -y https://dl.fedoraproject.org/pub/epel/epel-release-latest-7.noarch.rpm
fi

if [ "$1" = "centos_6" ]; then
        yum install -y https://dl.fedoraproject.org/pub/epel/epel-release-latest-6.noarch.rpm
        yum install -y epel-rpm-macros
fi

yum install -y xz ${ZSTDREQ} gzip bzip2 lzop lz4 diffutils

yum install -y built-packages/$1/libwandio1-${SOURCENAME}-*.rpm
yum install -y built-packages/$1/libwandio1-devel-${SOURCENAME}-*.rpm
yum install -y built-packages/$1/libwandio1-tools-${SOURCENAME}-*.rpm

cd test && ./do-basic-tests.sh

