#!/bin/bash

set -x -e -o pipefail

apt-get update

apt-get install -y curl apt-transport-https \
    ssl-cert ca-certificates gnupg lsb-release git
apt-get upgrade -y

echo "deb https://dl.bintray.com/wand/general $(lsb_release -sc) main" | tee -a /etc/apt/sources.list.d/wand.list

curl --silent "https://bintray.com/user/downloadSubjectPublicKey?username=wand" | gpg --no-default-keyring --keyring gnupg-ring:/etc/apt/trusted.gpg.d/wand.gpg --import

chmod 644 /etc/apt/trusted.gpg.d/wand.gpg

apt-get update

apt-get install -y libbz2-dev liblzma-dev liblzo2-dev libcurl4-openssl-dev \
        liblz4-dev libncurses5-dev libzstd-dev zlib1g-dev build-essential \
        autoconf automake libtool
apt-get install -y zstd lzop xz-utils liblz4-tool

./bootstrap.sh && ./configure && make
cd test && ./do-basic-tests.sh

