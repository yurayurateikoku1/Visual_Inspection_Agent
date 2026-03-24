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
            id          INTEGER PRIMARY KEY AUTOINCREMENT,
            camera_id   TEXT NOT NULL,
            pass        INTEGER NOT NULL,
            detail      TEXT,
            confidence  REAL,
            timestamp_ms INTEGER NOT NULL,
            image_path  TEXT
        )
    )");
    q.exec("CREATE INDEX IF NOT EXISTS idx_timestamp ON inspection_results(timestamp_ms)");
    q.exec("CREATE INDEX IF NOT EXISTS idx_camera ON inspection_results(camera_id)");
}

bool DatabaseManager::saveResult(const std::string &camera_id, const InspectionResult &result)
{
    std::lock_guard lock(mutex_);
    QSqlQuery q(db_);
    q.prepare(R"(
        INSERT INTO inspection_results (camera_id, pass, detail, confidence, timestamp_ms)
        VALUES (:cam, :pass, :detail, :conf, :ts)
    )");
    q.bindValue(":cam", QString::fromStdString(camera_id));
    q.bindValue(":pass", result.pass ? 1 : 0);
    q.bindValue(":detail", QString::fromStdString(result.detail));
    q.bindValue(":conf", result.confidence);
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
