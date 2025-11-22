#include "../PacketDefs.h"
#include <turbojpeg.h>
#include <GLES3/gl3.h>
#include <vector>
#include <iostream>
#include <cstring>

class Decoder {
public:
    Decoder(): tjHandle(nullptr), tex(0), pbo(0) {
        tjHandle = tjInitDecompress();
    }
    ~Decoder(){
        if(pbo) glDeleteBuffers(1, &pbo);
        if(tex) glDeleteTextures(1, &tex);
        if(tjHandle) tjDestroy(tjHandle);
    }

    // decode MJPEG frame bytes -> GL texture (uses PBO)
    bool decodeToTexture(const std::vector<uint8_t>& jpegData){
        int width, height, jpegSubsamp, jpegColorspace;
        if(tjDecompressHeader3(tjHandle, jpegData.data(), jpegData.size(), &width, &height, &jpegSubsamp, &jpegColorspace) != 0){
            std::cerr << "tjDecompressHeader3 failed\n";
            return false;
        }
        // allocate texture and PBO if needed
        if(tex==0){ glGenTextures(1, &tex); glBindTexture(GL_TEXTURE_2D, tex); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR); glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,width,height,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr); glBindTexture(GL_TEXTURE_2D,0); }
        if(pbo==0){ glGenBuffers(1, &pbo); }

        size_t bufSize = (size_t)width * (size_t)height * 4;
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo);
        glBufferData(GL_PIXEL_UNPACK_BUFFER, bufSize, nullptr, GL_STREAM_DRAW);
        void* dst = glMapBufferRange(GL_PIXEL_UNPACK_BUFFER, 0, bufSize, GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
        if(!dst){ std::cerr<<"PBO map failed\n"; glBindBuffer(GL_PIXEL_UNPACK_BUFFER,0); return false; }
        // decompress into dst as RGBA
        if(tjDecompress2(tjHandle, jpegData.data(), jpegData.size(), (unsigned char*)dst, width, 0, height, TJPF_RGBA, TJFLAG_FASTDCT) != 0){
            std::cerr << "tjDecompress2 failed\n";
            glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
            glBindBuffer(GL_PIXEL_UNPACK_BUFFER,0);
            return false;
        }
        glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
        // upload from PBO to texture
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, 0);
        glBindTexture(GL_TEXTURE_2D, 0);
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
        return true;
    }

    GLuint getTexture() const { return tex; }

private:
    tjhandle tjHandle;
    GLuint tex;
    GLuint pbo;
};
