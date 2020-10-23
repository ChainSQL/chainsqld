#include <peersafe/schema/SchemaManager.h>


namespace ripple {
	SchemaManager::SchemaManager(Application & app, beast::Journal j):
		app_(app),
		j_(j)
	{

	}

	std::shared_ptr<Schema> SchemaManager::createSchema(Config& config,SchemaParams const& params)
	{	
		if (config.SCHEMA_PATH.empty())
			return nullptr;

		auto schemaConfig = std::make_shared<Config>();
		schemaConfig->initSchemaConfig(config, params);

		return createSchema(schemaConfig,params);
	}

	std::shared_ptr<Schema> SchemaManager::createSchema(std::shared_ptr<Config> config, SchemaParams const& param)
	{
		auto schema = make_Schema(param, config, app_, j_);
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