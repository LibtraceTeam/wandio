#!/bin/bash

set -x -e -o pipefail
SOURCENAME=`echo ${CI_COMMIT_REF_NAME} | cut -d '-' -f 1`

if [ "$1" = "centos_7" ]; then
        yum install -y https://dl.fedoraproject.org/pub/epel/epel-release-latest-7.noarch.rpm
fi

if [ "$1" = "centos_6" ]; then
        yum install -y https://dl.fedoraproject.org/pub/epel/epel-release-latest-6.noarch.rpm
        yum install -y epel-rpm-macros
fi

yum install -y xz zstd gzip bzip2 lzop lz4

yum install -y built-packages/$1/libwandio1-${SOURCENAME}-*.rpm
yum install -y built-packages/$1/libwandio1-devel-${SOURCENAME}-*.rpm
yum install -y built-packages/$1/libwandio1-tools-${SOURCENAME}-*.rpm

cd test && ./do-basic-tests.sh

