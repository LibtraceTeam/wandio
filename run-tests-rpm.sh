#!/bin/bash

set -x -e -o pipefail

if [ "$1" = "centos_7" ]; then
        yum install -y https://dl.fedoraproject.org/pub/epel/epel-release-latest-7.noarch.rpm
fi

if [ "$1" = "centos_6" ]; then
        yum install -y https://dl.fedoraproject.org/pub/epel/epel-release-latest-6.noarch.rpm
        yum install -y epel-rpm-macros
fi

yum install -y xz zstd gzip bzip2 lzop lz4

yum install -y built-packages/amd64/$1/libwandio1-${CI_COMMIT_REF_NAME}-*.rpm
yum install -y built-packages/amd64/$1/libwandio1-devel-${CI_COMMIT_REF_NAME}-*.rpm
yum install -y built-packages/amd64/$1/libwandio1-tools-${CI_COMMIT_REF_NAME}-*.rpm

cd test && ./do-basic-tests.sh

