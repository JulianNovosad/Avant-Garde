#!/usr/bin/env python3

import zmq
import time

def test_zeromq_subscriber():
    """Test ZeroMQ subscriber implementation"""
    context = zmq.Context()
    
    # Create subscriber socket
    subscriber = context.socket(zmq.SUB)
    
    # Connect to the publisher
    subscriber.connect("tcp://localhost:2001")
    
    # Subscribe to all messages
    subscriber.setsockopt(zmq.SUBSCRIBE, b"")
    
    print("ZeroMQ subscriber connected to port 2001")
    print("Waiting for messages...")
    
    # Receive messages
    for i in range(5):
        try:
            message = subscriber.recv_string(zmq.NOBLOCK)
            print(f"Received: {message}")
        except zmq.Again:
            print("No message received yet...")
            time.sleep(1)
    
    # Clean up
    subscriber.close()
    context.term()
    
    print("Test completed.")

if __name__ == "__main__":
    test_zeromq_subscriber()