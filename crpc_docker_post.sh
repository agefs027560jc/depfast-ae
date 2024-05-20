#!/bin/bash

set -e

sudo docker run --rm -it --cpuset-cpus="64-96" --name nano_0 --net nano_net \
    --cap-add=NET_ADMIN --cap-add=SYS_ADMIN \
    -v /home/users/llvan/workspace/depfast-micro/db:/db \
    -v /sys/fs/cgroup:/sys/fs/cgroup \
    -v /home/users/llvan/workspace/depfast-micro:/root/depfast llvan/nanobench

# echo "Benchmarking complete for all server configurations."
