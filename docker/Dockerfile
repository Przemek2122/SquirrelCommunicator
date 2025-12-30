# OS
FROM fedora:latest

# Packages for project
RUN dnf update -y && \
    dnf install -y git wget curl tar zip unzip nano glibc-devel zlib-devel gcc gcc-c++ cmake make ninja-build pkgconf kernel-headers kernel-devel xz patch sed openssl openssl-devel && \
	dnf install -y alsa-lib-devel perl perl-IPC-Cmd perl-generators perl-devel nasm pkgconfig flex bison binutils elfutils-libelf-devel && \
	dnf install -y mysql-common mysql-devel libaio-devel ncurses-devel readline-devel libuuid-devel libtirpc-devel lz4-devel libedit-devel snappy-devel bzip2-devel && \
    dnf install -y libstdc++-static brotli-devel libwebsockets-devel libuv-devel freetype-devel
	
# Packages for fake screen
RUN dnf install -y libX11-devel libXext-devel libXrandr-devel libXrender-devel  libXinerama-devel libXi-devel libXcursor-devel libXfixes-devel wayland-devel libxkbcommon-devel mesa-libEGL-devel libdecor-devel
		
# Clean
RUN dnf clean all && rm -rf /var/cache/dnf
	
# Work dir
WORKDIR /var/repos

# Projects
RUN git clone https://github.com/Przemek2122/SquirrelCommunicator.git
RUN git clone https://github.com/Przemek2122/Engine.git

# Engine submodules
RUN cd Engine && \
    git submodule update --init --recursive
	