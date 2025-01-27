#!/bin/bash

# Variables
network_name="nano_net"
subnet="172.20.0.0/16"
base_ip="172.20.0."
image_name="llvan/nanobench"
start_ip=10
number_of_nodes=5 # Number of nodes passed as the first script argument

# # Create Docker network if it doesn't already exist
# if ! docker network ls | grep -q "$network_name"; then
#     sudo docker network create --subnet $subnet $network_name
# fi

# Function to stop and remove all containers
cleanup_containers() {
    wait
    echo "Stopping and removing existing containers..."
    sudo docker ps -a --filter "name=nano_" --format "{{.Names}}" | xargs -r sudo docker stop
    sudo docker ps -a --filter "name=nano_" --format "{{.Names}}" | xargs -r sudo docker rm
    echo "Cleanup complete."
}

# Stop and remove all existing containers before setting up new ones
cleanup_containers


