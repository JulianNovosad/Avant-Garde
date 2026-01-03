# Avant-Garde Auto-Discovery Testing Guide

## Quick Start - Virtual Testing

### 1. Test Pi Simulator Standalone

```bash
cd /home/julian/Avant-Garde
python3 pi_simulator.py
```

You should see:
```
[BEACON] Starting beacon broadcaster on port 9999
[ORIENTATION] Starting orientation receiver on port 5555
[VIDEO] Starting video sender on port 5556
```

### 2. Test with Android Device

**Prerequisites:**
- Android device with USB debugging enabled
- Both computer and Android on same WiFi network
- USB cable

**Steps:**

1. **Build and Install APK:**
```bash
cd /home/julian/Avant-Garde/WiredVideoViewerNative
./gradlew installDebug
```

2. **Start Pi Simulator:**
```bash
cd /home/julian/Avant-Garde
python3 pi_simulator.py
```

3. **Launch App on Android:**
- Open "Wired Video Viewer" app
- Wait 1-2 seconds for device discovery
- Device spinner should show: `"<IP> (Avant-Garde Pi)"`
- Tap "Start Stream"

4. **Monitor Logs:**
```bash
# In another terminal
adb logcat -s MainActivity WiredVideoViewer-Native
```

**Expected Results:**
- ✅ Pi appears in device list within 2 seconds
- ✅ Video shows solid red frame
- ✅ Orientation data displays yaw/pitch/roll
- ✅ Pi simulator logs orientation values

### 3. Test Orientation Transmission

**Move the Android device and watch Python console:**
```
[ORIENTATION] Received 30 messages | Latest: Yaw=15.2° Pitch=-5.8° Roll=2.3°
```

The values should change as you rotate the device.

### 4. Test Reconnection

1. Stop Pi simulator (Ctrl+C)
2. Wait 10 seconds (Android should detect disconnection)
3. Restart Pi simulator
4. Android should auto-reconnect within 5-10 seconds

## Troubleshooting

### Problem: No devices discovered

**Check:**
```bash
# Ensure simulator is running
ps aux | grep pi_simulator

# Check if beacon is being sent
sudo tcpdump -i any -n udp port 9999

# Verify Android can reach simulator IP
adb shell ping <simulator-ip>
```

### Problem: Video not displaying

**Check:**
```bash
# Verify video packets being sent
sudo tcpdump -i any -n udp port 5556 | head -20

# Check Android receiver port
adb logcat | grep "Starting RTP receiver"
```

### Problem: Orientation data not received

**Check:**
```bash
# Verify Android is sending
adb logcat | grep "orientation"

# Check ZeroMQ connection
netstat -an | grep 5555
```

## Command Reference

### Build Commands
```bash
# Clean build
cd WiredVideoViewerNative && ./gradlew clean assembleDebug

# Install to device
./gradlew installDebug

# View build logs
./gradlew assembleDebug --info
```

### Testing Commands
```bash
# Run Pi simulator with custom ports
python3 pi_simulator.py --orientation-port 6000 --video-port 6001 --beacon-port 10000

# Test H.264 encoder alone
python3 h264_encoder.py

# Monitor all network traffic
sudo tcpdump -i any -n 'udp or tcp'
```

### Debugging Commands
```bash
# Android logs
adb logcat -s MainActivity:D WiredVideoViewer-Native:D

# Clear Android log buffer
adb logcat -c

# List connected Android devices
adb devices

# Check Android network interfaces
adb shell ip addr
```

## Expected Network Traffic

When running successfully, you should see:

**UDP Port 9999 (Beacons):**
- 1 packet/second from Pi simulator
- ~150 bytes each

**TCP Port 5555 (Orientation):**
- ~10 packets/second from Android
- ~100 bytes each (JSON)

**UDP Port 5556 (Video):**
- ~30 packets/second from Pi simulator
- Varying sizes (NAL units)

## Next Steps

After successful virtual testing:

1. Deploy to actual Raspberry Pi hardware
2. Replace simulated video with real camera feed
3. Add connection status UI to Android app
4. Performance tuning and optimization

## Support

For issues, check:
- `walkthrough.md` for detailed documentation
- `implementation_plan.md` for architecture details
- Android logcat output for errors
- Python console for simulator status
