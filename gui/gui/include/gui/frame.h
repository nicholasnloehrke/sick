#pragma once

#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>

#include <TinyColormap.hpp>

#include <GL/glew.h>

namespace frame
{
    typedef tinycolormap::ColormapType colormap_type;

    struct Frame
    {
        std::vector<uint16_t> data;
        uint32_t height;
        uint32_t width;
        uint32_t number;
        uint64_t time_ms;

        Frame()
            : height(0), width(0), number(0), time_ms(0)
        {
        }

        Frame(const std::vector<uint16_t>& data, const uint32_t height, const uint32_t width, const uint32_t number, const uint64_t time)
            : data(data), height(height), width(width), number(number), time_ms(time)
        {
        }
    };

    const cv::Mat to_mat(const Frame& frame);

    const Frame to_frame(const cv::Mat& mat);

    bool to_texture(const cv::Mat& mat, GLuint& texture, int& texture_width, int& texture_height);
}