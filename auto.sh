#!/bin/bash

# Set the NAT server's IP and port
SERVER_IP="127.0.0.1"  # Change if the server is on a different machine
PORT=55000

# Number of clients to simulate
NUM_CLIENTS=5

# Run multiple client instances
for ((i=1; i<=NUM_CLIENTS; i++))
do
    PRIVATE_IP="192.168.1.$i"  # Generate private IP dynamically
    echo "Running client $i with private IP: $PRIVATE_IP"
    ./client $SERVER_IP $PORT $PRIVATE_IP &  # Run client in background
    sleep 1  # Stagger the starts (optional)
done

# Wait for all clients to finish
wait

echo "All clients have finished."