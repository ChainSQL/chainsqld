#pragma once

#include <ripple/protocol/AccountID.h>
#include <ripple/protocol/PublicKey.h>
#include <peersafe/schema/SchemaBase.h>

namespace ripple {
	enum class SchemaStragegy {
		new_chain = 1,
		with_state = 2,
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
	};

}