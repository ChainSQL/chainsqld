#pragma once

#include <ripple/protocol/AccountID.h>
#include <ripple/protocol/PublicKey.h>
#include <ripple/protocol/STLedgerEntry.h>
#include <ripple/protocol/st.h>
#include <ripple/basics/Slice.h>
#include <ripple/basics/StringUtilities.h>

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

		void readFromSle(std::shared_ptr<SLE const> const& sleSchema)
		{
			auto validators = sleSchema->getFieldArray(sfValidators);
			for (auto& validator : validators)
			{
				auto publicKey = PublicKey(makeSlice(validator.getFieldVL(sfPublicKey)));
				auto signedVal = (validator.getFieldU8(sfSigned) == 1);
				validator_list.push_back(std::make_pair(publicKey, signedVal));
			}

			account = sleSchema->getAccountID(sfAccount);
			schema_id = sleSchema->key();
			schema_name = strCopy(sleSchema->getFieldVL(sfSchemaName));
			strategy = sleSchema->getFieldU8(sfSchemaStrategy) == 1 ?
				SchemaStragegy::new_chain : SchemaStragegy::with_state;
			if (strategy == SchemaStragegy::with_state)
			{
				anchor_ledger_hash = sleSchema->getFieldH256(sfAnchorLedgerHash);
			}
			if (sleSchema->isFieldPresent(sfSchemaAdmin))
				admin = sleSchema->getAccountID(sfSchemaAdmin);

			auto peers = sleSchema->getFieldArray(sfPeerList);
			for (auto& peer : peers)
			{
				peer_list.push_back(strCopy(peer.getFieldVL(sfEndpoint)));
			}
		}

		void modify(SchemaModifyOp op,
			std::vector<PublicKey> const& validators,
			std::vector<std::string> const& peers)
		{
			if (op == SchemaModifyOp::add)
			{
				for(auto key : validators)
					validator_list.push_back(std::make_pair(key, 0));
				this->peer_list.insert(this->peer_list.end(), peers.begin(), peers.end());
			}
			else
			{
				for (auto key : validators)
				{
					auto it = validator_list.begin();
					while (it != validator_list.end())
					{
						if (it->first == key)
							it = validator_list.erase(it);
						else
							it++;
					}
				}
				for (auto peer : peers)
				{
					auto it = peer_list.begin();
					while (it != peer_list.end())
					{
						if (*it == peer)
							it = peer_list.erase(it);
						else
							it++;
					}
				}
			}
		}
	};

}