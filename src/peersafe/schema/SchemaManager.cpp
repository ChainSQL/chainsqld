#include <peersafe/schema/SchemaManager.h>


namespace ripple {
	SchemaManager::SchemaManager(Application & app, beast::Journal j):
		app_(app),
		j_(j)
	{

	}

	std::shared_ptr<Schema> SchemaManager::createSchema(Config& config,SchemaParams const& params, bool loadFromFile /*= false*/)
	{	
		auto schemaConfig = std::make_unique<Config>();	
		
		if(!loadFromFile)
			schemaConfig->initSchemaConfig(config, params);

		if (loadFromFile) {
			schemaConfig->START_UP = Config::NEWCHAIN_LOAD;
		}
	
		auto schema = make_Schema(params, *schemaConfig, app_, j_);
		schemas_[params.schema_id] = schema;

		return schema;
	}

	std::shared_ptr<Schema> SchemaManager::createSchemaMain(
		Config& config)
	{
		assert(schemas_.empty());
		SchemaParams params{};
		auto schema = make_Schema(params, config, app_, j_);
		schemas_[beast::zero] = schema;
		return schema;
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