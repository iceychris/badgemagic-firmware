# Minimal Dockerfile for building badgemagic-firmware

# Use an Ubuntu base image
FROM ubuntu:20.04

# Set non-interactive mode to avoid timezone prompts during package installation
ENV DEBIAN_FRONTEND=noninteractive

# Add build arguments for UID and GID
ARG USER_UID=1000
ARG USER_GID=1000

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
# NOTE: Update MRS_TOOLCHAIN_URL in .env as documented in README.md
ARG MRS_TOOLCHAIN_URL
RUN wget -O MRS_Toolchain_Linux_x64_V230.tar.xz "${MRS_TOOLCHAIN_URL}" \
    && tar -xf MRS_Toolchain_Linux_x64_V230.tar.xz \
    && mv Toolchain /home/builder/mrs_toolchain \
    && mv /home/builder/mrs_toolchain/RISC-V\ Embedded\ GCC /home/builder/mrs_toolchain/RISC-V_Embedded_GCC \
    && rm MRS_Toolchain_Linux_x64_V230.tar.xz

# Create runner user with provided UID/GID
USER root
RUN useradd -m -s /bin/bash runner -u ${USER_UID} -g ${USER_GID}
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
