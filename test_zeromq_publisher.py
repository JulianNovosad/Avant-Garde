#!/usr/bin/env python3

import zmq
import time

def test_zeromq_publisher():
    """Test ZeroMQ publisher implementation"""
    context = zmq.Context()
    
    # Create publisher socket
    publisher = context.socket(zmq.PUB)
    
    # Bind to test ports
    publisher.bind("tcp://*:2001")
    
    print("ZeroMQ publisher started on port 2001")
    
    # Send test messages
    for i in range(5):
        message = f"Test message {i}"
        publisher.send_string(message)
        print(f"Sent: {message}")
        time.sleep(1)
    
    # Clean up
    publisher.close()
    context.term()
    
    print("Test completed successfully!")

if __name__ == "__main__":
    test_zeromq_publisher()