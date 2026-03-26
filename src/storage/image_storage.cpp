#include "image_storage.h"
#include <spdlog/spdlog.h>
#include <filesystem>
#include <chrono>
#include <format>

ImageStorage &ImageStorage::instance()
{
    static ImageStorage inst;
    return inst;
}

void ImageStorage::setBaseDir(const std::string &dir)
{
    base_dir_ = dir;
}

std::string ImageStorage::saveImage(const std::string &camera_id, const HalconCpp::HObject &image, bool is_ng)
{
    namespace fs = std::filesystem;
    using namespace std::chrono;

    auto now = system_clock::now();
    auto date = floor<days>(now);
    auto ymd = year_month_day{date};

    std::string sub = is_ng ? "NG" : "OK";
    std::string dir = std::format("{}/{}/{:04d}{:02d}{:02d}",
                                  base_dir_, camera_id,
                                  int(ymd.year()), unsigned(ymd.month()), unsigned(ymd.day()));

    fs::create_directories(dir + "/" + sub);

    auto ts = duration_cast<milliseconds>(now.time_since_epoch()).count();
    std::string filename = std::format("{}/{}/{}_{}.bmp", dir, sub, camera_id, ts);

    try
    {
        HalconCpp::WriteImage(image, "bmp", 0, filename.c_str());
    }
    catch (HalconCpp::HException &e)
    {
        spdlog::error("ImageStorage: WriteImage failed: {}", e.ErrorMessage().Text());
    }
    return filename;
}

void ImageStorage::cleanOldImages(int keep_days)
{
    namespace fs = std::filesystem;
    using namespace std::chrono;

    auto cutoff = system_clock::now() - days(keep_days);
    auto cutoff_time = file_clock::from_sys(cutoff);

    if (!fs::exists(base_dir_))
        return;

    for (auto &entry : fs::recursive_directory_iterator(base_dir_))
    {
        if (entry.is_regular_file() && entry.last_write_time() < cutoff_time)
        {
            fs::remove(entry.path());
        }
    }
    spdlog::info("ImageStorage: cleaned images older than {} days", keep_days);
}
