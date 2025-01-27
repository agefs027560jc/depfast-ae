#!/bin/bash

set -e

# Server counts to iterate over
# server_counts=(3 5 9 13 17 21 25)
server_counts=(3 13 25)

# Values for -v option
duration=$1
v_values=(0 1)
mode="config/none_fpga_raft.yml"
rt="raft"

if [ "$2" == "paxos" ]; then
    v_values=(2 3)
    mode="config/occ_paxos.yml"
    rt="paxos"
elif [ "$2" == "copilot" ]; then
    v_values=(4 5)
    mode="config/none_copilot.yml"
    rt="copilot"
fi

# Iterate for each -v value
for v_value in "${v_values[@]}"; do
    # Determine the output file based on the -v value
    if [ $((v_value % 2)) -eq 0 ]; then
        output_file="broadcast_result.txt"
        output_node="broadcast_output.txt"
    else
        output_file="chaining_result.txt"
        output_node="chaining_output.txt"
    fi
    # Initialize output files with new headers
    echo "number_of_nodes, run_id, throughput, 50%_lat, 90%_lat, 99%_lat, 99.9%_lat, bw_util" > "$output_file"
    echo "" > "$output_node"
    
    # Setup and run Docker containers for each configuration
    for server_count in "${server_counts[@]}"; do

        # Perform the run 3 times for each server count and -v value
        for run_id in {1..1}; do
            echo "Setting up $server_count Docker containers for run_id $run_id..."
            ./crpc_docker_cleanup.sh
            ./crpc_docker_setup.sh $server_count

            # # Wait for containers to be ready
            # echo "Waiting for containers to initialize..."
            # sleep 30

            declare -a pids
            echo "Running $rt with server_count $server_count, -v $v_value, run_id $run_id"
            wait

            # Iterate through each server/process
            for process_id in $(seq 1 $server_count); do
                container_name="nano_${process_id}"
                config_filename="config/1c1s${server_count}r1p.yml"

                echo "Executing command on $container_name for server p$process_id"

                # Execute the command inside the Docker container
                if [ "$process_id" -eq 1 ]; then
                    # Capture the output from the first container
                    sudo docker exec -i "$container_name" \
                    build/deptran_server -f "$mode" -f "$config_filename" -f config/rw.yml -f config/concurrent_1.yml -P p$process_id -v $v_value -d $duration > "$output_node" &
                else
                    sudo docker exec -d "$container_name" \
                    build/deptran_server -f "$mode" -f "$config_filename" -f config/rw.yml -f config/concurrent_1.yml -P p$process_id -v $v_value -d $duration &
                fi
                pids+=($!)  # Store PID of the background process
            done

            # Short delay before monitoring bandwidth
            echo "Monitoring bandwidth for $duration seconds on nano_1..."
            sudo docker exec -i nano_1 bwm-ng --interfaces eth0 -t 1000 -o csv > bwmng_output.csv &

            # Allow the commands to execute for 70 seconds before stopping processes
            echo "Allowing commands to execute..."
            t=0
            d=10
            while true; do
                ln=$(grep "*************** ENDING BENCHMARK;" $output_node | wc -l)
                if [ $ln -eq 1 ] || [ $(( t++ * $d )) -gt $duration ]; then break; fi
                sleep $d
            done

            # Clean up all containers
            echo "Cleaning up containers..."
            ./crpc_docker_cleanup.sh

            # # Kill all background processes
            # echo "Stopping all processes..."
            # for pid in "${pids[@]}"; do
            #     kill -9 $pid 2>/dev/null
            # done

            wait
            echo "All processes for run_id $run_id with -v $v_value have been stopped."

            # Process output from the first node
            if [ -f "$output_node" ]; then
                throughput=$(awk -F'throughput: ' '/throughput:/ {print $2}' "$output_node" | awk '{print $1}')
                lat_50=$(awk -F'50.0% LATENCY: ' '/50.0% LATENCY:/ {print $2}' "$output_node" | awk '{gsub(";", "", $1); print $1}')
                lat_90=$(awk -F'90.0% LATENCY: ' '/90.0% LATENCY:/ {print $2}' "$output_node" | awk '{gsub(";", "", $1); print $1}')
                lat_99=$(awk -F'99.0% LATENCY: ' '/99.0% LATENCY:/ {print $2}' "$output_node" | awk '{gsub(";", "", $1); print $1}')
                lat_999=$(awk -F'99.9% LATENCY: ' '/99.9% LATENCY:/ {print $2}' "$output_node" | awk '{gsub(";", "", $1); print $1}')

                # Extract and print the bandwidth output
                bw_output=$(python3 extract_bw_utilization.py .)
                echo "bw_output: $bw_output"
                # Append the results to the respective output file
                echo "$server_count, $run_id, $throughput, $lat_50, $lat_90, $lat_99, $lat_999, $bw_output" >> $output_file
                echo "Recorded throughput for run_id $run_id: $throughput"
            fi
        done
    done
    # cp $output_file results/
done

echo "Benchmarking complete for all server configurations."
./crpc_docker_post.sh
