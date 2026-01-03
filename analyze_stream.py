
import sys

def analyze_udp_stream(filepath):
    print(f"Analyzing file: {filepath}")
    try:
        with open(filepath, 'rb') as f:
            data = f.read()
    except FileNotFoundError:
        print(f"Error: File not found at {filepath}")
        return

    if not data:
        print("File is empty.")
        return

    print(f"Read {len(data)} bytes from file.")

    # MPEG-TS packet size is typically 188 bytes
    ts_packet_size = 188
    
    potential_ts_packets = 0
    non_ts_starts = 0
    current_offset = 0

    print("\n--- Initial Packet Scan (first 100 packets) ---")
    
    # Analyze first 100 potential packets or until end of data
    while current_offset < len(data) and potential_ts_packets < 100:
        if current_offset + ts_packet_size <= len(data):
            packet_candidate = data[current_offset : current_offset + ts_packet_size]
            
            if packet_candidate[0] == 0x47: # Check for sync byte
                # print(f"Offset {current_offset}: Potential TS packet found (starts with 0x47)")
                potential_ts_packets += 1
            else:
                # print(f"Offset {current_offset}: Does NOT start with 0x47. First byte: {packet_candidate[0]:02x}")
                non_ts_starts += 1
            current_offset += ts_packet_size # Assume fixed size packets for this scan
        else:
            print(f"Reached end of data at offset {current_offset}. Remaining bytes: {len(data) - current_offset}")
            break
            
    print(f"\nScan results (assuming {ts_packet_size}-byte chunks):")
    print(f"  {potential_ts_packets} chunks started with 0x47 (MPEG-TS sync byte).")
    print(f"  {non_ts_starts} chunks did NOT start with 0x47.")

    if potential_ts_packets > non_ts_starts and potential_ts_packets > 0:
        print(f"\nStrong indication that the stream contains MPEG-TS packets of {ts_packet_size} bytes.")
        print("The RTP payload likely contains raw MPEG-TS packets.")
    elif potential_ts_packets == 0 and non_ts_starts == 0:
        print("\nNo full 188-byte chunks found for analysis.")
    else:
        print("\nThe stream does NOT appear to consistently contain MPEG-TS packets of 188 bytes based on sync byte check.")
        print("Further analysis (e.g., RTP header parsing, H.264 NAL unit signatures) might be needed.")

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python analyze_stream.py <raw_udp_stream_file>")
        sys.exit(1)
    
    analyze_udp_stream(sys.argv[1])
