# sudo docker build -t libdogecoin-builder .
# sudo docker run -d --name libdogecoin-container libdogecoin-builder
# sudo docker exec -it libdogecoin-container tail -f /libdogecoin/main.log

FROM ubuntu:latest

# Install necessary packages
RUN apt-get update && \
    apt-get install -y git nano htop automake libtool build-essential libevent-dev libunistring-dev pkg-config

# Clone the specific branch of libdogecoin
RUN git clone --branch 0.1.3-dev https://github.com/dogecoinfoundation/libdogecoin.git /libdogecoin

# Copy main.c and start.sh into the cloned directory
COPY main.c /libdogecoin/main.c
COPY start.sh /libdogecoin/start.sh

# Set execute permissions for start.sh
RUN chmod +x /libdogecoin/start.sh

# Build libdogecoin
WORKDIR /libdogecoin
RUN ./autogen.sh
RUN ./configure --disable-net --disable-tools
RUN make

# Compile main executable
RUN gcc main.c ./.libs/libdogecoin.a -I./include/dogecoin -L./.libs -ldogecoin -lpthread -levent -lunistring -o main

# Use JSON format for CMD to run the start.sh script
CMD ["/libdogecoin/start.sh"]