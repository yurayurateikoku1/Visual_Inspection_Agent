#pragma once

#include "algorithm_interface.h"
#include <map>
#include <functional>
#include <memory>

class AlgorithmFactory
{
public:
    using Creator = std::function<std::shared_ptr<IAlgorithm>()>;

    static AlgorithmFactory &instance();

    void registerAlgorithm(const std::string &name, Creator creator);
    std::shared_ptr<IAlgorithm> create(const std::string &name) const;
    std::vector<std::string> registeredNames() const;

    /// @brief 按分类返回已注册的算法 { category → [algorithm_id, ...] }
    std::map<std::string, std::vector<std::string>> registeredByCategory() const;

private:
    AlgorithmFactory() = default;
    std::map<std::string, Creator> creators_;
};

// 算法自注册宏
#define REGISTER_ALGORITHM(cls) \
    static bool _reg_##cls = [] {                                            \
        via::AlgorithmFactory::instance().registerAlgorithm(                 \
            #cls, []() -> std::shared_ptr<via::IAlgorithm> {                \
                return std::make_shared<cls>();                               \
            });                                                              \
        return true; }()
