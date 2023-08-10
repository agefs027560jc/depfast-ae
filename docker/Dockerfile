FROM ubuntu:20.04

ENV TZ=Europe/Kiev
RUN ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ > /etc/timezone

RUN echo 'root:root' |chpasswd

RUN apt-get clean && \
    rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*

# install basic libraries
RUN apt-get --assume-yes update

RUN apt-get --assume-yes install \
    git \
    wget \
    silversearcher-ag \
    python2 \
    pkg-config \
    build-essential \
    clang \
    cgroup-tools \
    libapr1-dev libaprutil1-dev \
    libboost-all-dev \
    libyaml-cpp-dev \
    libjemalloc-dev \
    python3-dev \
    python3-pip \
    python3-wheel \
    python3-setuptools \
    libjpeg-dev \
    zlib1g-dev \
    libgoogle-perftools-dev \
    python3-testresources \
    cmake \
    cmake-curses-gui \
    ninja-build \
    libsnappy-dev \
    libtool flex bison libssl-dev

RUN wget https://github.com/mikefarah/yq/releases/download/v4.24.2/yq_linux_amd64 -O /usr/bin/yq && chmod +x /usr/bin/yq

# copy .gitconfig (For the permission)
# COPY gitconfig /root/.gitconfig

WORKDIR "/root"
RUN git clone --recursive https://github.com/agefs027560jc/depfast-ae.git depfast

WORKDIR "/root/depfast"
# install dependencies
RUN git checkout cmake-sep
RUN pip3 install --upgrade pip wheel setuptools testresources
RUN pip3 install -r requirements.txt --ignore-installed
RUN pip3 install Pillow matplotlib pyyaml
RUN git clone https://github.com/agefs027560jc/didactic-octo-couscous.git sample

# workaround
#RUN ./bin/rpcgen --cpp --python src/deptran/rcc_rpc.rpc
RUN mkdir /db

# choose build env
#RUN cmake -G Ninja -S. -H. -B ./build
#RUN cmake --build ./build

#RUN python3 waf configure build

WORKDIR "/root"
RUN git clone https://github.com/apache/thrift

WORKDIR "/root/thrift"
RUN ./bootstrap.sh
RUN ./configure CPPFLAGS='-O2' CFLAGS='' CXXFLAGS=''
RUN make
RUN make install

WORKDIR "/root"
