#pragma once

#include <rfl.hpp>
#include <array>
#include "header.hpp"
#include <sertial/containers/fixed_vector.hpp>

namespace examples::messages {

/// Camera intrinsic parameters
template<typename T = float>
struct CameraIntrinsics {
    T fx = T{0};  // Focal length X
    T fy = T{0};  // Focal length Y
    T cx = T{0};  // Principal point X
    T cy = T{0};  // Principal point Y
    
    // Distortion coefficients (radial + tangential)
    T k1 = T{0};  // Radial distortion 1
    T k2 = T{0};  // Radial distortion 2
    T p1 = T{0};  // Tangential distortion 1
    T p2 = T{0};  // Tangential distortion 2
    T k3 = T{0};  // Radial distortion 3
};

/// Camera info message - intrinsics + image dimensions
template<typename THeader = Header<>,
         typename TIntrinsics = CameraIntrinsics<>>
struct CameraInfo {
    THeader header;
    
    uint32_t width = 0;      // Image width
    uint32_t height = 0;     // Image height
    
    TIntrinsics intrinsics;
    
    // Region of Interest (for cropped images)
    uint32_t roi_x = 0;
    uint32_t roi_y = 0;
    uint32_t roi_width = 0;
    uint32_t roi_height = 0;
};

/// Small grayscale image (64x48 for embedded)
template<typename THeader = Header<>,
         size_t Width = 64, 
         size_t Height = 48>
struct ImageGray {
    THeader header;
    static constexpr uint16_t width = Width;
    static constexpr uint16_t height = Height;
    std::array<uint8_t, Width * Height> data = {};
};

/// Small depth image (64x48, depth in mm)
template<typename THeader = Header<>,
         size_t Width = 64, 
         size_t Height = 48>
struct DepthImage {
    THeader header;
    static constexpr uint16_t width = Width;
    static constexpr uint16_t height = Height;
    float focal_length = 0.0f;
    std::array<uint16_t, Width * Height> data = {};
};

} // namespace examples::messages
