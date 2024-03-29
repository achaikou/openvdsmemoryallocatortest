ARG OPENVDS_IMAGE=openvds
ARG VDSSLICE_BASEIMAGE=golang:1.20-alpine3.16

FROM ${VDSSLICE_BASEIMAGE} as openvds

RUN apk --no-cache add \
    curl \
    git \
    g++ \
    gcc \
    make \
    cmake \
    curl-dev \
    boost-dev \
    libxml2-dev \
    libuv-dev \
    util-linux-dev

WORKDIR /
RUN git clone https://community.opengroup.org/osdu/platform/domain-data-mgmt-services/seismic/open-vds.git
WORKDIR /open-vds
RUN git checkout cbcd7b6163768118805dbcd080a5b0e386b82a6a

RUN cmake -S . \
    -B build \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_JAVA=OFF \
    -DBUILD_PYTHON=OFF \
    -DBUILD_EXAMPLES=OFF \
    -DBUILD_TESTS=OFF \
    -DBUILD_DOCS=OFF \
    -DDISABLE_AWS_IOMANAGER=ON \
    -DDISABLE_AZURESDKFORCPP_IOMANAGER=OFF \
    -DDISABLE_GCP_IOMANAGER=ON \
    -DDISABLE_DMS_IOMANAGER=OFF \
    -DDISABLE_STRICT_WARNINGS=OFF
RUN cmake --build build   --config Release  --target install  -j 8 --verbose


FROM $OPENVDS_IMAGE as builder
WORKDIR /
RUN git clone https://github.com/oneapi-src/oneTBB.git
WORKDIR /oneTBB
RUN git checkout v2021.10.0
RUN cmake -S . -B build -DTBB_TEST=OFF
RUN cmake --build build   --config Release  --target install  -j 8 --verbose

RUN apk --no-cache add \
    coreutils

WORKDIR /src
COPY . .
RUN chmod +x all.sh

ENV LD_LIBRARY_PATH=/open-vds/Dist/OpenVDS/lib:$LD_LIBRARY_PATH
ENV OPENVDS_AZURESDKFORCPP=1
ENTRYPOINT [ "./all.sh" ]
