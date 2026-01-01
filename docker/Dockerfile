# OS
FROM fedora:latest

# Basic packages
RUN dnf update -y && \
    dnf install -y git wget curl tar zip unzip gdb glibc-devel zlib-devel gcc gcc-c++ cmake make ninja-build pkgconf kernel-headers kernel-devel xz patch sed openssl openssl-devel && \
    dnf install -y nano ps ping htop

# Packages for project
RUN dnf install -y alsa-lib-devel perl perl-IPC-Cmd perl-generators perl-devel nasm pkgconfig flex bison binutils elfutils-libelf-devel && \
	dnf install -y mysql-common mysql-devel libaio-devel ncurses-devel readline-devel libuuid-devel libtirpc-devel lz4-devel libedit-devel snappy-devel bzip2-devel && \
    dnf install -y libstdc++-static brotli-devel libwebsockets-devel libuv-devel freetype-devel mariadb-connector-c

# Packages for fake screen (Should be skipped in GUI version)
RUN dnf install -y libX11-devel libXext-devel libXrandr-devel libXrender-devel  libXinerama-devel libXi-devel libXcursor-devel libXfixes-devel wayland-devel libxkbcommon-devel mesa-libEGL-devel libdecor-devel

# Clean
RUN dnf clean all && rm -rf /var/cache/dnf

# Link mariadb as mysql
RUN ln -s /usr/lib64/libmariadb.so.3 /usr/lib64/libmysqlclient.so
	
# Work dir
WORKDIR /var/repos

# Projects
RUN git clone https://github.com/Przemek2122/SquirrelCommunicator.git
RUN git clone https://github.com/Przemek2122/Engine.git

# Engine submodules
RUN cd Engine && \
    git submodule update --init --recursive
	