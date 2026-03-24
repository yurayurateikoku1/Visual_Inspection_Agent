#pragma once

#include <string>
#include <opencv2/core.hpp>

class ImageStorage
{
public:
    static ImageStorage &instance();

    void setBaseDir(const std::string &dir);

    std::string saveImage(const std::string &camera_id, const cv::Mat &image,
                          bool is_ng = false);

    void cleanOldImages(int keep_days = 30);

private:
    ImageStorage() = default;
    std::string base_dir_ = "data/images";
};
