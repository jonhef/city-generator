FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive
ENV CC=clang
ENV CXX=clang++

RUN apt-get update && \
    apt-get install -y --no-install-recommends \
        build-essential \
        clang \
        cmake \
        python3 \
        python3-pip && \
    rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY . .

# Configure with CMake (out-of-source), build, and run the test suite.
RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
RUN cmake --build build --config Release
RUN ctest --test-dir build --output-on-failure

CMD ["/bin/bash"]
