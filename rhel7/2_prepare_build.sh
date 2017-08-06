#!/bin/sh

#
#	prepare RPM Build
#	Tested on:
#	Centos 7
#

# ## Setup

if [ ! -f ~/.rpmmacros ] ; then
	echo "you need .rpmmacros (=a RPM build environment)"
	exit -1
fi


BUILDDIR=`grep topdir ~/.rpmmacros | cut -f2 -d" "`

SOURCES=${BUILDDIR}/SOURCES

if [ ! -d "${SOURCES}" ] ; then
	echo "you need the RPM build directories"
	exit -1
fi

SPECS=${BUILDDIR}/SPECS

## Sources are in ../src ..

PREFIX=openldap-overlay-compare-

pushd ../src > /dev/null
ls | while read f ; do
	TARGET=${SOURCES}/${PREFIX}${f}
	echo "writing :${TARGET}"
	cp $f ${TARGET}
done
popd > /dev/null

## .. and in SOURCES

pushd SOURCES  > /dev/null
ls | while read f ; do
        TARGET=${SOURCES}/${PREFIX}${f}
        echo "writing :${TARGET}"
        cp $f ${TARGET}
done
popd > /dev/null


## patching the specfile:

if [ ! -f ${SPECS}/openldap.spec.backup ] ; then
	cp ${SPECS}/openldap.spec ${SPECS}/openldap.spec.backup 
fi

patch ${SPECS}/openldap.spec < SPECS/openldap.spec.patch

echo "now everything should be set up to run rpmbuild ${SPECS}/openldap.spec"
