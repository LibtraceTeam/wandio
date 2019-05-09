#!/bin/bash

set -x -e -o pipefail

export QA_RPATHS=$[ 0x0001 ]
SOURCENAME=`echo ${CI_COMMIT_REF_NAME} | cut -d '-' -f 1`

if [ "$1" = "centos7" ]; then
        yum install -y https://dl.fedoraproject.org/pub/epel/epel-release-latest-7.noarch.rpm
fi

if [ "$1" = "centos6" ]; then
        yum install -y https://dl.fedoraproject.org/pub/epel/epel-release-latest-6.noarch.rpm
        yum install -y epel-rpm-macros
fi

if [[ "$1" =~ fedora* ]]; then
        dnf install -y rpm-build rpmdevtools which 'dnf-command(builddep)'
        dnf group install -y "C Development Tools and Libraries"
        dnf builddep -y rpm/libwandio1.spec
else
        yum install -y rpm-build yum-utils rpmdevtools which
        yum groupinstall -y 'Development Tools'
        yum-builddep -y rpm/libwandio1.spec
fi

rpmdev-setuptree

./bootstrap.sh && ./configure && make dist
cp wandio-*.tar.gz ~/rpmbuild/SOURCES/${SOURCENAME}.tar.gz
cp rpm/libwandio1.spec ~/rpmbuild/SPECS/

cd ~/rpmbuild && rpmbuild -bb --define "debug_package %{nil}" SPECS/libwandio1.spec

