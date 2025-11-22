#pragma once
#include <vector>
#include <cstdint>
#include <functional>

class VideoDec {
public:
    VideoDec();
    ~VideoDec();

    // Decode JPEG bytes into an RGB buffer; returns true on success
    bool decodeJPEG(const std::vector<uint8_t>& jpeg, int &w, int &h, std::vector<uint8_t> &outRGB);

    // Upload decoded RGB to GL texture using PBOs asynchronously
    // textureId must be a valid GL texture (RGBA)
    void uploadToTexture(unsigned int textureId, int w, int h, const std::vector<uint8_t>& rgb);

private:
    struct Impl;
    Impl* p;
};
