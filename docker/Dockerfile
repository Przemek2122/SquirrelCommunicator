# ── Stage 1: Build ─────────────────────────────────────────────────────────────
FROM fedora:latest AS builder

# System packages for compilation
RUN dnf update -y && \
    dnf install -y \
        git gcc gcc-c++ cmake make ninja-build \
        openssl-devel zlib-devel \
        mariadb-connector-c-devel \
        libcurl-devel \
    && dnf clean all && rm -rf /var/cache/dnf

# Database linking (SOCI expects mysqlclient)
RUN if [ -f /usr/lib64/pkgconfig/libmariadb.pc ]; then \
        ln -sf /usr/lib64/pkgconfig/libmariadb.pc /usr/lib64/pkgconfig/mysqlclient.pc; \
    elif [ -f /usr/lib64/pkgconfig/mariadb.pc ]; then \
        ln -sf /usr/lib64/pkgconfig/mariadb.pc /usr/lib64/pkgconfig/mysqlclient.pc; \
    else \
        echo "CRITICAL ERROR: No mariadb pc file found!"; exit 1; \
    fi
RUN ln -sf /usr/lib64/libmariadb.so.3 /usr/lib64/libmysqlclient.so

# Clone project (standalone — no Engine dependency)
WORKDIR /build
ARG CACHE_BUST=1
RUN git clone --recurse-submodules https://github.com/Przemek2122/SquirrelCommunicator.git

# Update to latest (CACHE_BUST forces re-pull)
WORKDIR /build/SquirrelCommunicator
RUN git pull && git submodule update --init --recursive

# Build release (RelWithDebInfo keeps debug symbols for Sentry symbolication)
WORKDIR /build/SquirrelCommunicator/ProjectServer
RUN cmake -B build -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=RelWithDebInfo
RUN cmake --build build --target communicatorsrv --parallel $(nproc)

# ── Stage 2: Runtime ───────────────────────────────────────────────────────────
FROM fedora:latest

# Runtime dependencies only (no compiler)
RUN dnf install -y \
        openssl zlib \
        mariadb-connector-c \
        boost-date-time \
        libidn2 libnghttp2 libpsl \
        curl \
    && dnf clean all && rm -rf /var/cache/dnf

# Database linking for runtime
RUN ln -sf /usr/lib64/libmariadb.so.3 /usr/lib64/libmysqlclient.so

# Copy the whole build output dir (binary, crashpad_handler if Sentry is
# enabled, and Assets). Using the directory copy keeps this robust whether or
# not crashpad_handler was produced.
WORKDIR /app
COPY --from=builder /build/SquirrelCommunicator/ProjectServer/build/bin/Release/Project/ ./

# Explicitly guarantee the config INI comes from the repository checkout, not
# from any snapshot embedded in the build output. This mirrors the behavior of
# Dockerfile.release: both images take ProjectServer/Assets/Config/BackendSettings.ini
# straight from the cloned repository, regardless of how the binary was produced.
COPY --from=builder /build/SquirrelCommunicator/ProjectServer/Assets/Config/BackendSettings.ini ./Assets/Config/BackendSettings.ini

CMD ["./communicatorsrv"]
