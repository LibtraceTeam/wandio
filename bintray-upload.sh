#!/bin/bash

set -e -o pipefail

BINTRAY_DEB_REPO="wand/general"
BINTRAY_RPM_REPO="wand/general-rpm"
BINTRAY_LICENSE="LGPL-3"

apt-get update && apt-get install -y curl util-linux

curl --silent -fL -XGET \
    "https://api.bintray.com/content/jfrog/jfrog-cli-go/\$latest/jfrog-cli-linux-amd64/jfrog?bt_package=jfrog-cli-linux-amd64" \
    > /usr/local/bin/jfrog
chmod +x /usr/local/bin/jfrog
mkdir ~/.jfrog/
cat << EOF > ~/.jfrog/jfrog-cli.conf
{
  "artifactory": null,
  "bintray": {
    "user": "${BINTRAY_USERNAME}",
    "key": "${BINTRAY_API_KEY}"
  },
  "Version": "1"
}
EOF

for path in `find built-packages/ -maxdepth 1 -type d`; do
    IFS=_ read linux_version <<< $(basename "${path}")
    for deb in `find "${path}" -maxdepth 1 -type f`; do
        ext=${deb##*.}
        pkg_filename=$(basename "${deb}")

        if [ "$ext" = "deb" ]; then
            IFS=_ read pkg_name pkg_version pkg_arch <<< $(basename -s ".deb" "${pkg_filename}")
            jfrog bt package-create --licenses ${BINTRAY_LICENSE} --vcs-url ${CI_PROJECT_URL} ${BINTRAY_DEB_REPO}/${pkg_name} || true
            jfrog bt upload --deb ${linux_version}/main/${pkg_arch} ${deb} ${BINTRAY_DEB_REPO}/${pkg_name}/${pkg_version} pool/${linux_version}/main/${pkg_name}/
        fi

        if [ "$ext" = "rpm" ]; then
            rev_filename=`echo ${pkg_filename} | rev`

            if [[ "$1" =~ centos_* ]]; then
                # centos
                pkg_name=`echo ${rev_filename} | cut -d '-' -f4- | rev`
                pkg_version=`echo ${rev_filename} | cut -d '-' -f1-3 | rev | cut -d '.' -f1-3`
                pkg_arch=`echo ${rev_filename} | cut -d '-' -f1 | rev | cut -d '.' -f1`
                pkg_dist=centos
                pkg_rel=`echo ${rev_filename} | cut -d '.' -f2 | rev | cut -d '-' -f 1`

                if [ "$pkg_rel" = "el6" ]; then
                        releasever=6
                elif [ "$pkg_rel" = "el7" ]; then
                        releasever=7
                else
                        releasever=unknown
                fi

            else
                # fedora
                pkg_name=`echo ${rev_filename} | cut -d '-' -f3- | rev`
                pkg_version=`echo ${rev_filename} | cut -d '-' -f1-2 | rev | cut -d '.' -f1-3`
                pkg_arch=`echo ${rev_filename} | cut -d '.' -f2 | rev`
                pkg_dist=fedora
                pkg_rel=`echo ${rev_filename} | cut -d '.' -f3 | rev`
                releasever="${pkg_rel:2}"
            fi

            jfrog bt package-create --licenses ${BINTRAY_LICENSE} --vcs-url ${CI_PROJECT_URL} ${BINTRAY_RPM_REPO}/${pkg_name} || true
            jfrog bt upload ${deb} ${BINTRAY_RPM_REPO}/${pkg_name}/${pkg_version} ${pkg_dist}/${releasever}/${pkg_arch}/

        fi
    done
done

