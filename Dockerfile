# Get the base Ubuntu image from Docker Hub
FROM ubuntu:latest

# Update apps on the base image
RUN apt-get -y update

# Install the Clang compiler and other dependencies
RUN apt-get -y install --no-install-recommends clang cmake make \
    alsa-base alsa-utils \
    pkg-config git-core doxygen build-essential ca-certificates \
    libasound2-dev libfftw3-dev libgtest-dev \
    libavcodec-dev libavformat-dev libswresample-dev

# Copy the current folder which contains C++ source code to the Docker image under /usr/src
COPY . /usr/src/spectrum

# Specify the working directory
RUN mkdir /usr/src/spectrum/build
WORKDIR /usr/src/spectrum/build

# Compile project
RUN cmake .. && make -j 8

# Install X server
RUN apt-get -y install --no-install-recommends xvfb

RUN Xvfb :99 &
ARG DISPLAY=:99;

# Run the output program from the previous step
CMD ["./src/spectrum"]