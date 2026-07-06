# syntax=docker/dockerfile:1
# Multi-stage build (roadmap D1): stage 1 compiles the C++ engine on the same
# ubuntu:24.04 + Arrow-apt toolchain as CI; stage 2 ships only the binaries,
# runtime libraries, data, and the Python analysis environment.
#
#   docker build -t qse .
#   docker run -v "$PWD/out:/results" qse
#
# The default command runs an SMA 20/50 backtest on the bundled AAPL minute
# ticks and writes equity_curve.csv, tradelog.csv, and tearsheet.pdf to the
# mounted /results volume.

########## Stage 1: build ##########
FROM ubuntu:24.04 AS builder
ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y -V \
        ca-certificates lsb-release wget pkg-config build-essential cmake git \
        libprotobuf-dev protobuf-compiler libzmq3-dev libyaml-cpp-dev \
        libcurl4-openssl-dev \
    && wget -q "https://packages.apache.org/artifactory/arrow/$(lsb_release --id --short | tr 'A-Z' 'a-z')/apache-arrow-apt-source-latest-$(lsb_release --codename --short).deb" \
    && apt-get install -y -V ./apache-arrow-apt-source-latest-*.deb \
    && rm ./apache-arrow-apt-source-latest-*.deb \
    && apt-get update && apt-get install -y -V libarrow-dev libparquet-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY . .

RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
    && cmake --build build --target qse strategy_engine impact_sweep ab_audit -j"$(nproc)"

########## Stage 2: runtime ##########
FROM ubuntu:24.04
ENV DEBIAN_FRONTEND=noninteractive

# Runtime shared libraries (installed via the same Arrow apt source so the
# versions always match the builder stage) plus the Python analysis stack
RUN apt-get update && apt-get install -y -V ca-certificates lsb-release wget \
    && wget -q "https://packages.apache.org/artifactory/arrow/$(lsb_release --id --short | tr 'A-Z' 'a-z')/apache-arrow-apt-source-latest-$(lsb_release --codename --short).deb" \
    && apt-get install -y -V ./apache-arrow-apt-source-latest-*.deb \
    && rm ./apache-arrow-apt-source-latest-*.deb \
    && apt-get update && apt-get install -y -V \
        libarrow-dev libparquet-dev libprotobuf-dev libzmq3-dev libyaml-cpp-dev \
        libcurl4-openssl-dev \
        python3 python3-pandas python3-numpy python3-matplotlib \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY --from=builder /src/build/libqse.so /app/lib/
COPY --from=builder /src/build/strategy_engine \
                    /src/build/impact_sweep \
                    /src/build/ab_audit /app/bin/
COPY data/ /app/data/
COPY scripts/analysis/tearsheet.py /app/scripts/analysis/tearsheet.py
COPY docker/entrypoint.sh /app/entrypoint.sh

RUN chmod +x /app/entrypoint.sh
ENV LD_LIBRARY_PATH=/app/lib

VOLUME /results
CMD ["/app/entrypoint.sh"]
