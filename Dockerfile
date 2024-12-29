# Minimal Dockerfile for building badgemagic-firmware

# Use an Ubuntu base image
FROM ubuntu:20.04

# Set non-interactive mode to avoid timezone prompts during package installation
ENV DEBIAN_FRONTEND=noninteractive

# Install required dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    wget \
    tar \
    git \
    libusb-1.0-0-dev \
    && rm -rf /var/lib/apt/lists/*

# Create a non-root user and switch to it
RUN useradd -m -s /bin/bash builder
USER builder

# Set the working directory
WORKDIR /home/builder/workspace

# Download and install the MounRiver Toolchain
RUN wget http://file-oss.mounriver.com/tools/MRS_Toolchain_Linux_x64_V1.91.tar.xz \
    && tar -xf MRS_Toolchain_Linux_x64_V1.91.tar.xz \
    && mv MRS_Toolchain_Linux_x64_V1.91 /home/builder/mrs_toolchain \
    && rm MRS_Toolchain_Linux_x64_V1.91.tar.xz

# Run as uid 502 and gid 20
USER root
RUN useradd -m -s /bin/bash runner -u 502 -g 20
USER runner

# Set the toolchain location in PATH
ENV PATH="/home/builder/mrs_toolchain/RISC-V_Embedded_GCC/bin:$PATH"

# Copy the source code into the container
COPY . /home/builder/workspace

# Set the PREFIX environment variable for the toolchain
ENV PREFIX=/home/builder/mrs_toolchain/RISC-V_Embedded_GCC/bin/riscv-none-embed-

RUN git config --global --add safe.directory /home/builder/workspace

ENV USBC_VERSION=1
ENV DEBUG=1

# Default command to build the firmware
CMD ["make"]
