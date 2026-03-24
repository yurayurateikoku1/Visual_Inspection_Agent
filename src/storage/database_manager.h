#pragma once

#include "app/common.h"
#include <string>
#include <QSqlDatabase>
#include <mutex>

class DatabaseManager
{
public:
    static DatabaseManager &instance();

    bool init(const std::string &db_path = "data/inspection.db");
    void close();

    bool saveResult(const std::string &camera_id, const InspectionResult &result);
    int queryPassCount(int64_t from_ms, int64_t to_ms);
    int queryNgCount(int64_t from_ms, int64_t to_ms);

private:
    DatabaseManager() = default;
    void createTables();

    QSqlDatabase db_;
    std::mutex mutex_;
};
