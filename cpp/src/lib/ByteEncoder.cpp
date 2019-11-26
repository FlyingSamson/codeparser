
#include "ByteEncoder.h"

#include <cassert>

size_t ByteEncoder::size(int val) {
    
    assert(val >= 0);
    
    if (val <= 0x7f) {
        
        return 1;
        
    } else if (val <= 0x7ff) {
        
        return 2;
        
    } else if (val <= 0xffff) {
        
        return 3;
        
    } else {
        
        return 4;
    }
}

void ByteEncoder::encodeBytes(std::ostream& stream, int val, ByteEncoderState *state) {
    
    assert(val >= 0);
    
    if (val <= 0x7f) {
        
        //
        // 1 byte UTF-8 sequence
        //
        
        auto firstByte = static_cast<unsigned char>(((val >> 0) & 0x7f) | 0x00);
        
        stream.put(firstByte);
        
    } else if (val <= 0x7ff) {
        
        //
        // 2 byte UTF-8 sequence
        //
        
        auto firstByte = static_cast<unsigned char>(((val >> 6) & 0x1f) | 0xc0);
        auto secondByte = static_cast<unsigned char>(((val >> 0) & 0x3f) | 0x80);
        
        stream.put(firstByte);
        stream.put(secondByte);
        
    } else if (val <= 0xffff) {
        
        //
        // 3 byte UTF-8 sequence
        //
        
        auto firstByte = static_cast<unsigned char>(((val >> 12) & 0x0f) | 0xe0);
        auto secondByte = static_cast<unsigned char>(((val >> 6) & 0x3f) | 0x80);
        auto thirdByte = static_cast<unsigned char>(((val >> 0) & 0x3f) | 0x80);
        
        stream.put(firstByte);
        stream.put(secondByte);
        stream.put(thirdByte);
        
    } else {
        
        //
        // 4 byte UTF-8 sequence
        //
        
        assert(val <= 0x10ffff);
        
        auto firstByte = static_cast<unsigned char>(((val >> 18) & 0x07) | 0xf0);
        auto secondByte = static_cast<unsigned char>(((val >> 12) & 0x3f) | 0x80);
        auto thirdByte = static_cast<unsigned char>(((val >> 6) & 0x3f) | 0x80);
        auto fourthByte = static_cast<unsigned char>(((val >> 0) & 0x3f) | 0x80);
        
        stream.put(firstByte);
        stream.put(secondByte);
        stream.put(thirdByte);
        stream.put(fourthByte);
    }
}

void ByteEncoder::encodeBytes(std::array<unsigned char, 4>& arr, int val, ByteEncoderState *state) {
    
    assert(val >= 0);
    
    if (val <= 0x7f) {
        
        //
        // 1 byte UTF-8 sequence
        //
        
        auto firstByte = static_cast<unsigned char>(((val >> 0) & 0x7f) | 0x00);
        
        arr[0] = firstByte;
        
    } else if (val <= 0x7ff) {
        
        //
        // 2 byte UTF-8 sequence
        //
        
        auto firstByte = static_cast<unsigned char>(((val >> 6) & 0x1f) | 0xc0);
        auto secondByte = static_cast<unsigned char>(((val >> 0) & 0x3f) | 0x80);
        
        arr[0] = firstByte;
        arr[1] = secondByte;
        
    } else if (val <= 0xffff) {
        
        //
        // 3 byte UTF-8 sequence
        //
        
        auto firstByte = static_cast<unsigned char>(((val >> 12) & 0x0f) | 0xe0);
        auto secondByte = static_cast<unsigned char>(((val >> 6) & 0x3f) | 0x80);
        auto thirdByte = static_cast<unsigned char>(((val >> 0) & 0x3f) | 0x80);
        
        arr[0] = firstByte;
        arr[1] = secondByte;
        arr[2] = thirdByte;
        
    } else {
        
        //
        // 4 byte UTF-8 sequence
        //
        
        assert(val <= 0x10ffff);
        
        auto firstByte = static_cast<unsigned char>(((val >> 18) & 0x07) | 0xf0);
        auto secondByte = static_cast<unsigned char>(((val >> 12) & 0x3f) | 0x80);
        auto thirdByte = static_cast<unsigned char>(((val >> 6) & 0x3f) | 0x80);
        auto fourthByte = static_cast<unsigned char>(((val >> 0) & 0x3f) | 0x80);
        
        arr[0] = firstByte;
        arr[1] = secondByte;
        arr[2] = thirdByte;
        arr[3] = fourthByte;
    }
}
