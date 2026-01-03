#!/bin/bash

# Simple test script to verify ZeroMQ implementation
echo "Testing ZeroMQ implementation..."

# Check if we have the ZeroMQ jar file
if [ -f "/home/julian/Avant-Garde/libs/jeromq-0.5.2.jar" ]; then
    echo "ZeroMQ jar file found."
else
    echo "ZeroMQ jar file not found."
fi

# Check if we have Java
if command -v java &> /dev/null; then
    echo "Java is available."
    java -version
else
    echo "Java is not available."
fi

echo "Test completed."