#!/bin/bash

set -x -e -o pipefail
SOURCENAME=`echo ${GITHUB_REF##*/} | cut -d '-' -f 1`

ZSTDREQ=zstd

if [ "$1" = "centos_8" ]; then
        yum install -y https://dl.fedoraproject.org/pub/epel/epel-release-latest-8.noarch.rpm

        dnf install -y 'dnf-command(config-manager)' || true
        yum config-manager --set-enabled PowerTools || true

        # XXX Temporary, until Centos updates to 8.2 where libzstd is
        # included in the base OS
        # ref: https://lists.fedoraproject.org/archives/list/epel-devel@lists.fedoraproject.org/thread/MFZCRQCULJALRIJJFSSAETSDZ4RL6GCU/

        wget -N https://archives.fedoraproject.org/pub/archive/epel/8.1/Everything/x86_64/Packages/z/zstd-1.4.4-1.el8.x86_64.rpm && rpm -U zstd-1.4.4-1.el8.x86_64.rpm

        ZSTDREQ=
fi

if [ "$1" = "centos_7" ]; then
        yum install -y https://dl.fedoraproject.org/pub/epel/epel-release-latest-7.noarch.rpm
fi

yum install -y xz ${ZSTDREQ} gzip bzip2 lzop lz4 diffutils

yum install -y packages/$1/libwandio1-${SOURCENAME}-*.rpm
yum install -y packages/$1/libwandio1-devel-${SOURCENAME}-*.rpm
yum install -y packages/$1/libwandio1-tools-${SOURCENAME}-*.rpm

cd test && ./do-basic-tests.sh

