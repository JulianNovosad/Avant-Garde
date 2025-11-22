#include "VideoDec.h"
#include <turbojpeg.h>
#include <GL/gl.h>
#include <iostream>

struct VideoDec::Impl {
    tjhandle tj = nullptr;
    GLuint pbo = 0;
    Impl(){ tj = tjInitDecompress(); }
    ~Impl(){ if (tj) tjDestroy(tj); if (pbo) glDeleteBuffers(1, &pbo); }
};

VideoDec::VideoDec(){ p = new Impl(); }
VideoDec::~VideoDec(){ delete p; }

bool VideoDec::decodeJPEG(const std::vector<uint8_t>& jpeg, int &w, int &h, std::vector<uint8_t> &outRGB){
    if (!p || !p->tj) return false;
    int jpegSubsamp, jpegColorspace;
    if (tjDecompressHeader3(p->tj, jpeg.data(), jpeg.size(), &w, &h, &jpegSubsamp, &jpegColorspace) != 0) return false;
    size_t outSize = (size_t)w * (size_t)h * 3;
    outRGB.resize(outSize);
    if (tjDecompress2(p->tj, jpeg.data(), jpeg.size(), outRGB.data(), w, 0 /*pitch*/, h, TJPF_RGB, 0) != 0) return false;
    return true;
}

void VideoDec::uploadToTexture(unsigned int textureId, int w, int h, const std::vector<uint8_t>& rgb){
    if (!p) return;
    // Create PBO if missing
    if (p->pbo==0){ glGenBuffers(1, &p->pbo); }
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, p->pbo);
    glBufferData(GL_PIXEL_UNPACK_BUFFER, rgb.size(), nullptr, GL_STREAM_DRAW);
    void* dst = glMapBufferRange(GL_PIXEL_UNPACK_BUFFER, 0, rgb.size(), GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
    if (dst){ memcpy(dst, rgb.data(), rgb.size()); glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER); }
    glBindTexture(GL_TEXTURE_2D, textureId);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
}
