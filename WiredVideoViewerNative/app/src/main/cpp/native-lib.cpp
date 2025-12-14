#include <jni.h>
#include <string>
#include <android/log.h>
#include <media/NdkMediaCodec.h>
#include <media/NdkMediaFormat.h>
#include <android/native_window_jni.h> // For ANativeWindow_fromSurface

#include <sys/socket.h> // For socket API
#include <netinet/in.h> // For sockaddr_in
#include <arpa/inet.h>  // For inet_pton
#include <unistd.h>     // For close
#include <thread>       // For std::thread
#include <vector>       // For std::vector
#include <atomic>       // For std::atomic
#include <cstring>      // For memset

#define LOG_TAG "WiredVideoViewer-Native"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// Hardcoded port configuration
#define VIDEO_STREAM_PORT 1001
#define BOUNDING_BOXES_PORT 1002
#define RETICLE_COORDINATE_PORT 1003
#define STATUS_INFO_PORT 1004
#define YAW_PHONE_TO_PI_PORT 2001
#define PITCH_PHONE_TO_PI_PORT 2002
#define ROLL_PHONE_TO_PI_PORT 2003

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

    bool initDecoder(JNIEnv* env, jobject surface) {
        LOGD("Initializing decoder...");

        // If already configured, reset first
        if (isConfigured) {
            LOGD("Decoder already configured, resetting first.");
            resetDecoder();
        }

        LOGD("Creating MediaCodec for %s", MIME_TYPE);
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
        LOGD("Media format created");

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
            
            // For now, we'll keep using placeholders
            // A full implementation would parse the SPS to get actual dimensions
            LOGD("SPS parsed - Profile: %d, Level: %d", profile, level);
        }
    }

        // Feed H.264 NAL units to decoder
    void decodeH264NalUnit(const uint8_t* data, size_t size, long presentationTimeUs) {
        if (!isConfigured || !mediaCodec) {
            LOGE("Decoder not configured or not started.");
            return;
        }

        // Check if this is an SPS NAL unit (type 7) for format detection
        if (size >= 5 && (data[4] & 0x1F) == 7) {
            parseSPS(data, size);
        }

        ssize_t bufIdx = AMediaCodec_dequeueInputBuffer(mediaCodec, 500000); // 500ms timeout
        if (bufIdx >= 0) {
            size_t bufSize;
            uint8_t* buffer = AMediaCodec_getInputBuffer(mediaCodec, bufIdx, &bufSize);
            if (buffer && bufSize >= size) {
                memcpy(buffer, data, size);
                AMediaCodec_queueInputBuffer(mediaCodec, bufIdx, 0, size, presentationTimeUs, 0);
            } else {
                LOGE("Input buffer too small or not available.");
            }
        } else {
            LOGD("No input buffer available, try again. Result: %zd", bufIdx);
        }

        AMediaCodecBufferInfo bufferInfo;
        ssize_t outBufIdx = AMediaCodec_dequeueOutputBuffer(mediaCodec, &bufferInfo, 0); // Non-blocking
        while (outBufIdx >= 0) {
            // Use the presentation time from the buffer info if available, otherwise use our calculated time
            long renderTimeUs = (bufferInfo.presentationTimeUs > 0) ? bufferInfo.presentationTimeUs : presentationTimeUs;
            
            AMediaCodec_releaseOutputBufferAtTime(mediaCodec, outBufIdx, renderTimeUs);
            outBufIdx = AMediaCodec_dequeueOutputBuffer(mediaCodec, &bufferInfo, 0);
        }
        
        if (outBufIdx == AMEDIACODEC_INFO_OUTPUT_BUFFERS_CHANGED) {
            LOGD("Output buffers changed (should not happen with AHardwareBuffer)");
        } else if (outBufIdx == AMEDIACODEC_INFO_OUTPUT_FORMAT_CHANGED) {
            AMediaFormat* format = AMediaCodec_getOutputFormat(mediaCodec);
            LOGD("Output format changed: %s", AMediaFormat_toString(format));
            
            // Extract video dimensions from the format
            int width, height;
            if (AMediaFormat_getInt32(format, AMEDIAFORMAT_KEY_WIDTH, &width) &&
                AMediaFormat_getInt32(format, AMEDIAFORMAT_KEY_HEIGHT, &height)) {
                videoWidth = width;
                videoHeight = height;
                LOGD("Video dimensions detected: %dx%d", width, height);
                
                // Update surface aspect ratio if needed
                if (nativeWindow && width > 0 && height > 0) {
                    ANativeWindow_setBuffersGeometry(nativeWindow, width, height, WINDOW_FORMAT_RGBA_8888);
                }
            }
            
            AMediaFormat_delete(format);
            waitForFormatChange = false;
        } else if (outBufIdx == AMEDIACODEC_INFO_TRY_AGAIN_LATER) {
            // No output buffer available, wait for next frame
        } else {
            LOGE("Unexpected dequeueOutputBuffer result: %zd", outBufIdx);
        }
    }

    int getWidth() const { return videoWidth; }
    int getHeight() const { return videoHeight; }
    bool isWaitingForFormatChange() const { return waitForFormatChange; }
};

class H264NalParser {
private:
    std::vector<uint8_t> buffer_;
    size_t bufferSize_;

public:
    H264NalParser(size_t initialBufferSize = 1024 * 1024) : bufferSize_(initialBufferSize) {
        buffer_.reserve(initialBufferSize);
    }

    void appendData(const uint8_t* data, size_t size) {
        buffer_.insert(buffer_.end(), data, data + size);
    }

    bool extractNextNalUnit(std::vector<uint8_t>& nalUnit) {
        if (buffer_.size() < 4) {
            return false; // Not enough data
        }

        // Look for start codes (0x00000001 or 0x000001)
        size_t startIdx = 0;
        bool foundStart = false;

        // Find the first start code
        for (size_t i = 0; i <= buffer_.size() - 3; ++i) {
            if (buffer_[i] == 0 && buffer_[i+1] == 0 && buffer_[i+2] == 0 && buffer_[i+3] == 1) {
                // 4-byte start code
                startIdx = i + 4;
                foundStart = true;
                break;
            } else if (buffer_[i] == 0 && buffer_[i+1] == 0 && buffer_[i+2] == 1) {
                // 3-byte start code
                startIdx = i + 3;
                foundStart = true;
                break;
            }
        }

        if (!foundStart) {
            // No start code found, maybe we need more data
            // If buffer is getting too large, discard some data
            if (buffer_.size() > bufferSize_ / 2) {
                buffer_.erase(buffer_.begin(), buffer_.begin() + buffer_.size() / 2);
            }
            return false;
        }

        // Find the next start code to determine the end of this NAL unit
        size_t endIdx = buffer_.size(); // Default to end of buffer
        for (size_t i = startIdx; i <= buffer_.size() - 3; ++i) {
            if (buffer_[i] == 0 && buffer_[i+1] == 0 && buffer_[i+2] == 0 && buffer_[i+3] == 1) {
                // 4-byte start code
                endIdx = i;
                break;
            } else if (buffer_[i] == 0 && buffer_[i+1] == 0 && buffer_[i+2] == 1) {
                // 3-byte start code
                endIdx = i;
                break;
            }
        }

        // Extract the NAL unit (including the start code)
        size_t nalStart = startIdx - (buffer_[startIdx-1] == 0 ? 
                         (buffer_[startIdx-2] == 0 ? 4 : 3) : 0);
        nalUnit.assign(buffer_.begin() + nalStart, buffer_.begin() + endIdx);

        // Remove processed data from buffer
        buffer_.erase(buffer_.begin(), buffer_.begin() + endIdx);

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
    H264NalParser nalParser;
    long frameCounter = 0;
    std::chrono::steady_clock::time_point lastFrameTime;

    void run(int port) {
        LOGD("RTP receiver thread started on port %d", port);

        // Create UDP socket for RTP
        sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
        if (sock_fd == -1) {
            LOGE("Failed to create UDP socket");
            running_ = false;
            return;
        }

        // Bind to the specified port
        struct sockaddr_in serv_addr;
        memset(&serv_addr, 0, sizeof(serv_addr));
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_addr.s_addr = INADDR_ANY;
        serv_addr.sin_port = htons(port);

        if (bind(sock_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
            LOGE("Bind failed for port %d, errno: %d", port, errno);
            close(sock_fd);
            sock_fd = -1;
            running_ = false;
            return;
        }

        LOGD("RTP receiver bound to port %d", port);

        // Set socket timeout to avoid blocking indefinitely
        struct timeval tv;
        tv.tv_sec = 5;  // 5 seconds timeout
        tv.tv_usec = 0;
        setsockopt(sock_fd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);

        // Buffer for receiving RTP packets
        std::vector<uint8_t> recvBuffer(65536); // Max UDP packet size
        std::vector<uint8_t> nalUnit;
        lastFrameTime = std::chrono::steady_clock::now();
        
        while (running_) {
            ssize_t bytes_read = recv(sock_fd, reinterpret_cast<char*>(recvBuffer.data()), recvBuffer.size(), 0);
            if (bytes_read > 0) {
                // Update last frame time
                lastFrameTime = std::chrono::steady_clock::now();
                
                // Process RTP packet (skip RTP header, typically 12 bytes)
                if (bytes_read > 12) {
                    const uint8_t* payload = recvBuffer.data() + 12;
                    size_t payload_size = bytes_read - 12;
                    
                    // Append received data to parser
                    nalParser.appendData(payload, payload_size);

                    // Extract and process NAL units
                    while (nalParser.extractNextNalUnit(nalUnit)) {
                        if (decoder_) {
                            // Calculate presentation timestamp
                            long presentationTimeUs = (frameCounter * 1000000) / 30;
                            decoder_->decodeH264NalUnit(nalUnit.data(), nalUnit.size(), presentationTimeUs);
                            frameCounter++;
                        }
                    }
                }
            } else if (bytes_read == 0) {
                LOGD("No data received.");
                continue;
            } else {
                // Check if it's a timeout or other error
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    // Check if we haven't received data for too long
                    auto now = std::chrono::steady_clock::now();
                    auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - lastFrameTime);
                    if (duration.count() > 10) {
                        LOGE("No data received for more than 10 seconds");
                    }
                    continue; // Just try again
                } else if (errno != EINTR) {
                    LOGE("recv failed with errno: %d", errno);
                    break;
                }
            }
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


// Global instances
static VideoDecoder* decoder = nullptr;
static RtpReceiver* rtpReceiver = nullptr;
static JavaVM* g_jvm = nullptr;
static jobject g_activity_obj = nullptr;


extern "C" JNIEXPORT jboolean JNICALL
Java_com_wiredvideoviewer_MainActivity_initDecoder(JNIEnv* env, jclass clazz, jobject surface) {
    // Save reference to the JVM
    env->GetJavaVM(&g_jvm);
    
    if (!decoder) {
        decoder = new VideoDecoder();
    }
    // If decoder exists, it will handle re-initialization internally
    return decoder->initDecoder(env, surface);
}

// Function to set the activity object reference
extern "C" JNIEXPORT void JNICALL
Java_com_wiredvideoviewer_MainActivity_setActivityReference(JNIEnv* env, jclass clazz, jobject activity) {
    if (g_activity_obj != nullptr) {
        env->DeleteGlobalRef(g_activity_obj);
    }
    g_activity_obj = env->NewGlobalRef(activity);
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
        LOGE("Cannot start RTP receiver: Decoder not initialized.");
        return;
    }
    if (rtpReceiver) {
        LOGD("RTP receiver already exists, stopping and re-starting.");
        rtpReceiver->stop();
        delete rtpReceiver;
        rtpReceiver = nullptr;
    }
    
    rtpReceiver = new RtpReceiver(decoder);
    // Use the hardcoded video stream port
    rtpReceiver->start(VIDEO_STREAM_PORT);
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