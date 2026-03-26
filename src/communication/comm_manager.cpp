#include "comm_manager.h"
#include "modbus_tcp_client.h"
#include "modbus_rtu_client.h"
#include <spdlog/spdlog.h>

CommManager &CommManager::getInstance()
{
    static CommManager inst;
    return inst;
}

CommManager::CommManager()
{
    this->moveToThread(&worker_thread_);

    connect(&worker_thread_, &QThread::started, this, [this]()
            {
        light_ctrl_ = new LightSerialController(this);

        io_timer_ = new QTimer(this);
        connect(io_timer_, &QTimer::timeout, this, &CommManager::slot_refreshIO);
        io_timer_->start(200);

        conn_timer_ = new QTimer(this);
        connect(conn_timer_, &QTimer::timeout, this, &CommManager::slot_checkConnection);
        conn_timer_->start(3000);

        SPDLOG_INFO("CommManager thread started"); });

    worker_thread_.start();
}

CommManager::~CommManager() = default;

void CommManager::shutdown()
{
    if (!worker_thread_.isRunning())
        return;

    QMetaObject::invokeMethod(this, [this]()
                              {
        if (io_timer_) io_timer_->stop();
        if (conn_timer_) conn_timer_->stop();
        for (auto &[id, comm] : comms_)
            comm->disconnectDevice();
        comms_.clear();
        if (light_ctrl_) light_ctrl_->close(); }, Qt::BlockingQueuedConnection);

    worker_thread_.quit();
    worker_thread_.wait();

    SPDLOG_INFO("CommManager thread clearn");
}

void CommManager::addComm(const CommunicationParam &config)
{
    if (comms_.count(config.id))
    {
        spdlog::warn("Comm {} already exists", config.id);
        return;
    }

    std::unique_ptr<ModbusClientBase> comm;
    switch (config.protocol)
    {
    case CommProtocol::ModbusTCP:
        comm = std::make_unique<ModbusTcpClient>(config.id);
        break;
    case CommProtocol::ModbusRTU:
        comm = std::make_unique<ModbusRtuClient>(config.id);
        break;
    }

    if (comm->connectDevice(config))
    {
        SPDLOG_INFO("Comm [{}] ({}) connecting, protocol={}",
                    config.id, config.name,
                    config.protocol == CommProtocol::ModbusTCP ? "TCP" : "RTU");
        comm_configs_[config.id] = config;  // 保存配置用于重连
        comms_[config.id] = std::move(comm);
    }
    else
    {
        SPDLOG_ERROR("Comm [{}] connect failed", config.id);
        // 连接失败也保存配置和实例，后续定时器会自动重连
        comm_configs_[config.id] = config;
        comms_[config.id] = std::move(comm);
    }
}

void CommManager::disconnectAll()
{
    if (io_timer_)
        io_timer_->stop();
    if (conn_timer_)
        conn_timer_->stop();
    for (auto &[id, comm] : comms_)
        comm->disconnectDevice();
    comms_.clear();
    if (light_ctrl_)
        light_ctrl_->close();
}

void CommManager::writeSingleCoil(const std::string &comm_id, uint16_t addr, bool value)
{
    auto it = comms_.find(comm_id);
    if (it == comms_.end() || !it->second->isConnected())
    {
        SPDLOG_WARN("Comm {} not connected for coil write", comm_id);
        return;
    }
    it->second->writeSingleCoil(addr, value);
}

void CommManager::writeMultipleRegisters(const std::string &comm_id, uint16_t addr, const std::vector<uint16_t> &values)
{
    auto it = comms_.find(comm_id);
    if (it == comms_.end() || !it->second->isConnected())
    {
        SPDLOG_WARN("Comm {} not connected for register write", comm_id);
        return;
    }
    it->second->writeMultipleRegisters(addr, values);
}

void CommManager::openLightSerial(const std::string &port, int baud_rate)
{
    light_port_ = port;
    light_baud_rate_ = baud_rate;
    if (light_ctrl_ && !light_ctrl_->isOpen())
    {
        bool ok = light_ctrl_->open(port, baud_rate);
        emit sign_lightStatusChanged(ok);
    }
}

void CommManager::setLuminance(int channel, int luminance)
{
    if (light_ctrl_ && light_ctrl_->isOpen())
        light_ctrl_->setLuminance(channel, luminance);
}

std::array<bool, 8> CommManager::diState() const
{
    std::lock_guard lock(state_mutex_);
    return di_state_;
}

std::array<bool, 8> CommManager::doState() const
{
    std::lock_guard lock(state_mutex_);
    return do_state_;
}

bool CommManager::isCommConnected(const std::string &id) const
{
    auto it = comms_.find(id);
    return it != comms_.end() && it->second->isConnected();
}

// ── 子线程定时器槽 ──

void CommManager::slot_refreshIO()
{
    auto it = comms_.find("plc_1");
    if (it == comms_.end() || !it->second->isConnected())
        return;

    auto *comm = it->second.get();

    comm->readCoils(ModbusAddr::DI0_STRIP_VISION, 8, [this](bool ok, const std::vector<bool> &vals)
                    {
        if (!ok) return;
        {
            std::lock_guard lock(state_mutex_);
            for (size_t i = 0; i < vals.size() && i < 8; ++i)
                di_state_[i] = vals[i];
        }
        emit sign_ioStateUpdated(); });

    comm->readCoils(ModbusAddr::DO0_VISION_OK, 8, [this](bool ok, const std::vector<bool> &vals)
                    {
        if (!ok) return;
        {
            std::lock_guard lock(state_mutex_);
            for (size_t i = 0; i < vals.size() && i < 8; ++i)
                do_state_[i] = vals[i];
        }
        emit sign_ioStateUpdated(); });
}

void CommManager::slot_checkConnection()
{
    // Modbus：每次都汇报实际连接状态，断线时尝试重连
    for (auto &[id, comm] : comms_)
    {
        bool connected = comm->isConnected();
        emit sign_commStatusChanged(id, connected);

        if (!connected)
        {
            auto cfg_it = comm_configs_.find(id);
            if (cfg_it == comm_configs_.end())
                continue;

            SPDLOG_WARN("Comm {} disconnected, attempting reconnect...", id);

            // 先断开旧连接，再重连（connectDevice 是异步的，下次定时器再检查状态）
            comm->disconnectDevice();
            if (comm->connectDevice(cfg_it->second))
            {
                SPDLOG_INFO("Comm {} reconnect initiated", id);
            }
            else
            {
                SPDLOG_WARN("Comm {} reconnect failed, will retry in 3s", id);
            }
        }
    }

    // 光源串口：每次都汇报实际状态，断线时尝试重连
    if (light_ctrl_ && !light_port_.empty())
    {
        bool light_ok = light_ctrl_->isOpen();
        emit sign_lightStatusChanged(light_ok);

        if (!light_ok)
        {
            SPDLOG_WARN("Light serial disconnected, attempting reconnect on {}...", light_port_);
            if (light_ctrl_->open(light_port_, light_baud_rate_))
            {
                SPDLOG_INFO("Light serial reconnected on {}", light_port_);
                emit sign_lightStatusChanged(true);
            }
        }
    }
}
