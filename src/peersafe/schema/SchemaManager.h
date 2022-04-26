#pragma once

#include <ripple/basics/base_uint.h>
#include <peersafe/schema/Schema.h>
#include <map>

namespace ripple {

class SchemaParams;
class Application;
class Config;

class SchemaManager
{
public:
    SchemaManager(Application& app, beast::Journal j);

    std::shared_ptr<Schema>
    createSchema(Config& config, SchemaParams const& param);

    std::shared_ptr<Schema>
    createSchema(std::shared_ptr<Config> config, SchemaParams const& param);

    std::shared_ptr<Schema>
    createSchemaMain(std::shared_ptr<Config> config);

    std::shared_ptr<Schema>
    getSchema(uint256 const& schemaId);

    void
    removeSchema(uint256 const& schemaId);

    bool
    contains(uint256 const& id);

    void
    foreach(
        std::function<void(std::shared_ptr<Schema>)> fun);



private:
    std::map<uint256, std::shared_ptr<Schema>> schemas_;

    Application& app_;
    beast::Journal j_;

    std::recursive_mutex mutex_;
};
}  // namespace ripple