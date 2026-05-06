FROM ubuntu:24.04
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update
RUN apt-get install --reinstall make
RUN apt-get update
RUN apt-get install -y g++ vim git
RUN apt install -y git vim wget emacs pciutils
# Install gem5 packages
RUN apt install -y build-essential scons python3-dev git pre-commit zlib1g zlib1g-dev \
    libprotobuf-dev protobuf-compiler libprotoc-dev libgoogle-perftools-dev \
    libboost-all-dev  libhdf5-serial-dev python3-pydot python3-venv python3-tk mypy \
    m4 libcapstone-dev libpng-dev libelf-dev pkg-config wget cmake doxygen clang-format
RUN apt-get update
# Install dieharder
RUN apt-get install -y dieharder
# Install TestU01
RUN apt install -y testu01-bin libtestu01-dev
RUN apt-get update
# Install iverilog
RUN apt install -y iverilog yosys gtkwave
