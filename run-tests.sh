#!/bin/bash

set -x -e -o pipefail

export DEBIAN_FRONTEND=noninteractive

apt-get update

apt-get install -y curl apt-transport-https \
    ssl-cert ca-certificates gnupg lsb-release git
apt-get upgrade -y

curl -1sLf 'https://dl.cloudsmith.io/public/wand/libwandio/cfg/setup/bash.deb.sh' | bash

apt-get update

apt-get install -y devscripts equivs
apt-get install -y zstd lzop xz-utils lz4

mk-build-deps --install --remove --tool "apt-get -o Debug::pkgProblemResolver=yes -y --no-install-recommends"

deb=`find packages/$1/libwandio1-dev* -maxdepth 1 -type f`
pkg_filename=$(basename "${deb}")
IFS=_ read pkg_name pkg_version pkg_arch <<< $(basename -s ".deb" "${pkg_filename}")

dpkg -i packages/$1/libwandio1_${pkg_version}_${pkg_arch}.deb
dpkg -i packages/$1/libwandio1-dev_${pkg_version}_${pkg_arch}.deb
dpkg -i packages/$1/wandio1-tools_${pkg_version}_${pkg_arch}.deb

cd test && ./do-basic-tests.sh
