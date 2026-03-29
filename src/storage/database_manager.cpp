#include "database_manager.h"
#include <QSqlQuery>
#include <QSqlError>
#include <spdlog/spdlog.h>

DatabaseManager &DatabaseManager::instance()
{
    static DatabaseManager inst;
    return inst;
}

bool DatabaseManager::init(const std::string &db_path)
{
    db_ = QSqlDatabase::addDatabase("QSQLITE");
    db_.setDatabaseName(QString::fromStdString(db_path));

    if (!db_.open())
    {
        spdlog::error("Database open failed: {}", db_.lastError().text().toStdString());
        return false;
    }

    createTables();
    spdlog::info("Database initialized: {}", db_path);
    return true;
}

void DatabaseManager::close()
{
    db_.close();
}

void DatabaseManager::createTables()
{
    QSqlQuery q(db_);
    q.exec(R"(
        CREATE TABLE IF NOT EXISTS inspection_results (
            id           INTEGER PRIMARY KEY AUTOINCREMENT,
            camera_name  TEXT NOT NULL,
            pass         INTEGER NOT NULL,
            defects      TEXT,
            timestamp_ms INTEGER NOT NULL,
            image_path   TEXT
        )
    )");
    q.exec("CREATE INDEX IF NOT EXISTS idx_timestamp ON inspection_results(timestamp_ms)");
    q.exec("CREATE INDEX IF NOT EXISTS idx_camera ON inspection_results(camera_name)");
}

bool DatabaseManager::saveResult(const std::string &camera_name, const InspectionResult &result)
{
    // Serialize defects as simple CSV: "label,conf,r1,c1,r2,c2|..."
    std::string defects_str;
    for (const auto &d : result.defects)
    {
        if (!defects_str.empty()) defects_str += "|";
        defects_str += d.label + "," +
                       std::to_string(d.confidence) + "," +
                       std::to_string(d.row1) + "," +
                       std::to_string(d.col1) + "," +
                       std::to_string(d.row2) + "," +
                       std::to_string(d.col2);
    }

    std::lock_guard lock(mutex_);
    QSqlQuery q(db_);
    q.prepare(R"(
        INSERT INTO inspection_results (camera_name, pass, defects, timestamp_ms)
        VALUES (:cam, :pass, :defects, :ts)
    )");
    q.bindValue(":cam", QString::fromStdString(camera_name));
    q.bindValue(":pass", result.pass ? 1 : 0);
    q.bindValue(":defects", QString::fromStdString(defects_str));
    q.bindValue(":ts", static_cast<qint64>(result.timestamp_ms));

    if (!q.exec())
    {
        spdlog::error("saveResult failed: {}", q.lastError().text().toStdString());
        return false;
    }
    return true;
}

int DatabaseManager::queryPassCount(int64_t from_ms, int64_t to_ms)
{
    std::lock_guard lock(mutex_);
    QSqlQuery q(db_);
    q.prepare("SELECT COUNT(*) FROM inspection_results WHERE pass=1 AND timestamp_ms BETWEEN :from AND :to");
    q.bindValue(":from", static_cast<qint64>(from_ms));
    q.bindValue(":to", static_cast<qint64>(to_ms));
    q.exec();
    return q.next() ? q.value(0).toInt() : 0;
}

int DatabaseManager::queryNgCount(int64_t from_ms, int64_t to_ms)
{
    std::lock_guard lock(mutex_);
    QSqlQuery q(db_);
    q.prepare("SELECT COUNT(*) FROM inspection_results WHERE pass=0 AND timestamp_ms BETWEEN :from AND :to");
    q.bindValue(":from", static_cast<qint64>(from_ms));
    q.bindValue(":to", static_cast<qint64>(to_ms));
    q.exec();
    return q.next() ? q.value(0).toInt() : 0;
}
