#include "algorithm_factory.h"
#include <spdlog/spdlog.h>

AlgorithmFactory &AlgorithmFactory::instance()
{
    static AlgorithmFactory inst;
    return inst;
}

void AlgorithmFactory::registerAlgorithm(const std::string &name, Creator creator)
{
    creators_[name] = std::move(creator);
    spdlog::info("Algorithm registered: {}", name);
}

std::shared_ptr<IAlgorithm> AlgorithmFactory::create(const std::string &name) const
{
    auto it = creators_.find(name);
    if (it == creators_.end())
    {
        spdlog::error("Algorithm not found: {}", name);
        return nullptr;
    }
    return it->second();
}

std::vector<std::string> AlgorithmFactory::registeredNames() const
{
    std::vector<std::string> names;
    for (auto &[name, _] : creators_)
        names.push_back(name);
    return names;
}
