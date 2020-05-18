#!/bin/bash

set -x -e -o pipefail

export DEBIAN_FRONTEND=noninteractive

apt-get update

apt-get install -y curl apt-transport-https \
    ssl-cert ca-certificates gnupg lsb-release git
apt-get upgrade -y

echo "deb https://dl.bintray.com/wand/general $(lsb_release -sc) main" | tee -a /etc/apt/sources.list.d/wand.list

curl --silent "https://bintray.com/user/downloadSubjectPublicKey?username=wand" | apt-key add -

apt-get update

apt-get install -y devscripts equivs
apt-get install -y zstd lzop xz-utils liblz4-tool

mk-build-deps --install --remove --tool "apt-get -o Debug::pkgProblemResolver=yes -y --no-install-recommends"

deb=`find built-packages/$1/libwandio1-dev* -maxdepth 1 -type f`
pkg_filename=$(basename "${deb}")
IFS=_ read pkg_name pkg_version pkg_arch <<< $(basename -s ".deb" "${pkg_filename}")

dpkg -i built-packages/$1/libwandio1_${pkg_version}_${pkg_arch}.deb
dpkg -i built-packages/$1/libwandio1-dev_${pkg_version}_${pkg_arch}.deb
dpkg -i built-packages/$1/wandio1-tools_${pkg_version}_${pkg_arch}.deb

cd test && ./do-basic-tests.sh
