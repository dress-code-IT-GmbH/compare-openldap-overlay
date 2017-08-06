#!/bin/sh
yum -y install rpm-build redhat-rpm-config gcc make
yum -y install yum-utils

yum-builddep -y openldap

yumdownloader --source openldap
rpm -i openldap*
