#!/bin/sh
sudo yum -y install rpm-build redhat-rpm-config gcc make
sudo yum -y install yum-utils

sudo yum-builddep -y openldap

yumdownloader --source openldap
rpm -i openldap*
