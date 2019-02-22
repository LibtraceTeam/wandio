#!/bin/bash

set -x -e -o pipefail

apt-get update && apt-get install -y curl util-linux

for path in `find built-packages/amd64/ -maxdepth 1 -type d`; do
    IFS=_ read linux_dist linux_version <<< $(basename "${path}")
    for deb in `find "${path}" -maxdepth 1 -type f`; do

        ext=${deb##*.}
        pkg_filename=$(basename "${deb}")

        if [ "$ext" = "deb" ]; then
            IFS=_ read pkg_name pkg_version pkg_arch <<< $(basename -s ".deb" "${pkg_filename}")
            curl -T "${deb}" -u${BINTRAY_USERNAME}:${BINTRAY_API_KEY} \
                "https://api.bintray.com/content/wand/general/${pkg_name}/${pkg_version}/pool/${linux_version}/main/${pkg_filename};deb_distribution=${linux_version};deb_component=main;deb_architecture=${pkg_arch}"
        fi

        if [ "$ext" = "rpm" ]; then

            rev_filename=`echo ${pkg_filename} | rev`
            pkg_name=`echo ${rev_filename} | cut -d '-' -f4- | rev`
            pkg_version=`echo ${rev_filename} | cut -d '-' -f1-3 | rev | cut -d '.' -f1-3`

            curl -T "${uprpm}" -u ${BINTRAY_USERNAME}:${BINTRAY_API_KEY} \
                "https://api.bintray.com/content/wand/general-rpm/${pkg_name}/${pkg_version}/${pkg_filename}"
        fi
    done
done
