FROM centos:centos7

RUN yum -y install epel-release

RUN yum -y install git wget \
    python-devel.x86_64 python-pip \
    python3.x86_64 python3-devel.x86_64 python3-pip \
    clang cmake3 libxml2-devel

RUN yum groupinstall -y "Development Tools" "Development Libraries"

RUN pip install --upgrade pip
