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

    class schema_iterator
    {
    public:
        schema_iterator(SchemaManager const* manager, bool begin)
        {
            if (begin)
                iter_ = manager->schemas_.begin();
            else
                iter_ = manager->schemas_.end();
        }
        void
        operator++()
        {
            iter_++;
        }

        void
        operator++(int)
        {
            operator++();
        }

        std::pair<uint256, std::shared_ptr<Schema>> operator*()
        {
            return *iter_;
        }
        const std::pair<uint256, std::shared_ptr<Schema>> operator*() const
        {
            return *iter_;
        }
        bool
        operator!=(const schema_iterator& src)
        {
            return iter_ != src.iter_;
        }
        uint256
        first()
        {
            return iter_->first;
        }
        std::shared_ptr<Schema>
        second()
        {
            return iter_->second;
        }

    private:
        std::map<uint256, std::shared_ptr<Schema>>::const_iterator iter_;

        friend class SchemaManager;
    };

    schema_iterator
    begin();
    schema_iterator
    end();

private:
    std::map<uint256, std::shared_ptr<Schema>> schemas_;

    Application& app_;
    beast::Journal j_;

    friend class schema_iterator;
};
}  // namespace ripple