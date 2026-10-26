# Phase 13.2 — headless match server image. Multi-stage: the builder needs
# the full toolchain plus SFML's own dependency headers (net/ carries
# sf::Packet, so even the server-side libraries link SFML's network
# component; Ubuntu's libsfml is 2.x, hence the FetchContent source build).

FROM ubuntu:24.04 AS builder
RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential cmake git libsqlite3-dev \
        libfreetype-dev libx11-dev libxrandr-dev libxcursor-dev \
        libxi-dev libudev-dev libopenal-dev libvorbis-dev libflac-dev \
    && rm -rf /var/lib/apt/lists/*
WORKDIR /src
COPY . .
RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
    && cmake --build build -j "$(nproc)" --target chess-server

FROM ubuntu:24.04
RUN apt-get update && apt-get install -y --no-install-recommends \
        ca-certificates libsqlite3-0 \
    && rm -rf /var/lib/apt/lists/* \
    && mkdir -p /usr/local/lib
COPY --from=builder /src/build/_deps/sfml-build/lib/ /usr/local/lib/
COPY --from=builder /src/build/server/chess-server /usr/local/bin/chess-server
RUN ldconfig
WORKDIR /data
EXPOSE 5555
ENTRYPOINT ["/usr/local/bin/chess-server", "--port", "5555"]
