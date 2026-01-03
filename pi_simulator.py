#!/usr/bin/env python3
import zmq
import json
import time
import socket
import threading
import sys

# Protocol Constants
VIDEO_PORT = 5000
CONTROL_PORT = 6000
PHONE_ZMQ_PORT = 2001

class PiSimulator:
    def __init__(self):
        self.running = True
        self.phone_ip = None
        self.zmq_ctx = zmq.Context()
        self.zmq_sub = None
        
        # Threads
        self.video_thread = None
        self.control_thread = None
        self.orientation_thread = None

    def start(self):
        print("[SIM] Starting Pi Simulator...")
        
        # Start Control Listener
        self.control_thread = threading.Thread(target=self.control_loop, daemon=True)
        self.control_thread.start()
        
        # Start Video Server
        self.video_thread = threading.Thread(target=self.video_loop, daemon=True)
        self.video_thread.start()
        
        try:
            while self.running:
                time.sleep(1)
        except KeyboardInterrupt:
            self.stop()
            
    def stop(self):
        self.running = False
        print("[SIM] Stopping...")

    def control_loop(self):
        print(f"[CONTROL] Listening on Port {CONTROL_PORT}")
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        s.bind(('0.0.0.0', CONTROL_PORT))
        s.listen(1)
        
        while self.running:
            try:
                conn, addr = s.accept()
                data = conn.recv(1024).decode('utf-8').strip()
                print(f"[CONTROL] Received: {data}")
                
                if data.startswith("START"):
                    parts = data.split(" ")
                    if len(parts) > 1:
                        self.phone_ip = parts[1]
                        print(f"[CONTROL] Phone IP set to {self.phone_ip}")
                        self.restart_orientation_sub()
                    else:
                        print("[CONTROL] START command missing IP")
                
                elif data == "STOP":
                    print("[CONTROL] STOP received")
                    self.stop_orientation_sub()
                
                conn.close()
            except Exception as e:
                print(f"[CONTROL] Error: {e}")

    def video_loop(self):
        print(f"[VIDEO] Video Server listening on Port {VIDEO_PORT}")
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        s.bind(('0.0.0.0', VIDEO_PORT))
        s.listen(1)
        
        while self.running:
            try:
                conn, addr = s.accept()
                print(f"[VIDEO] Client connected from {addr}")
                
                # Send dummy MPEG-TS data
                # Typically 188 byte packets. We'll just send random noise or a pattern 
                # that *might* look like a stream to a naive reader, but likely won't decode.
                # However, for connectivity testing, any stream of bytes is fine.
                # Ideally, we'd read a local test.ts file.
                try:
                    while self.running:
                        # Send 32KB chunks of zeros/noise
                        chunk = b'\x47' + b'\x00'*187 # Sync byte + null
                        conn.sendall(chunk * 100) # Send ~18KB
                        time.sleep(0.01) # ~1.8MB/s
                except BrokenPipeError:
                    print("[VIDEO] Client disconnected")
                except Exception as e:
                    print(f"[VIDEO] Stream error: {e}")
                finally:
                    conn.close()
            except Exception as e:
                print(f"[VIDEO] Server error: {e}")
                time.sleep(1)

    def restart_orientation_sub(self):
        self.stop_orientation_sub()
        if not self.phone_ip:
            return
            
        print("[ORIENT] Starting ZMQ Subscriber...")
        self.orientation_thread = threading.Thread(target=self.orientation_loop, args=(self.phone_ip,), daemon=True)
        self.orientation_thread.start()

    def stop_orientation_sub(self):
        # We can't easily kill a python thread, but we can close the socket context if we managed it carefully.
        # For this simulator, we just let the thread die or check a flag?
        # Re-creating context is easiest for cleaner restart.
        # Actually, ZMQ logic handles reconnects well. 
        pass

    def orientation_loop(self, ip):
        print(f"[ORIENT] Connecting to tcp://{ip}:{PHONE_ZMQ_PORT}")
        socket = self.zmq_ctx.socket(zmq.SUB)
        socket.connect(f"tcp://{ip}:{PHONE_ZMQ_PORT}")
        socket.subscribe("") # Subscribe to all
        
        while self.running:
            try:
                # Non-blocking check or short timeout
                if socket.poll(100): 
                    msg = socket.recv_string()
                    data = json.loads(msg)
                    print(f"[ORIENT] Yaw: {data.get('yaw',0):.1f}, Pitch: {data.get('pitch',0):.1f}, Roll: {data.get('roll',0):.1f}")
            except Exception as e:
                print(f"[ORIENT] Error: {e}")
                time.sleep(1)
        
        socket.close()

if __name__ == "__main__":
    sim = PiSimulator()
    sim.start()
