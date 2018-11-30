#!/bin/bash

set -x -e -o pipefail

export DEBEMAIL='packaging@wand.net.nz'
export DEBFULLNAME='WAND Packaging'
export DEBIAN_FRONTEND=noninteractive

apt-get update
apt-get install -y equivs devscripts dpkg-dev quilt curl apt-transport-https \
    apt-utils ssl-cert ca-certificates gnupg lsb-release debhelper git
apt-get upgrade -y

# Install libzstd-dev and liblz4-dev if available for optional zstd and lz4 support
apt-get install -y libzstd-dev || true
apt-get install -y liblz4-dev || true

debchange --newversion ${CI_COMMIT_REF_NAME} -b "New upstream release"
mk-build-deps -i -r -t 'apt-get -f -y --force-yes'
dpkg-buildpackage -b -us -uc -rfakeroot -j4
