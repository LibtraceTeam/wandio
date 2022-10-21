#!/bin/bash

set -x -e -o pipefail

export DEBEMAIL='packaging@wand.net.nz'
export DEBFULLNAME='WAND Packaging'
export DEBIAN_FRONTEND=noninteractive

export SOURCENAME=`echo ${GITHUB_REF##*/} | cut -d '-' -f 1`

LSB=`lsb_release -cs`

apt-get update
apt-get install -y equivs devscripts dpkg-dev quilt curl apt-transport-https \
    apt-utils ssl-cert ca-certificates gnupg lsb-release debhelper git \
    pkg-config

curl -1sLf 'https://dl.cloudsmith.io/public/wand/libwandio/cfg/setup/bash.deb.sh' | bash

apt-get update
apt-get upgrade -y

if [ "${LSB}" == "bionic" ]; then
    apt install -y debhelper/${LSB}-backports
fi


