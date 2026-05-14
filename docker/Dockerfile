# OS
FROM fedora:latest

# Basic packages
RUN dnf update -y && \
    dnf install -y git wget curl tar zip unzip gdb glibc-devel zlib-devel gcc gcc-c++ cmake make ninja-build pkgconf kernel-headers kernel-devel xz patch sed openssl openssl-devel && \
    dnf install -y nano ps ping htop netstat ss

# Packages for project (removed mysql-common mysql-devel as we use mariadb-connector-c anyway)
RUN dnf install -y alsa-lib-devel perl perl-IPC-Cmd perl-generators perl-devel nasm pkgconfig flex bison binutils elfutils-libelf-devel && \
	dnf install -y libaio-devel ncurses-devel readline-devel libuuid-devel libtirpc-devel lz4-devel libedit-devel snappy-devel bzip2-devel && \
    dnf install -y libstdc++-static brotli-devel libwebsockets-devel libuv-devel freetype-devel mariadb-connector-c mariadb-connector-c-devel

# Packages for fake screen (Should be skipped in GUI version)
RUN dnf install -y libX11-devel libXext-devel libXrandr-devel libXrender-devel  libXinerama-devel libXi-devel libXcursor-devel libXfixes-devel wayland-devel libxkbcommon-devel mesa-libEGL-devel libdecor-devel

# Clean
RUN dnf clean all && rm -rf /var/cache/dnf

# Database linking
RUN ls -la /usr/lib64/pkgconfig/ | grep maria
RUN if [ -f /usr/lib64/pkgconfig/libmariadb.pc ]; then \
        ln -sf /usr/lib64/pkgconfig/libmariadb.pc /usr/lib64/pkgconfig/mysqlclient.pc; \
    elif [ -f /usr/lib64/pkgconfig/mariadb.pc ]; then \
        ln -sf /usr/lib64/pkgconfig/mariadb.pc /usr/lib64/pkgconfig/mysqlclient.pc; \
    else \
        echo "CRITICAL ERROR: No mariadb pc file found!"; exit 1; \
    fi
RUN ln -sf /usr/lib64/libmariadb.so.3 /usr/lib64/libmysqlclient.so

# Random argument to ensure we have next steps rebuild each time we call build instead of caching them
ARG CACHE_BUST=1

# Work dir
RUN mkdir /var/repos
WORKDIR /var/repos

# Projects
RUN git clone https://github.com/Przemek2122/SquirrelCommunicator.git
RUN git clone -b communicator https://github.com/Przemek2122/Engine.git

# Engine submodules
WORKDIR /var/repos/Engine
RUN git submodule update --init --recursive

# Compile
WORKDIR /var/repos/SquirrelCommunicator/configsrv
RUN chmod +x srv-config-linux-release_2cores.sh
RUN ./srv-config-linux-release_2cores.sh

# Go into build dir
WORKDIR /var/repos/SquirrelCommunicator/buildsrv/linux-x64/bin/Release/Project/
# Run but when container starts
CMD ["./communicatorsrv"]
