#pragma once

#include <ripple/protocol/AccountID.h>
#include <ripple/protocol/PublicKey.h>

namespace ripple {
	using SchemaID = uint256;

	enum class SchemaStragegy {
		new_chain = 1,
		with_state = 2,
	};

	enum class SchemaModifyOp {
		add = 1,
		del = 2,
	};

	class SchemaParams {
	public:
		SchemaID		schema_id;
		AccountID		account;
		std::string		schema_name;
		SchemaStragegy	strategy;
		AccountID		admin;
		uint256			anchor_ledger_hash;
		std::vector<std::pair<PublicKey, bool>> validator_list;
		std::vector<std::string>				peer_list;
	public:
		SchemaID schemaId()
		{
			return schema_id;
		}
	};

}