VERSION="1.0"
DISTRO="saucy"
PACKAGE_VERSION="4"

tar -czf pycrosswalk_$VERSION~$DISTRO$PACKAGE_VERSION.orig.tar.gz --exclude-vcs --exclude=debian --exclude=out --transform="s:^pycrosswalk:pycrosswalk-$VERSION~$DISTRO$PACKAGE_VERSION:S" pycrosswalk/
