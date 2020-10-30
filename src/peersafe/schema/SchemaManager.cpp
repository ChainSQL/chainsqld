#include <peersafe/schema/SchemaManager.h>
#include <boost/format.hpp>

namespace ripple {
	SchemaManager::SchemaManager(Application & app, beast::Journal j):
		app_(app),
		j_(j)
	{

	}

	std::shared_ptr<Schema> SchemaManager::createSchema(Config& config,SchemaParams const& params)
	{	
		if (config.SCHEMA_PATH.empty())
		{
			JLOG(j_.warn()) << "schema_path not configured.";
			return nullptr;
		}			

		//Construct cfg for schema
		Config cfg;
		cfg.initSchemaConfig(config, params);

		//Reload cfg for schema
		auto schemaConfig = std::make_shared<Config>();
		std::string config_path = (boost::format("%1%/%2%/%3%")
			% config.SCHEMA_PATH
			% params.schema_id
			% "chainsqld.cfg").str();
		schemaConfig->setup(config_path, config.quiet(), config.silent(), config.standalone());
		return createSchema(schemaConfig,params);
	}

	std::shared_ptr<Schema> SchemaManager::createSchema(std::shared_ptr<Config> config, SchemaParams const& param)
	{
		auto schema = make_Schema(param, config, app_, j_);
		schema->doStart();
		schemas_[param.schema_id] = schema;

		return schema;
	}

	std::shared_ptr<Schema> SchemaManager::createSchemaMain(
		std::shared_ptr<Config> config)
	{
		assert(schemas_.empty());
		SchemaParams params{};
		params.schema_id = beast::zero;
		return createSchema(config, params);
	}

	std::shared_ptr<Schema> SchemaManager::getSchema(uint256 const& schemaId)
	{
		if (schemas_.find(schemaId) != schemas_.end())
			return schemas_[schemaId];
		else
			return nullptr;
	}

	void SchemaManager::removeSchema(uint256 const& schemaId)
	{
		if (!contains(schemaId))
			return;
		schemas_.erase(schemaId);
	}

	bool SchemaManager::contains(uint256 const& id)
	{
		return schemas_.find(id) != schemas_.end();
	}

	SchemaManager::schema_iterator SchemaManager::begin()
	{
		return SchemaManager::schema_iterator(this,true);
	}

	SchemaManager::schema_iterator SchemaManager::end()
	{
		return SchemaManager::schema_iterator(this,false);
	}
}