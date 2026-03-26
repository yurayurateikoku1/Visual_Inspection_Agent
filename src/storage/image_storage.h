#pragma once

#include <string>
#include <halconcpp/HalconCpp.h>

class ImageStorage
{
public:
    static ImageStorage &instance();

    void setBaseDir(const std::string &dir);

    std::string saveImage(const std::string &camera_id, const HalconCpp::HObject &image,
                          bool is_ng = false);

    void cleanOldImages(int keep_days = 30);

private:
    ImageStorage() = default;
    std::string base_dir_ = "data/images";
};
