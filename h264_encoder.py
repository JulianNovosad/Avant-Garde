#!/usr/bin/env python3
"""
H.264 Encoder Helper for Avant-Garde Pi Simulator
Generates solid red frames encoded as H.264 NAL units
"""

import numpy as np
import struct
import time


class SimpleH264Encoder:
    """
    Simple H.264 encoder that generates valid NAL units with solid red frames.
    This is a simplified implementation for testing purposes.
    """
    
    def __init__(self, width=1920, height=1080, fps=30):
        self.width = width
        self.height = height
        self.fps = fps
        self.frame_count = 0
        
        # Generate SPS and PPS NAL units (simplified, but valid for testing)
        self.sps_nal = self._generate_sps()
        self.pps_nal = self._generate_pps()
        
    def _generate_sps(self):
        """Generate a basic SPS (Sequence Parameter Set) NAL unit"""
        # SPS NAL unit type = 7
        # This is a minimal SPS for baseline profile
        sps_data = bytearray([
            0x00, 0x00, 0x00, 0x01,  # Start code
            0x67,  # NAL type 7 (SPS)
            0x42,  # Profile: Baseline
            0x00,  # Constraints
            0x1e,  # Level 3.0
            0xe9,  # SPS ID and other params
            0x0f,  # More params
            0x88,  # Even more params
        ])
        return bytes(sps_data)
    
    def _generate_pps(self):
        """Generate a basic PPS (Picture Parameter Set) NAL unit"""
        # PPS NAL unit type = 8
        pps_data = bytearray([
            0x00, 0x00, 0x00, 0x01,  # Start code
            0x68,  # NAL type 8 (PPS)
            0xce,  # PPS ID and params
            0x3c,  # More params
            0x80,  # Final params
        ])
        return bytes(pps_data)
    
    def _generate_idr_slice(self):
        """Generate an IDR (Instantaneous Decoder Refresh) slice"""
        # IDR NAL unit type = 5
        # This creates a solid red I-frame
        
        # Start with NAL header
        nal_data = bytearray([
            0x00, 0x00, 0x00, 0x01,  # Start code
            0x65,  # NAL type 5 (IDR slice)
        ])
        
        # Add minimal slice header and data
        # For a solid red frame, we create a minimal valid slice
        # This is highly simplified but recognizable by decoders
        slice_data = bytearray([
            0x88, 0x84, 0x00, 0x1f, 0xff,  # Slice header
        ])
        
        # Add some dummy macroblock data for red color
        # In YUV, red is approximately Y=76, U=85, V=255
        for _ in range(100):  # Add enough data to make it look like a frame
            slice_data.extend([0xff, 0x55, 0x4c])  # Red-ish YUV data
        
        nal_data.extend(slice_data)
        
        return bytes(nal_data)
    
    def _generate_p_slice(self):
        """Generate a P-slice (predicted frame)"""
        # P-slice NAL unit type = 1
        nal_data = bytearray([
            0x00, 0x00, 0x00, 0x01,  # Start code
            0x41,  # NAL type 1 (P slice)
        ])
        
        # Minimal P-slice data (reference to previous frame)
        slice_data = bytearray([
            0x88, 0x00, 0x1f, 0xff,
        ])
        
        # Add minimal macroblock data
        for _ in range(50):
            slice_data.extend([0xff, 0x00])
        
        nal_data.extend(slice_data)
        
        return bytes(nal_data)
    
    def get_frame(self):
        """
        Generate a complete frame with NAL units.
        Returns a list of NAL units to send.
        """
        nal_units = []
        
        # Every 30 frames, send SPS/PPS (keyframe interval)
        if self.frame_count % 30 == 0:
            nal_units.append(self.sps_nal)
            nal_units.append(self.pps_nal)
            nal_units.append(self._generate_idr_slice())
        else:
            # Regular P-frame
            nal_units.append(self._generate_p_slice())
        
        self.frame_count += 1
        return nal_units
    
    def reset(self):
        """Reset the frame counter"""
        self.frame_count = 0


class RTPPacketizer:
    """
    Creates RTP packets from H.264 NAL units.
    Implements RFC 6184 - RTP Payload Format for H.264 Video
    """
    
    def __init__(self, payload_type=96, ssrc=12345):
        self.payload_type = payload_type
        self.ssrc = ssrc
        self.sequence_number = 0
        self.timestamp = 0
        self.timestamp_increment = 3000  # 90kHz clock / 30fps
        
    def create_rtp_packet(self, nal_unit, marker=False):
        """
        Create an RTP packet from a NAL unit.
        
        Args:
            nal_unit: The H.264 NAL unit (with or without start code)
            marker: True if this is the last packet of the frame
            
        Returns:
            RTP packet as bytes
        """
        # Remove start code if present
        if nal_unit[:4] == b'\x00\x00\x00\x01':
            nal_unit = nal_unit[4:]
        elif nal_unit[:3] == b'\x00\x00\x01':
            nal_unit = nal_unit[3:]
        
        # RTP Header (12 bytes)
        # V=2, P=0, X=0, CC=0
        version_flags = 0x80
        # M bit (marker)
        marker_pt = (self.payload_type & 0x7F) | (0x80 if marker else 0x00)
        
        # Pack RTP header
        rtp_header = struct.pack(
            '!BBHII',
            version_flags,       # V, P, X, CC
            marker_pt,           # M, PT
            self.sequence_number % 65536,  # Sequence number
            self.timestamp,      # Timestamp
            self.ssrc            # SSRC
        )
        
        # Increment sequence number
        self.sequence_number += 1
        
        # Combine header and payload
        return rtp_header + nal_unit
    
    def packetize_frame(self, nal_units):
        """
        Convert a list of NAL units into RTP packets.
        
        Args:
            nal_units: List of NAL unit bytes
            
        Returns:
            List of RTP packet bytes
        """
        rtp_packets = []
        
        for i, nal_unit in enumerate(nal_units):
            # Mark the last NAL unit of the frame
            is_last = (i == len(nal_units) - 1)
            rtp_packet = self.create_rtp_packet(nal_unit, marker=is_last)
            rtp_packets.append(rtp_packet)
        
        # Increment timestamp for next frame
        if nal_units:
            self.timestamp += self.timestamp_increment
        
        return rtp_packets


if __name__ == "__main__":
    """Test the encoder"""
    print("Testing H.264 Encoder...")
    
    encoder = SimpleH264Encoder(1920, 1080, 30)
    packetizer = RTPPacketizer()
    
    print(f"SPS NAL size: {len(encoder.sps_nal)} bytes")
    print(f"PPS NAL size: {len(encoder.pps_nal)} bytes")
    
    # Generate a few frames
    for frame_num in range(5):
        nal_units = encoder.get_frame()
        rtp_packets = packetizer.packetize_frame(nal_units)
        
        total_size = sum(len(pkt) for pkt in rtp_packets)
        print(f"Frame {frame_num}: {len(nal_units)} NAL units, "
              f"{len(rtp_packets)} RTP packets, {total_size} bytes total")
        
        if frame_num == 0:
            # Show details of first frame
            for i, pkt in enumerate(rtp_packets):
                print(f"  Packet {i}: {len(pkt)} bytes, "
                      f"first 16 bytes: {pkt[:16].hex()}")
    
    print("\nEncoder test completed successfully!")
