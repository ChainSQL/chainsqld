#pragma once
#include <ripple/basics/base_uint.h>
namespace ripple {
	using SchemaID = uint256;

	class SchemaBase {
	public:
		SchemaBase(SchemaID const& id) :schema_id_(id){
			
		}
		SchemaID schemaId() {
			return schema_id_;
		}
	protected:
		SchemaID schema_id_;
	};
}
