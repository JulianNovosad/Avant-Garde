#include <jni.h>
#include <string>
#include <android/log.h>
#include <media/NdkMediaCodec.h>
#include <media/NdkMediaFormat.h>
#include <android/native_window_jni.h> // For ANativeWindow_fromSurface

#include <sys/socket.h> // For socket API
#include <netinet/in.h> // For sockaddr_in
#include <arpa/inet.h>  // For inet_pton
#include <arpa/inet.h>  // For htons/ntohs
#include <unistd.h>     // For close
#include <sys/time.h>   // For struct timeval
#include <thread>       // For std::thread
#include <vector>       // For std::vector
#include <atomic>       // For std::atomic
#include <cstring>      // For memset
#include <sstream>      // For std::stringstream
#include <map>         // For std::map
#include <fstream>     // For std::ofstream

#define LOG_TAG "WiredVideoViewer-Native"

static JavaVM* g_jvm = nullptr;
static jclass g_main_activity_class = nullptr;
static jmethodID g_native_log_method_id = nullptr;

// Helper function to call Kotlin's log function from native code
void callKotlinLog(const std::string& message) {
    if (!g_jvm) {
        __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "JVM not attached when trying to log to Kotlin: %s", message.c_str());
        return;
    }

    JNIEnv* env;
    bool detach = false;
    // Get JNIEnv, if current thread is not attached, attach it
    int getEnvStat = g_jvm->GetEnv((void**)&env, JNI_VERSION_1_6);
    if (getEnvStat == JNI_EDETACHED) {
        if (g_jvm->AttachCurrentThread(&env, nullptr) != 0) {
            __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "Failed to attach current thread to JVM for logging: %s", message.c_str());
            return;
        }
        detach = true;
    } else if (getEnvStat == JNI_EVERSION) {
        __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "JNI_EVERSION error for logging: %s", message.c_str());
        return;
    }

    if (env && g_main_activity_class && g_native_log_method_id) {
        jstring jMessage = env->NewStringUTF(message.c_str());
        if (jMessage) {
            env->CallStaticVoidMethod(g_main_activity_class, g_native_log_method_id, jMessage);
            env->DeleteLocalRef(jMessage);
        } else {
            __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "Failed to create JNI string for logging: %s", message.c_str());
        }
    } else {
         __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "JNI components not initialized for logging: %s", message.c_str());
    }

    if (detach && g_jvm) {
        g_jvm->DetachCurrentThread();
    }
}

#define LOGD(...) do { \
    char buffer[256]; \
    snprintf(buffer, sizeof(buffer), __VA_ARGS__); \
    __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, "%s", buffer); \
    callKotlinLog(buffer); \
} while(0)

#define LOGE(...) do { \
    char buffer[256]; \
    snprintf(buffer, sizeof(buffer), __VA_ARGS__); \
    __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "%s", buffer); \
    callKotlinLog(std::string("ERROR: ") + buffer); \
} while(0)

// Hardcoded port configuration
// Changed to non-privileged ports (>1024) to avoid permission issues on Android
#define VIDEO_STREAM_PORT 5000
#define YAW_PHONE_TO_PI_PORT 6001
#define PITCH_PHONE_TO_PI_PORT 6002
#define ROLL_PHONE_TO_PI_PORT 6003

// Example H.264 SPS and PPS NAL units (including start codes) for a common profile/level/resolution
// These might need to be adjusted based on the actual stream from the Pi
// This specific example is for a 1280x720 30fps stream (Baseline Profile, Level 3.1)
// Source: Derived from common H.264 profiles
const unsigned char H264_SPS_NALU[] = {
    0x00, 0x00, 0x00, 0x01, // Start code
    0x67, 0x42, 0x00, 0x1f, // SPS NAL unit (Profile IDC: 0x42 Baseline, Level IDC: 0x1f Level 3.1)
    0x8d, 0x8d, 0x40, 0x28, 0x02, 0xdd, 0x80, // More SPS data for 1280x720
};
const size_t H264_SPS_NALU_SIZE = sizeof(H264_SPS_NALU);

const unsigned char H264_PPS_NALU[] = {
    0x00, 0x00, 0x00, 0x01, // Start code
    0x68, 0xce, 0x38, 0x80  // PPS NAL unit
};
const size_t H264_PPS_NALU_SIZE = sizeof(H264_PPS_NALU);

class VideoDecoder {
private:
    AMediaCodec* mediaCodec = nullptr;
    ANativeWindow* nativeWindow = nullptr;
    bool isConfigured = false;
    const char* MIME_TYPE = "video/avc"; // H.264 MIME type
    int videoWidth = 0;
    int videoHeight = 0;
    int frameRate = 30; // Default assumption
    bool waitForFormatChange = true;

public:
    VideoDecoder() {}

    ~VideoDecoder() {
        releaseDecoder();
    }

    void resetDecoder() {
        if (isConfigured) {
            LOGD("Resetting decoder...");
            releaseDecoder();
        }
    }

    bool initDecoder(JNIEnv* env, jobject surface, int surfaceWidth, int surfaceHeight) {
        LOGD("Initializing decoder with surface dimensions: %dx%d", surfaceWidth, surfaceHeight);


        // If already configured, reset first
        if (isConfigured) {
            LOGD("Decoder already configured, resetting first.");
            resetDecoder();
        }

        LOGD("Creating MediaCodec for %s (MIME type: %s)", MIME_TYPE, MIME_TYPE);
        mediaCodec = AMediaCodec_createDecoderByType(MIME_TYPE);
        if (!mediaCodec) {
            LOGE("Failed to create MediaCodec for %s", MIME_TYPE);
            return false;
        }
        LOGD("MediaCodec created successfully");

        LOGD("Getting native window from surface");
        nativeWindow = ANativeWindow_fromSurface(env, surface);
        if (!nativeWindow) {
            LOGE("Failed to get native window from surface");
            AMediaCodec_delete(mediaCodec);
            mediaCodec = nullptr;
            return false;
        }
        LOGD("Native window obtained successfully");

        // Create a minimal format for initialization
        // We'll detect actual format from the stream
        LOGD("Creating media format");
        AMediaFormat* format = AMediaFormat_new();
        AMediaFormat_setString(format, AMEDIAFORMAT_KEY_MIME, MIME_TYPE);
        // Provide known encoder dimensions for initial configuration.
        // Default to 640x480 based on Pi encoder output.
        AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_WIDTH, 640);
        AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_HEIGHT, 480);
        // Explicitly provide SPS and PPS to the MediaCodec for configuration
        AMediaFormat_setBuffer(format, AMEDIAFORMAT_KEY_CSD_0, (void*)H264_SPS_NALU, H264_SPS_NALU_SIZE);
        AMediaFormat_setBuffer(format, AMEDIAFORMAT_KEY_CSD_1, (void*)H264_PPS_NALU, H264_PPS_NALU_SIZE);

        LOGD("Media format created with explicit SPS/PPS.");

        LOGD("Configuring MediaCodec");
        media_status_t status = AMediaCodec_configure(mediaCodec, format, nativeWindow, nullptr, 0);
        if (status != AMEDIA_OK) {
            LOGE("Failed to configure MediaCodec: %d", status);
            AMediaFormat_delete(format);
            releaseDecoder();
            return false;
        }
        LOGD("MediaCodec configured successfully");
        AMediaFormat_delete(format);

        LOGD("Starting MediaCodec");
        status = AMediaCodec_start(mediaCodec);
        if (status != AMEDIA_OK) {
            LOGE("Failed to start MediaCodec: %d", status);
            releaseDecoder();
            return false;
        }
        LOGD("MediaCodec started successfully");

        isConfigured = true;
        waitForFormatChange = true;
        videoWidth = 0;
        videoHeight = 0;
        LOGD("Decoder initialized and started successfully.");
        return true;
    }

    void releaseDecoder() {
        if (mediaCodec) {
            media_status_t status = AMediaCodec_stop(mediaCodec);
            if (status != AMEDIA_OK) {
                LOGE("Failed to stop MediaCodec: %d", status);
            }
            
            status = AMediaCodec_delete(mediaCodec);
            if (status != AMEDIA_OK) {
                LOGE("Failed to delete MediaCodec: %d", status);
            }
            
            mediaCodec = nullptr;
            LOGD("MediaCodec released.");
        }
        if (nativeWindow) {
            ANativeWindow_release(nativeWindow);
            nativeWindow = nullptr;
            LOGD("Native window released.");
        }
        isConfigured = false;
        videoWidth = 0;
        videoHeight = 0;
        waitForFormatChange = true;
    }

        // Parse basic H.264 NAL unit information
    void parseSPS(const uint8_t* data, size_t size) {
        if (size < 8) return;
        
        // Skip start code (assume 4 bytes: 0x00000001)
        const uint8_t* sps = data + 4;
        size_t sps_size = size - 4;
        
        // Basic parsing to get width/height
        // This is a simplified parser - a full implementation would be more complex
        if (sps_size > 7) {
            // Extract profile and level info
            uint8_t profile = sps[1];
            uint8_t level = sps[3];
            
            LOGD("SPS NAL unit detected. Profile: %d, Level: %d", profile, level);
            // For now, we'll keep using placeholders
            // A full implementation would parse the SPS to get actual dimensions
        }
    }

        // Feed H.264 NAL units to decoder
    void decodeH264NalUnit(const uint8_t* data, size_t size, long presentationTimeUs) {
        if (!isConfigured || !mediaCodec) {
            LOGE("Decoder not configured or not started.");
            return;
        }

        // Validate input data
        if (!data || size == 0) {
            LOGE("Invalid input data for decoding.");
            return;
        }

        // Check if this is an SPS NAL unit (type 7) for format detection
        if (size >= 5 && (data[4] & 0x1F) == 7) {
            parseSPS(data, size);
        } else if (size >= 5 && (data[4] & 0x1F) == 8) { // PPS NAL unit type 8
            LOGD("PPS NAL unit detected.");
        }

        ssize_t bufIdx = AMediaCodec_dequeueInputBuffer(mediaCodec, 500000); // 500ms timeout
        if (bufIdx >= 0) {
            size_t bufSize;
            uint8_t* buffer = AMediaCodec_getInputBuffer(mediaCodec, bufIdx, &bufSize);
            if (buffer && bufSize >= size) {
                memcpy(buffer, data, size);
                media_status_t status = AMediaCodec_queueInputBuffer(mediaCodec, bufIdx, 0, size, presentationTimeUs, 0);
                if (status != AMEDIA_OK) {
                    LOGE("Failed to queue input buffer: %d", status);
                } else {
                    // Log successful input buffer queueing for NAL units
                    // Optionally log NAL type if it's not SPS/PPS already logged
                    if (!((data[4] & 0x1F) == 7 || (data[4] & 0x1F) == 8)) {
                        LOGD("Queued input buffer with NAL type %d, size %zu", (data[4] & 0x1F), size);
                    }
                }
            } else {
                LOGE("Input buffer too small or not available. Buffer size: %zu, NAL size: %zu", bufSize, size);
            }
        } else {
            LOGE("Failed to dequeue input buffer: %zd", bufIdx);
            if (bufIdx == AMEDIACODEC_INFO_TRY_AGAIN_LATER) {
                LOGD("AMEDIACODEC_INFO_TRY_AGAIN_LATER: Decoder is busy, will try again later.");
            } else {
                LOGE("Critical decoder error when dequeuing input buffer: %zd", bufIdx);
            }
        }

        // Handle output buffers
        AMediaCodecBufferInfo info;
        ssize_t status;
        while ((status = AMediaCodec_dequeueOutputBuffer(mediaCodec, &info, 0)) >= 0) {
            if (status == AMEDIACODEC_INFO_OUTPUT_FORMAT_CHANGED) {
                // Handle format change
                AMediaFormat* format = AMediaCodec_getOutputFormat(mediaCodec);
                if (format) {
                    int32_t width, height, colorFormat;
                    int32_t cropLeft = 0, cropTop = 0, cropRight = 0, cropBottom = 0; // Initialize with 0

                    AMediaFormat_getInt32(format, AMEDIAFORMAT_KEY_WIDTH, &width);
                    AMediaFormat_getInt32(format, AMEDIAFORMAT_KEY_HEIGHT, &height);
                    AMediaFormat_getInt32(format, "color-format", &colorFormat);

                    LOGD("OUTPUT_FORMAT_CHANGED");
                    LOGD("width = %d", width);
                    LOGD("height = %d", height);

                    // Check for crop information
                    bool hasCrop = false;
                    int32_t visibleWidth = width;
                    int32_t visibleHeight = height;

                    if (AMediaFormat_getInt32(format, "crop-left", &cropLeft) &&
                        AMediaFormat_getInt32(format, "crop-top", &cropTop) &&
                        AMediaFormat_getInt32(format, "crop-right", &cropRight) &&
                        AMediaFormat_getInt32(format, "crop-bottom", &cropBottom)) {
                        
                        LOGD("crop = %d,%d -> %d,%d", cropLeft, cropTop, cropRight, cropBottom);
                        visibleWidth = cropRight - cropLeft + 1;
                        visibleHeight = cropBottom - cropTop + 1;
                        hasCrop = true;
                    }

                    if (hasCrop) {
                        LOGD("Visible width = %d", visibleWidth);
                        LOGD("Visible height = %d", visibleHeight);
                    }
                    
                    int32_t stride = -1;
                    if (AMediaFormat_getInt32(format, AMEDIAFORMAT_KEY_STRIDE, &stride)) {
                        LOGD("stride = %d", stride);
                    }

                    int32_t sliceHeight = -1;
                    if (AMediaFormat_getInt32(format, AMEDIAFORMAT_KEY_SLICE_HEIGHT, &sliceHeight)) {
                        LOGD("slice-height = %d", sliceHeight);
                    }

                    // Update videoWidth and videoHeight with visible dimensions if cropping exists
                    // Or keep the original width/height if no cropping
                    videoWidth = hasCrop ? visibleWidth : width;
                    videoHeight = hasCrop ? visibleHeight : height;

                    LOGD("Color Format: %d", colorFormat); // Keep this existing log.
                    waitForFormatChange = false;
                    AMediaFormat_delete(format);
                }
            } else if (status == AMEDIACODEC_INFO_OUTPUT_BUFFERS_CHANGED) {
                // Output buffers changed
                LOGD("AMEDIACODEC_INFO_OUTPUT_BUFFERS_CHANGED: Output buffers changed.");
            } else if (status >= 0) {
                // Successfully decoded a frame
                LOGD("Successfully decoded and rendered frame. Presentation Time: %lld us", info.presentationTimeUs);
                AMediaCodec_releaseOutputBuffer(mediaCodec, status, true);
            }
        }
    }

    int getWidth() const { return videoWidth; }
    int getHeight() const { return videoHeight; }
    bool isWaitingForFormatChange() const { return waitForFormatChange; }
};

class H264NalParser {
private:
    std::vector<uint8_t> buffer_;

public:
    H264NalParser() {}

    void appendData(const uint8_t* data, size_t size) {
        buffer_.insert(buffer_.end(), data, data + size);
    }

    bool extractNextNalUnit(std::vector<uint8_t>& nalUnit) {
        nalUnit.clear();

        // Need at least 4 bytes for a start code
        if (buffer_.size() < 4) {
            return false;
        }

        // Find the first start code (3 or 4 bytes)
        size_t startIdx = 0;
        size_t startCodeLength = 0;

        // Look for 4-byte start code (0x00000001)
        for (size_t i = 0; i <= buffer_.size() - 4; ++i) {
            if (buffer_[i] == 0 && buffer_[i+1] == 0 && buffer_[i+2] == 0 && buffer_[i+3] == 1) {
                startIdx = i + 4; // Start of NAL unit data
                startCodeLength = 4;
                break;
            }
        }

        // If not found, look for 3-byte start code (0x000001)
        if (startCodeLength == 0) {
            for (size_t i = 0; i <= buffer_.size() - 3; ++i) {
                if (buffer_[i] == 0 && buffer_[i+1] == 0 && buffer_[i+2] == 1) {
                    startIdx = i + 3; // Start of NAL unit data
                    startCodeLength = 3;
                    break;
                }
            }
        }

        // If we still haven't found a start code, we can't extract a NAL unit
        if (startCodeLength == 0) {
            return false;
        }

        // Find the next start code to determine the end of this NAL unit
        size_t endIdx = buffer_.size(); // Default to end of buffer
        size_t nextStartCodeLength = 0;
        for (size_t i = startIdx; i <= buffer_.size() - 3; ++i) {
            if (buffer_[i] == 0 && buffer_[i+1] == 0 && buffer_[i+2] == 0 && buffer_[i+3] == 1) {
                // 4-byte start code
                endIdx = i;
                nextStartCodeLength = 4;
                break;
            } else if (buffer_[i] == 0 && buffer_[i+1] == 0 && buffer_[i+2] == 1) {
                // 3-byte start code
                endIdx = i;
                nextStartCodeLength = 3;
                break;
            }
        }

        // Extract the NAL unit (including the start code)
        size_t nalStart = startIdx - startCodeLength;
        nalUnit.assign(buffer_.begin() + nalStart, buffer_.begin() + endIdx);

        // Remove processed data from buffer (keep the last start code for the next NAL unit)
        if (endIdx < buffer_.size()) {
            // If we found a next start code, remove everything up to that start code
            buffer_.erase(buffer_.begin(), buffer_.begin() + endIdx);
        } else {
            // If we didn't find a next start code, keep the last start code for the next packet
            buffer_.erase(buffer_.begin(), buffer_.begin() + endIdx - startCodeLength);
        }

        return !nalUnit.empty();
    }

    void clear() {
        buffer_.clear();
    }

    size_t getBufferSize() const {
        return buffer_.size();
    }
};

class RtpReceiver {
private:
    std::atomic<bool> running_;
    std::thread receiverThread_;
    VideoDecoder* decoder_;
    int sock_fd = -1;
    std::vector<uint8_t> fu_nal_buffer_; // Buffer for FU-A reassembly
    H264NalParser nalParser;
    long frameCounter = 0;
    std::chrono::steady_clock::time_point lastFrameTime;
    bool firstPacketRecorded = false;
    std::chrono::steady_clock::time_point firstPacketTimestamp;
    std::ofstream outputFile;
    int packetsToCapture = 100; // Limit capture to 100 packets
    int packetsCaptured = 0;

    void run(int port) {
        LOGD("RTP receiver thread started on port %d", port);

        // Create UDP socket for RTP
        sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
        if (sock_fd == -1) {
            LOGE("Failed to create UDP socket, errno: %d", errno);
            running_ = false;
            return;
        }
        LOGD("UDP socket created successfully (fd: %d)", sock_fd); // Added log

        // Open file for raw UDP capture
        std::string filename = "/sdcard/Download/raw_udp_stream.bin"; // Standard external storage path
        outputFile.open(filename, std::ios::out | std::ios::binary);
        if (outputFile.is_open()) {
            LOGD("Opened file for raw UDP capture: %s", filename.c_str());
        } else {
            LOGE("Failed to open file for raw UDP capture: %s, errno: %d (%s)", filename.c_str(), errno, strerror(errno));
        }


        // Allow socket reuse to avoid "Address already in use" errors
        int reuse = 1;
        if (setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
            LOGE("Failed to set SO_REUSEADDR, errno: %d", errno);
        }

        // Bind to the specified port
        struct sockaddr_in serv_addr;
        memset(&serv_addr, 0, sizeof(serv_addr));
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_addr.s_addr = INADDR_ANY;
        serv_addr.sin_port = htons(port);

        if (bind(sock_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
            LOGE("Bind failed for port %d, errno: %d (%s)", port, errno, strerror(errno));
            // If we can't bind to the privileged port, try a non-privileged port
            if (errno == EACCES && port < 1024) {
                LOGD("Trying non-privileged port 11001 instead");
                serv_addr.sin_port = htons(11001);
                int fallback_port_actual = 11001;
                if (bind(sock_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
                    LOGE("Bind also failed for fallback port 11001, errno: %d (%s)", errno, strerror(errno));
                    close(sock_fd);
                    sock_fd = -1;
                    running_ = false;
                    return;
                }
                LOGD("RTP receiver successfully bound to fallback port %d (UDP)", fallback_port_actual);
            } else {
                close(sock_fd);
                sock_fd = -1;
                running_ = false;
                return;
            }
        } else {
            LOGD("RTP receiver successfully bound to port %d (UDP)", port);
        }
        // Set socket timeout to avoid blocking indefinitely
        struct timeval tv;
        tv.tv_sec = 5;  // 5 seconds timeout
        tv.tv_usec = 0;
        if (setsockopt(sock_fd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv) < 0) {
            LOGE("Failed to set socket timeout, errno: %d", errno);
        }

        // Buffer for receiving RTP packets
        std::vector<uint8_t> recvBuffer(65536); // Max UDP packet size
        std::vector<uint8_t> nalUnit;
        lastFrameTime = std::chrono::steady_clock::now();
        
        while (running_) {
            ssize_t bytes_read = recv(sock_fd, reinterpret_cast<char*>(recvBuffer.data()), recvBuffer.size(), 0);
            if (bytes_read > 0) {
                if (!firstPacketRecorded) {
                    firstPacketTimestamp = std::chrono::steady_clock::now();
                    LOGD("First UDP packet received. Timestamp: %lld ms",
                         std::chrono::duration_cast<std::chrono::milliseconds>(firstPacketTimestamp.time_since_epoch()).count());
                    firstPacketRecorded = true;
                }
                // Update last frame time
                lastFrameTime = std::chrono::steady_clock::now();
                
                // Log received packets - make it more noticeable
                LOGD("Received UDP packet, size: %zd bytes.", bytes_read);
                
                // Save raw packet to file
                if (outputFile.is_open() && packetsCaptured < packetsToCapture) {
                    outputFile.write(reinterpret_cast<const char*>(recvBuffer.data()), bytes_read);
                    packetsCaptured++;
                    if (packetsCaptured == packetsToCapture) {
                        LOGD("Captured %d raw UDP packets to %s. Closing file.", packetsToCapture, filename.c_str());
                        outputFile.close();
                    }
                }
                
                const uint8_t* payload = recvBuffer.data();
                size_t payload_size = static_cast<size_t>(bytes_read);

                // Assuming RTP header is 12 bytes. If so, skip it.
                if (payload_size >= 12 && (payload[0] & 0xC0) == 0x80) { // Check for RTP Version 2
                    LOGD("Detected RTP packet. Skipping 12-byte RTP header.");
                    payload += 12;
                    payload_size -= 12;
                } else {
                    LOGD("Received non-RTP UDP packet or too small. Size: %zu", payload_size);
                    continue; // Skip processing if not a recognized RTP-like packet
                }

                // Ensure there's enough payload for at least a NAL header
                if (payload_size < 1) {
                    LOGE("RTP payload too small to contain NAL header. Size: %zu", payload_size);
                    continue;
                }

                uint8_t nal_type = payload[0] & 0x1F; // NAL unit type

                if (nal_type >= 1 && nal_type <= 23) { // Case A: Single NAL unit
                    std::vector<uint8_t> completeNal;
                    completeNal.push_back(0x00);
                    completeNal.push_back(0x00);
                    completeNal.push_back(0x00);
                    completeNal.push_back(0x01);
                    completeNal.insert(completeNal.end(), payload, payload + payload_size);

                    if (decoder_) {
                        long presentationTimeUs = (frameCounter * 1000000) / 30; // Simple frame rate for PTS
                        decoder_->decodeH264NalUnit(completeNal.data(), completeNal.size(), presentationTimeUs);
                        if (nal_type == 5) { // IDR frame
                            LOGD("Queued NAL type 5 (IDR)");
                        }
                        frameCounter++;
                    }
                } else if (nal_type == 28) { // Case B: FU-A
                    if (payload_size < 2) {
                        LOGE("FU-A payload too small to contain FU header. Size: %zu", payload_size);
                        continue;
                    }

                    uint8_t fu_header = payload[1];
                    uint8_t S = (fu_header >> 7) & 0x01; // Start bit
                    uint8_t E = (fu_header >> 6) & 0x01; // End bit
                    uint8_t orig_nal_type = fu_header & 0x1F; // Original NAL type

                    if (S == 1) { // Start of FU-A
                        LOGD("FU-A start detected (original NAL type: %d)", orig_nal_type);
                        fu_nal_buffer_.clear();
                        fu_nal_buffer_.push_back(0x00);
                        fu_nal_buffer_.push_back(0x00);
                        fu_nal_buffer_.push_back(0x00);
                        fu_nal_buffer_.push_back(0x01);
                        
                        // Reconstruct NAL header: (nal_ref_idc << 5) | orig_nal_type
                        // nal_ref_idc is in payload[0] bits 5-6 (payload[0] & 0xE0)
                        uint8_t reconstructed_nal_header = (payload[0] & 0xE0) | orig_nal_type;
                        fu_nal_buffer_.push_back(reconstructed_nal_header);
                        
                        // Append fragment payload (skip FU indicator and FU header)
                        fu_nal_buffer_.insert(fu_nal_buffer_.end(), payload + 2, payload + payload_size);
                    } else { // Middle or End of FU-A
                        if (fu_nal_buffer_.empty()) {
                            LOGE("FU-A middle/end fragment received without start fragment. Dropping.");
                            continue;
                        }
                        // Append fragment payload (skip FU indicator and FU header)
                        fu_nal_buffer_.insert(fu_nal_buffer_.end(), payload + 2, payload + payload_size);
                    }

                    if (E == 1) { // End of FU-A
                        LOGD("FU-A end detected, NAL size = %zu", fu_nal_buffer_.size());
                        if (decoder_) {
                            long presentationTimeUs = (frameCounter * 1000000) / 30; // Simple frame rate for PTS
                            decoder_->decodeH264NalUnit(fu_nal_buffer_.data(), fu_nal_buffer_.size(), presentationTimeUs);
                             if (orig_nal_type == 5) { // IDR frame
                                LOGD("Queued NAL type 5 (IDR)");
                            }
                            frameCounter++;
                        }
                        fu_nal_buffer_.clear();
                    }
                } else {
                    LOGD("Unsupported NAL type: %d. Dropping packet.", nal_type);
                }
            } else if (bytes_read == 0) {
                // This typically means the peer has performed an orderly shutdown, rare for UDP
                LOGD("recv returned 0 bytes (orderly shutdown for UDP, unexpected).");
                continue;
            } else {
                // Check if it's a timeout or other error
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    // Check if we haven't received data for too long
                    auto now = std::chrono::steady_clock::now();
                    auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - lastFrameTime);
                    if (duration.count() > 10) {
                        LOGE("No data received for more than 10 seconds. Receiver active but idle."); // Modified log
                    }
                    continue; // Just try again
                } else if (errno != EINTR) {
                    LOGE("recv failed with errno: %d (%s)", errno, strerror(errno)); // Added strerror
                    break;
                }
            }
        }

        // Ensure file is closed if loop exits prematurely
        if (outputFile.is_open()) {
            outputFile.close();
            LOGD("Raw UDP capture file closed during receiver stop.");
        }

        // Close socket if it's still open
        if (sock_fd != -1) {
            close(sock_fd);
            sock_fd = -1;
        }
        
        LOGD("RTP receiver thread stopped.");
    }

public:
    RtpReceiver(VideoDecoder* decoder) : decoder_(decoder), running_(false) {}

    ~RtpReceiver() {
        stop();
    }

    void start(int port) {
        if (!running_) {
            running_ = true;
            receiverThread_ = std::thread(&RtpReceiver::run, this, port);
        }
    }

    void stop() {
        if (running_) {
            LOGD("Stopping RTP receiver...");
            running_ = false;
            if (receiverThread_.joinable()) {
                receiverThread_.join();
            }
            if (sock_fd != -1) {
                close(sock_fd);
                sock_fd = -1;
            }
            // Clear the NAL parser buffer
            nalParser.clear();
            frameCounter = 0;
            LOGD("RTP receiver stopped cleanly.");
        }
    }
};

class RtspReceiver {
private:
    std::atomic<bool> running_;
    std::thread receiverThread_;
    VideoDecoder* decoder_;
    int rtsp_sock_fd = -1;
    std::vector<uint8_t> fu_nal_buffer_; // Buffer for FU-A reassembly
    H264NalParser nalParser;
    long frameCounter = 0;
    std::chrono::steady_clock::time_point lastFrameTime;
    std::string server_ip_;
    int server_port_;
    std::string session_id_;
    
    // RTSP methods
    
    void run(const char* ip, int port) {
        LOGD("RTSP receiver thread started for %s:%d", ip, port);
        
        server_ip_ = std::string(ip);
        server_port_ = port;
        
        // Create TCP socket for RTSP control
        rtsp_sock_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (rtsp_sock_fd == -1) {
            LOGE("Failed to create RTSP socket, errno: %d", errno);
            running_ = false;
            return;
        }
        
        // Connect to RTSP server
        struct sockaddr_in serv_addr;
        memset(&serv_addr, 0, sizeof(serv_addr));
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_port = htons(port);
        
        if (inet_pton(AF_INET, ip, &serv_addr.sin_addr) <= 0) {
            LOGE("Invalid address/ Address not supported: %s", ip);
            close(rtsp_sock_fd);
            rtsp_sock_fd = -1;
            running_ = false;
            return;
        }
        
        if (connect(rtsp_sock_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
            LOGE("Connection to RTSP server failed, errno: %d", errno);
            close(rtsp_sock_fd);
            rtsp_sock_fd = -1;
            running_ = false;
            return;
        }
        
        LOGD("Connected to RTSP server at %s:%d", ip, port);
        
        // Send OPTIONS request
        std::string optionsReq = "OPTIONS rtsp://" + server_ip_ + ":" + std::to_string(server_port_) + "/stream RTSP/1.0\r\n";
        optionsReq += "CSeq: 1\r\n";
        optionsReq += "User-Agent: WiredVideoViewer/1.0\r\n";
        optionsReq += "\r\n";
        
        std::string response;
        if (!sendRtspRequest(optionsReq, response)) {
            LOGE("Failed to send OPTIONS request");
            close(rtsp_sock_fd);
            rtsp_sock_fd = -1;
            running_ = false;
            return;
        }
        
        // Send DESCRIBE request
        std::string describeReq = "DESCRIBE rtsp://" + server_ip_ + ":" + std::to_string(server_port_) + "/stream RTSP/1.0\r\n";
        describeReq += "CSeq: 2\r\n";
        describeReq += "User-Agent: WiredVideoViewer/1.0\r\n";
        describeReq += "Accept: application/sdp\r\n";
        describeReq += "\r\n";
        
        if (!sendRtspRequest(describeReq, response)) {
            LOGE("Failed to send DESCRIBE request");
            close(rtsp_sock_fd);
            rtsp_sock_fd = -1;
            running_ = false;
            return;
        }
        
        // Parse SDP to get stream information
        // For simplicity, we'll assume H.264 stream
        
        // Send SETUP request
        std::string setupReq = "SETUP rtsp://" + server_ip_ + ":" + std::to_string(server_port_) + "/stream/streamid=0 RTSP/1.0\r\n";
        setupReq += "CSeq: 3\r\n";
        setupReq += "User-Agent: WiredVideoViewer/1.0\r\n";
        setupReq += "Transport: RTP/AVP/TCP;unicast\r\n";  // Using TCP transport for simplicity
        setupReq += "\r\n";
        
        if (!sendRtspRequest(setupReq, response)) {
            LOGE("Failed to send SETUP request");
            close(rtsp_sock_fd);
            rtsp_sock_fd = -1;
            running_ = false;
            return;
        }
        
        // Extract session ID from response
        std::map<std::string, std::string> headers;
        std::string body;
        if (parseRtspResponse(response, headers, body)) {
            auto it = headers.find("Session");
            if (it != headers.end()) {
                session_id_ = it->second;
                // Remove any parameters after semicolon
                size_t semicolonPos = session_id_.find(';');
                if (semicolonPos != std::string::npos) {
                    session_id_ = session_id_.substr(0, semicolonPos);
                }
            }
        }
        
        // Send PLAY request
        std::string playReq = "PLAY rtsp://" + server_ip_ + ":" + std::to_string(server_port_) + "/stream RTSP/1.0\r\n";
        playReq += "CSeq: 4\r\n";
        playReq += "User-Agent: WiredVideoViewer/1.0\r\n";
        playReq += "Session: " + session_id_ + "\r\n";
        playReq += "\r\n";
        
        if (!sendRtspRequest(playReq, response)) {
            LOGE("Failed to send PLAY request");
            close(rtsp_sock_fd);
            rtsp_sock_fd = -1;
            running_ = false;
            return;
        }
        
        LOGD("RTSP stream is now playing");
        
        // Buffer for receiving RTSP/RTP packets
        std::vector<uint8_t> recvBuffer(65536); // Max packet size
        std::vector<uint8_t> nalUnit;
        lastFrameTime = std::chrono::steady_clock::now();
        
                    // Main receiving loop
                while (running_) {
                    ssize_t bytes_read = recv(rtsp_sock_fd, reinterpret_cast<char*>(recvBuffer.data()), recvBuffer.size(), 0);
                    if (bytes_read > 0) {
                        // Update last frame time
                        lastFrameTime = std::chrono::steady_clock::now();
                        
                        // Log first few bytes for debugging
                        if (frameCounter < 10) {
                            LOGD("Received RTSP/RTP packet, size: %zd, first 8 bytes: %02x %02x %02x %02x %02x %02x %02x %02x",
                                 bytes_read,
                                 recvBuffer[0], recvBuffer[1], recvBuffer[2], recvBuffer[3],
                                 recvBuffer[4], recvBuffer[5], recvBuffer[6], recvBuffer[7]);
                        }
                        
                        // Process RTSP/RTP packet (skip RTP header, typically 12 bytes)
                        const uint8_t* payload = recvBuffer.data();
                        size_t payload_size = bytes_read;
        
                        // Assuming RTP header is 12 bytes. If so, skip it.
                        if (payload_size >= 12 && (payload[0] & 0xC0) == 0x80) { // Check for RTP Version 2
                            LOGD("Detected RTP packet. Skipping 12-byte RTP header.");
                            payload += 12;
                            payload_size -= 12;
                        } else {
                            LOGD("Received non-RTP UDP packet or too small. Size: %zu", payload_size);
                            continue; // Skip processing if not a recognized RTP-like packet
                        }
        
                        // Ensure there's enough payload for at least a NAL header
                        if (payload_size < 1) {
                            LOGE("RTP payload too small to contain NAL header. Size: %zu", payload_size);
                            continue;
                        }
                        
                        uint8_t nal_type = payload[0] & 0x1F; // NAL unit type
        
                        if (nal_type >= 1 && nal_type <= 23) { // Case A: Single NAL unit
                            std::vector<uint8_t> completeNal;
                            completeNal.push_back(0x00);
                            completeNal.push_back(0x00);
                            completeNal.push_back(0x00);
                            completeNal.push_back(0x01);
                            completeNal.insert(completeNal.end(), payload, payload + payload_size);
        
                            if (decoder_) {
                                long presentationTimeUs = (frameCounter * 1000000) / 30; // Simple frame rate for PTS
                                decoder_->decodeH264NalUnit(completeNal.data(), completeNal.size(), presentationTimeUs);
                                if (nal_type == 5) { // IDR frame
                                    LOGD("Queued NAL type 5 (IDR)");
                                }
                                frameCounter++;
                            }
                        } else if (nal_type == 28) { // Case B: FU-A
                            if (payload_size < 2) {
                                LOGE("FU-A payload too small to contain FU header. Size: %zu", payload_size);
                                continue;
                            }
        
                            uint8_t fu_header = payload[1];
                            uint8_t S = (fu_header >> 7) & 0x01; // Start bit
                            uint8_t E = (fu_header >> 6) & 0x01; // End bit
                            uint8_t orig_nal_type = fu_header & 0x1F; // Original NAL type
        
                            if (S == 1) { // Start of FU-A
                                LOGD("FU-A start detected (original NAL type: %d)", orig_nal_type);
                                fu_nal_buffer_.clear();
                                fu_nal_buffer_.push_back(0x00);
                                fu_nal_buffer_.push_back(0x00);
                                fu_nal_buffer_.push_back(0x00);
                                fu_nal_buffer_.push_back(0x01);
                                
                                // Reconstruct NAL header: (nal_ref_idc << 5) | orig_nal_type
                                // nal_ref_idc is in payload[0] bits 5-6 (payload[0] & 0xE0)
                                uint8_t reconstructed_nal_header = (payload[0] & 0xE0) | orig_nal_type;
                                fu_nal_buffer_.push_back(reconstructed_nal_header);
                                
                                // Append fragment payload (skip FU indicator and FU header)
                                fu_nal_buffer_.insert(fu_nal_buffer_.end(), payload + 2, payload + payload_size);
                            } else { // Middle or End of FU-A
                                if (fu_nal_buffer_.empty()) {
                                    LOGE("FU-A middle/end fragment received without start fragment. Dropping.");
                                    continue;
                                }
                                // Append fragment payload (skip FU indicator and FU header)
                                fu_nal_buffer_.insert(fu_nal_buffer_.end(), payload + 2, payload + payload_size);
                            }
        
                            if (E == 1) { // End of FU-A
                                LOGD("FU-A end detected, NAL size = %zu", fu_nal_buffer_.size());
                                if (decoder_) {
                                    long presentationTimeUs = (frameCounter * 1000000) / 30; // Simple frame rate for PTS
                                    decoder_->decodeH264NalUnit(fu_nal_buffer_.data(), fu_nal_buffer_.size(), presentationTimeUs);
                                     if (orig_nal_type == 5) { // IDR frame
                                        LOGD("Queued NAL type 5 (IDR)");
                                    }
                                    frameCounter++;
                                }
                                fu_nal_buffer_.clear();
                            }
                        } else {
                            LOGD("Unsupported NAL type: %d. Dropping packet.", nal_type);
                        }
                    } else if (bytes_read == 0) {
                        LOGD("Connection closed by server.");
                        break;
                    } else {
                        // Check for errors
                        if (errno != EINTR) {
                            LOGE("recv failed with errno: %d", errno);
                            break;
                        }
                    }
                }        
        // Close socket if it's still open
        if (rtsp_sock_fd != -1) {
            close(rtsp_sock_fd);
            rtsp_sock_fd = -1;
        }
        
        LOGD("RTSP receiver thread stopped.");
    }

public:
    RtspReceiver(VideoDecoder* decoder) : decoder_(decoder), running_(false) {}

    ~RtspReceiver() {
        stop();
    }

    void start(const char* ip, int port) {
        if (!running_) {
            running_ = true;
            receiverThread_ = std::thread(&RtspReceiver::run, this, ip, port);
        }
    }

    void stop() {
        if (running_) {
            LOGD("Stopping RTSP receiver...");
            running_ = false;
            if (receiverThread_.joinable()) {
                receiverThread_.join();
            }
            if (rtsp_sock_fd != -1) {
                close(rtsp_sock_fd);
                rtsp_sock_fd = -1;
            }
            // Clear the NAL parser buffer
            nalParser.clear();
            frameCounter = 0;
            LOGD("RTSP receiver stopped cleanly.");
        }
    }
    
    bool sendRtspRequest(const std::string& request, std::string& response) {
        LOGD("Sending RTSP request:\n%s", request.c_str());
        
        if (rtsp_sock_fd == -1) {
            LOGE("RTSP socket not initialized");
            return false;
        }
        
        // Send request
        ssize_t sent = send(rtsp_sock_fd, request.c_str(), request.length(), 0);
        if (sent < 0) {
            LOGE("Failed to send RTSP request, errno: %d", errno);
            return false;
        }
        
        // Receive response
        response.clear();
        char buffer[4096];
        ssize_t bytes_received;
        bool header_complete = false;
        int content_length = 0;
        
        while (!header_complete || (content_length > 0 && response.length() < content_length)) {
            bytes_received = recv(rtsp_sock_fd, buffer, sizeof(buffer) - 1, 0);
            if (bytes_received > 0) {
                buffer[bytes_received] = '\0';
                response += std::string(buffer, bytes_received);
                
                // Check if we've received the complete header
                if (!header_complete) {
                    size_t header_end = response.find("\r\n\r\n");
                    if (header_end != std::string::npos) {
                        header_complete = true;
                        
                        // Parse Content-Length header
                        size_t content_length_pos = response.find("Content-Length:");
                        if (content_length_pos != std::string::npos) {
                            size_t start = response.find_first_of("0123456789", content_length_pos);
                            if (start != std::string::npos) {
                                size_t end = response.find_first_not_of("0123456789", start);
                                std::string length_str = response.substr(start, end != std::string::npos ? end - start : std::string::npos);
                                content_length = std::stoi(length_str);
                            }
                        }
                    }
                }
            } else if (bytes_received == 0) {
                LOGD("Connection closed by server");
                break;
            } else {
                if (errno != EINTR) {
                    LOGE("Failed to receive RTSP response, errno: %d", errno);
                    return false;
                }
            }
        }
        
        LOGD("Received RTSP response:\n%s", response.c_str());
        return true;
    }
    
    bool parseRtspResponse(const std::string& response, std::map<std::string, std::string>& headers, std::string& body) {
        std::istringstream iss(response);
        std::string line;
        
        // Parse status line
        if (!std::getline(iss, line)) {
            return false;
        }
        
        // Parse headers
        while (std::getline(iss, line) && line != "\r") {
            // Remove \r at the end if present
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            
            size_t colon_pos = line.find(':');
            if (colon_pos != std::string::npos) {
                std::string key = line.substr(0, colon_pos);
                std::string value = line.substr(colon_pos + 1);
                
                // Trim whitespace
                key.erase(0, key.find_first_not_of(" \t"));
                key.erase(key.find_last_not_of(" \t") + 1);
                value.erase(0, value.find_first_not_of(" \t"));
                value.erase(value.find_last_not_of(" \t") + 1);
                
                headers[key] = value;
            }
        }
        
        // The rest is the body
        body.clear();
        std::string remaining;
        while (std::getline(iss, line)) {
            body += line + "\n";
        }
        
        return true;
    }
    
    bool establishRtspConnection() {
        // For now, we'll just return true as the connection is established in the run method
        // In a more sophisticated implementation, we might do additional checks here
        return true;
    }
    
    void closeRtspConnection() {
        if (rtsp_sock_fd != -1) {
            close(rtsp_sock_fd);
            rtsp_sock_fd = -1;
        }
    }
};

// Global instances
static VideoDecoder* decoder = nullptr;
static RtpReceiver* rtpReceiver = nullptr;
static RtspReceiver* rtspReceiver = nullptr;

extern "C" JNIEXPORT jboolean JNICALL
Java_com_wiredvideoviewer_MainActivity_initDecoder(JNIEnv* env, jclass clazz, jobject surface, jint width, jint height) {
    // Cache JVM and MainActivity class/method IDs
    env->GetJavaVM(&g_jvm);
    g_main_activity_class = (jclass)env->NewGlobalRef(env->FindClass("com/wiredvideoviewer/MainActivity"));
    g_native_log_method_id = env->GetStaticMethodID(g_main_activity_class, "nativeLog", "(Ljava/lang/String;)V");

    if (!g_main_activity_class || !g_native_log_method_id) {
        __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "Failed to cache MainActivity class or nativeLog method ID!");
        if (g_main_activity_class) env->DeleteGlobalRef(g_main_activity_class);
        g_main_activity_class = nullptr;
        return JNI_FALSE;
    }
    
    if (!decoder) {
        decoder = new VideoDecoder();
    }
    // If decoder exists, it will handle re-initialization internally
    return decoder->initDecoder(env, surface, width, height);
}

// This function is now empty as we are not using g_activity_obj anymore
extern "C" JNIEXPORT void JNICALL
Java_com_wiredvideoviewer_MainActivity_setActivityReference(JNIEnv* env, jclass clazz, jobject activity) {
}

extern "C" JNIEXPORT void JNICALL
Java_com_wiredvideoviewer_MainActivity_decodeNalUnit(JNIEnv* env, jclass clazz, jbyteArray nalUnit, jlong presentationTimeUs) {
    if (!decoder) {
        LOGE("Decoder not initialized when decodeNalUnit was called.");
        return;
    }

    jbyte* bufferPtr = env->GetByteArrayElements(nalUnit, nullptr);
    jsize length = env->GetArrayLength(nalUnit);

    decoder->decodeH264NalUnit(reinterpret_cast<const uint8_t*>(bufferPtr), static_cast<size_t>(length), presentationTimeUs);

    env->ReleaseByteArrayElements(nalUnit, bufferPtr, JNI_ABORT);
}

// Helper function to call Java updateOrientationData method
void updateOrientationDataInJava(float yaw, float pitch, float roll) {
    if (!g_jvm) return; 
    
    JNIEnv* env;
    if (g_jvm->GetEnv((void**)&env, JNI_VERSION_1_6) != JNI_OK) {
        return;
    }
    
    // TODO: Implement proper Java method calling
    // This would require caching the class and method IDs
}

extern "C" JNIEXPORT void JNICALL
Java_com_wiredvideoviewer_MainActivity_updateOrientationData(JNIEnv *env, jobject thiz, jfloat yaw, jfloat pitch, jfloat roll) {
    // This function is called from Java to update orientation data
    // We don't need to do anything here as the Java side handles the UI update
}

extern "C" JNIEXPORT void JNICALL
Java_com_wiredvideoviewer_MainActivity_releaseDecoder(JNIEnv* env, jclass clazz) {
    if (rtpReceiver) {
        rtpReceiver->stop();
        delete rtpReceiver;
        rtpReceiver = nullptr;
        LOGD("RTP receiver instance deleted.");
    }
    
    if (rtspReceiver) {
        rtspReceiver->stop();
        delete rtspReceiver;
        rtspReceiver = nullptr;
        LOGD("RTSP receiver instance deleted.");
    }
    
    if (decoder) {
        decoder->releaseDecoder();
        delete decoder;
        decoder = nullptr;
        LOGD("Decoder instance deleted.");
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_wiredvideoviewer_MainActivity_startRtpReceiver(JNIEnv* env, jclass clazz, jint port) {
    if (!decoder) {
        LOGE("Decoder not initialized when startRtpReceiver was called.");
        return;
    }
    
    if (rtpReceiver) {
        rtpReceiver->stop();
        delete rtpReceiver;
    }
    
    rtpReceiver = new RtpReceiver(decoder);
    
    // Use the port parameter passed from Java instead of hardcoded port
    rtpReceiver->start(port);
}

extern "C" JNIEXPORT void JNICALL
Java_com_wiredvideoviewer_MainActivity_stopRtpReceiver(JNIEnv* env, jclass clazz) {
    if (rtpReceiver) {
        rtpReceiver->stop();
        delete rtpReceiver;
        rtpReceiver = nullptr;
        LOGD("RTP receiver instance deleted.");
    }
}

// New JNI functions for RTSP support
extern "C" JNIEXPORT void JNICALL
Java_com_wiredvideoviewer_MainActivity_startRtspReceiver(JNIEnv* env, jclass clazz, jstring ip, jint port) {
    if (!decoder) {
        LOGE("Decoder not initialized when startRtspReceiver was called.");
        return;
    }
    
    if (rtspReceiver) {
        rtspReceiver->stop();
        delete rtspReceiver;
    }
    
    rtspReceiver = new RtspReceiver(decoder);
    
    // Convert jstring to const char*
    const char* ipStr = env->GetStringUTFChars(ip, nullptr);
    
    rtspReceiver->start(ipStr, port);
    
    // Release the string
    env->ReleaseStringUTFChars(ip, ipStr);
}

extern "C" JNIEXPORT void JNICALL
Java_com_wiredvideoviewer_MainActivity_stopRtspReceiver(JNIEnv* env, jclass clazz) {
    if (rtspReceiver) {
        rtspReceiver->stop();
        delete rtspReceiver;
        rtspReceiver = nullptr;
        LOGD("RTSP receiver instance deleted.");
    }
}