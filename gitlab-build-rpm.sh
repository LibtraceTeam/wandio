#!/bin/bash

set -x -e -o pipefail

export QA_RPATHS=$[ 0x0001 ]

if [ "$1" = "centos7" ]; then
        yum install -y https://dl.fedoraproject.org/pub/epel/epel-release-latest-7.noarch.rpm
fi

if [ "$1" = "centos6" ]; then
        yum install -y https://dl.fedoraproject.org/pub/epel/epel-release-latest-6.noarch.rpm
        yum install -y epel-rpm-macros
fi

yum install -y bzip2-devel coreutils diffutils gcc gcc-c++ git libcurl-devel \
    libzstd-devel lz4-devel lzo-devel make passwd patch python \
    rpm-build rpm-devel rpmlint rpmdevtools tar vim xz-devel zlib-devel \
    automake libtool autoconf

rpmdev-setuptree

./bootstrap.sh && ./configure && make dist
cp wandio-*.tar.gz ~/rpmbuild/SOURCES/${CI_COMMIT_REF_NAME}.tar.gz
cp rpm/libwandio1.spec ~/rpmbuild/SPECS/

cd ~/rpmbuild && rpmbuild -bb --define "debug_package %{nil}" SPECS/libwandio1.spec

