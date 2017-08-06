#!/bin/sh

if [ -f ~/.rpmmacros ] ; then
	echo "~/.rpmmacros exists: nothing to do"
	exit 0
fi

mkdir -p ~/rpmbuild/{BUILD,RPMS,SOURCES,SPECS,SRPMS}
echo '%_topdir %(echo $HOME)/rpmbuild' > ~/.rpmmacros
