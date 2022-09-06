#include <peersafe/app/tx/SchemaTx.h>
#include <peersafe/schema/SchemaParams.h>
#include <peersafe/app/tx/impl/Tuning.h>
#include <ripple/protocol/STTx.h>
#include <ripple/ledger/View.h>
#include <ripple/protocol/st.h>
#include <ripple/app/main/Application.h>
#include <ripple/app/ledger/LedgerMaster.h>
#include <ripple/core/Config.h>


namespace ripple {

	// A validator node can only participate in MAX_VALIDATOR_SCHEMA_COUNT schemas
	TER
	checkCountsForValidator(
		PreclaimContext const& ctx,
		std::vector<Blob> const& validators)
	{
		// check for final node count
		if (ctx.tx.getFieldU16(sfTransactionType) == ttSCHEMA_MODIFY &&
			ctx.tx.getFieldU16(sfOpType) == (uint16_t)SchemaModifyOp::del)
		{
			return tesSUCCESS;
		}

		std::map<Blob, int> mapValidatorCount;

        for (auto const& v : validators)
        {
            ++mapValidatorCount[v];
        }

		// This is a time-consuming process for a project that has many
		// sles.
        auto sleIndex = ctx.view.read(keylet::schema_index());
        if (sleIndex)
        {
            auto& schemaIndexes = sleIndex->getFieldV256(sfSchemaIndexes);
            for (auto const& index : schemaIndexes)
            {
                auto key = Keylet(ltSCHEMA, index);
                auto sle = ctx.view.read(key);
                if (sle)
                {
                    for (auto& validator : sle->getFieldArray(sfValidators))
                    {
                        Json::Value val(Json::objectValue);
                        auto publicKey = validator.getFieldVL(sfPublicKey);
                        if (++mapValidatorCount[publicKey] >
                            MAX_VALIDATOR_SCHEMA_COUNT)
                            return tefSCHEMA_MAX_SCHEMAS;
                    }
                }
            }
        }
		return tesSUCCESS;
	}

	TER preClaimCommon(PreclaimContext const& ctx)
	{
		std::vector<Blob> validators;
		auto const& vals = ctx.tx.getFieldArray(sfValidators);
		for (auto val : vals)
		{
			// check the construct of the validators object
			if (val.getCount() != 1 || 
				!val.isFieldPresent(sfPublicKey) || 
				val.getFieldVL(sfPublicKey).size() == 0 ||
				!publicKeyType(makeSlice(val.getFieldVL(sfPublicKey))))
			{
				return temBAD_VALIDATOR;
			}
			if (std::find(validators.begin(), validators.end(), val.getFieldVL(sfPublicKey)) != validators.end())
				return tefBAD_DUPLACATE_ITEM;
			validators.push_back(val.getFieldVL(sfPublicKey));
		}

		std::vector<Blob> peerList;
		auto const& peers = ctx.tx.getFieldArray(sfPeerList);
		for (auto peer : peers)
		{
			// check the construct of the validators object
			if (peer.getCount() != 1 || !peer.isFieldPresent(sfEndpoint) || 
				peer.getFieldVL(sfEndpoint).size() == 0)
			{
				return temBAD_PEERLIST;
			}
			if (std::find(peerList.begin(), peerList.end(), peer.getFieldVL(sfEndpoint)) != peerList.end())
				return tefBAD_DUPLACATE_ITEM;
			peerList.push_back(peer.getFieldVL(sfEndpoint));
		}

		if (validators.size() != peerList.size())
			return temMALFORMED;

		return checkCountsForValidator(ctx,validators);
	}

	TER checkMulsignValid(STArray const & vals, STArray const& txSigners)
	{
		for (auto const& txSigner : txSigners)
		{
			auto const &spk = txSigner.getFieldVL(sfSigningPubKey);

			auto iter(vals.end());
			iter = std::find_if(vals.begin(), vals.end(),
				[spk](STObject const &val) {
				if (val.getFieldVL(sfPublicKey) == spk) return true;
				return false;
			});
			if (iter == vals.end())
			{
				return temBAD_SIGNERFORVAL;
			}
		}
		return tesSUCCESS;
	}

	void setVavlidValInfo(STObject &val, const STTx & tx)
	{
		val.setFieldU8(sfSigned, (uint8_t)0);
		// Multi-Sign
		if (tx.getSigningPubKey().empty())
		{
			// Get the array of transaction signers.
			STArray const& txSigners(tx.getFieldArray(sfSigners));
			auto const &spk = val.getFieldVL(sfPublicKey);
			for (auto const& txSigner : txSigners)
			{
				if (txSigner.getFieldVL(sfSigningPubKey) == spk)
				{
					val.setFieldU8(sfSigned, 1);
				}
			}
		}
	}	

	NotTEC SchemaCreate::preflight(PreflightContext const& ctx)
	{
		auto const ret = preflight1(ctx);
		if (!isTesSuccess(ret))
			return ret;

		if( !ctx.tx.isFieldPresent(sfSchemaName) ||
			!ctx.tx.isFieldPresent(sfSchemaStrategy) ||
			!ctx.tx.isFieldPresent(sfValidators) ||
			!ctx.tx.isFieldPresent(sfPeerList))
			return temMALFORMED;

		if (ctx.app.schemaId() != beast::zero)
			return tefSCHEMA_TX_FORBIDDEN;

		return preflight2(ctx);
	}

	TER SchemaCreate::preclaim(PreclaimContext const& ctx)
	{
		auto j = ctx.app.journal("preclaimSchema");

		if ((uint8_t)SchemaStragegy::with_state ==ctx.tx.getFieldU8(sfSchemaStrategy)
			&&ctx.tx.isFieldPresent(sfAnchorLedgerHash)
			&&!ctx.app.getLedgerMaster().getLedgerByHash(ctx.tx.getFieldH256(sfAnchorLedgerHash)))
		{
			JLOG(j.trace()) << "anchor ledger is not match the schema strategy.";
			return temBAD_ANCHORLEDGER;
		} 
			
		if (ctx.tx.getFieldArray(sfValidators).size() <= 0 || 
			ctx.tx.getFieldArray(sfPeerList).size() <= 0)
		{
			return temMALFORMED;
		}
		else if (ctx.tx.getFieldArray(sfValidators).size() < MIN_NODE_COUNT_SCHEMA ||
			ctx.tx.getFieldArray(sfPeerList).size() < MIN_NODE_COUNT_SCHEMA)
		{
			return tefSCHEMA_NODE_COUNT;
		}

        if(  ctx.tx.isFieldPresent(sfSchemaAdmin) )
        {
            AccountID const uSchemaAccountID(ctx.tx[sfSchemaAdmin]);
            auto const k = keylet::account(uSchemaAccountID);
            auto const sleSchemaAdmin = ctx.view.read(k);

            if (!sleSchemaAdmin)
            {
                // Schema Admin account does not exist.
                return temBAD_SCHEMAADMIN;
            }
        }

		auto const ret = preClaimCommon(ctx);
		if (!isTesSuccess(ret))
			return ret;

		return checkMulsignValid(ctx.tx.getFieldArray(sfValidators), ctx.tx.getFieldArray(sfSigners));
		
	}

	TER SchemaCreate::doApply()
	{
		auto j = ctx_.app.journal("schemaCreateApply");

		auto const account = ctx_.tx[sfAccount];
		auto const sle = ctx_.view().peek(keylet::account(account));
		// Create schema in ledger
		auto const slep = std::make_shared<SLE>(
			keylet::schema(account, (*sle)[sfSequence] - 1, ctx_.view().info().parentHash));
		(*slep)[sfAccount] = account;
		(*slep)[sfSchemaName] = ctx_.tx[sfSchemaName];
		(*slep)[sfSchemaStrategy] = ctx_.tx[sfSchemaStrategy];
		(*slep)[~sfSchemaAdmin] = ctx_.tx[~sfSchemaAdmin];
		 
		if ((uint8_t)SchemaStragegy::with_state ==ctx_.tx.getFieldU8(sfSchemaStrategy) 
			&& !ctx_.tx.isFieldPresent(sfAnchorLedgerHash))
			(*slep)[~sfAnchorLedgerHash] =
				ctx_.app.getLedgerMaster().getValidatedLedger()->info().hash;
        else
            (*slep)[~sfAnchorLedgerHash] = ctx_.tx[~sfAnchorLedgerHash];

		//Reset validators
		{
			STArray vals = ctx_.tx.getFieldArray(sfValidators);
			for (auto& val : vals)
			{
				setVavlidValInfo(val, ctx_.tx);
			}
			slep->setFieldArray(sfValidators, vals);
		}

		STArray const& peerList = ctx_.tx.getFieldArray(sfPeerList);
		slep->setFieldArray(sfPeerList, peerList);
		
		ctx_.view().insert(slep);

		// Add schema to schema_index
        {
			auto sleIndexes = ctx_.view().peek(keylet::schema_index());
            STVector256 schemas;
            bool exists = true;
            if (!sleIndexes)
            {
                sleIndexes = std::make_shared<SLE>(keylet::schema_index());
                exists = false;
            }
            else
				schemas = sleIndexes->getFieldV256(sfSchemaIndexes);

            schemas.push_back(slep->key());
			sleIndexes->setFieldV256(sfSchemaIndexes, schemas);

            if (exists)
				ctx_.view().update(sleIndexes);
            else
                ctx_.view().insert(sleIndexes);

		}

		// Add schema to sender's owner directory
		{
			auto page = dirAdd(ctx_.view(), keylet::ownerDir(account), slep->key(),
				false, describeOwnerDir(account), ctx_.app.journal("View"));
			if (!page)
				return tecDIR_FULL;
			(*slep)[sfOwnerNode] = *page;
		}
		adjustOwnerCount(ctx_.view(), sle, 1, ctx_.journal);
		ctx_.view().update(sle);

		JLOG(j.trace()) << "schema sle is created.";

		return tesSUCCESS;
	}

	//------------------------------------------------------------------------------

	NotTEC SchemaModify::preflight(PreflightContext const& ctx)
	{
		auto const ret = preflight1(ctx);
		if (!isTesSuccess(ret))
			return ret;

		if (!ctx.tx.isFieldPresent(sfOpType)     ||
			!ctx.tx.isFieldPresent(sfValidators) ||
			!ctx.tx.isFieldPresent(sfPeerList)   ||
			!ctx.tx.isFieldPresent(sfSchemaID))
			return temMALFORMED;

        if (ctx.app.schemaId() != beast::zero)
            return tefSCHEMA_TX_FORBIDDEN;

		return preflight2(ctx);
	}

	TER SchemaModify::preclaim(PreclaimContext const& ctx)
	{
		auto j = ctx.app.journal("schemaModifyPreclaim");

		if (ctx.tx.getFieldU16(sfOpType) != (uint16_t)SchemaModifyOp::add && 
			ctx.tx.getFieldU16(sfOpType) != (uint16_t)SchemaModifyOp::del)
		{
			JLOG(j.trace()) << "modify operator is not valid.";
			return temBAD_OPTYPE;
		}

		if (ctx.tx.getFieldArray(sfValidators).size() <= 0 && 
			ctx.tx.getFieldArray(sfPeerList).size() <= 0)
		{
			return temMALFORMED;
		}

		return preClaimCommon(ctx);
	}

	TER SchemaModify::doApply()
	{
//		auto j = ctx_.app.journal("schemaModifyApply");
		auto sleSchema = ctx_.view().peek(Keylet(ltSCHEMA, ctx_.tx.getFieldH256(sfSchemaID)));
		if (sleSchema == nullptr)
		{
			return tefBAD_SCHEMAID;
		}

//		auto const account = ctx_.tx[sfAccount];
		if (!ctx_.tx.getSigningPubKey().empty())
		{
			if (!sleSchema->isFieldPresent(sfSchemaAdmin))
			{
				return tefBAD_SCHEMAADMIN;
			}
			if (sleSchema->getAccountID(sfSchemaAdmin) != ctx_.tx.getAccountID(sfAccount))
			{
				return tefBAD_SCHEMAADMIN;
			}			
		}
		else
		{
			if (sleSchema->getAccountID(sfAccount) != ctx_.tx.getAccountID(sfAccount))
			{
				return tefBAD_SCHEMAACCOUNT;
			}
			auto const ret = checkMulsignValid(sleSchema->getFieldArray(sfValidators), ctx_.tx.getFieldArray(sfSigners));
			if (!isTesSuccess(ret))
				return ret;
		}

		//for sle
		auto & peers = sleSchema->peekFieldArray(sfPeerList);
		auto & vals = sleSchema->peekFieldArray(sfValidators);

		//for tx
		STArray const & peersTx = ctx_.tx.getFieldArray(sfPeerList);
		STArray valsTx  = ctx_.tx.getFieldArray(sfValidators);

		//check for final node count
		if (ctx_.tx.getFieldU16(sfOpType) == (uint16_t)SchemaModifyOp::del)
		{
			if (vals.size() - valsTx.size() < MIN_NODE_COUNT_SCHEMA)
				return tefSCHEMA_NODE_COUNT;
		}

		for (auto& valTx : valsTx)
		{
			auto iter(vals.end());
			iter = std::find_if(vals.begin(), vals.end(),
				[valTx](STObject const &val) {
				auto const& spk = val.getFieldVL(sfPublicKey);
				auto const& spkTx = valTx.getFieldVL(sfPublicKey);

				return spk == spkTx;
			});
			if (ctx_.tx.getFieldU16(sfOpType) == (uint16_t)SchemaModifyOp::add)
			{
				if (iter != vals.end())
				{
					return tefSCHEMA_VALIDATOREXIST;
				}

				setVavlidValInfo(valTx, ctx_.tx);
				vals.push_back(valTx);
			}
			else
			{
				if (iter == vals.end())
				{
					return tefSCHEMA_NOVALIDATOR;
				}
				vals.erase(iter);
			}
		}

		for (auto const& peerTx : peersTx)
		{
			auto iter(peers.end());
			iter = std::find_if(peers.begin(), peers.end(),
				[peerTx](STObject const &peer) {
				auto const& sEndpoint = peer.getFieldVL(sfEndpoint);
				auto const& sEndpointTx = peerTx.getFieldVL(sfEndpoint);

				return sEndpoint == sEndpointTx;
			});
			if (ctx_.tx.getFieldU16(sfOpType) == (uint16_t)SchemaModifyOp::add)
			{
				if (iter != peers.end())
				{
					return tefSCHEMA_PEEREXIST;
				}
				peers.push_back(peerTx);
			}
			else
			{
				if (iter == peers.end())
				{
					return tefSCHEMA_NOPEER;
				}
				peers.erase(iter);
			}
		}

		ctx_.view().update(sleSchema);
		return tesSUCCESS;
	}

	NotTEC SchemaDelete::preflight(PreflightContext const& ctx)
	{
		auto const ret = preflight1(ctx);
		if (!isTesSuccess(ret))
			return ret;

		if (!ctx.tx.isFieldPresent(sfSchemaID))
			return temMALFORMED;

        if (ctx.app.schemaId() != beast::zero)
        return tefSCHEMA_TX_FORBIDDEN;

		return preflight2(ctx);
	}

	TER SchemaDelete::preclaim(PreclaimContext const& ctx)
	{
        return tesSUCCESS;
	}

	TER SchemaDelete::doApply()
	{
		auto j = ctx_.app.journal("schemaDeleteApply");
		auto sleSchema = ctx_.view().peek(Keylet(ltSCHEMA, ctx_.tx.getFieldH256(sfSchemaID)));
		if (sleSchema == nullptr)
		{
			return tefBAD_SCHEMAID;
		}

		auto const account = ctx_.tx[sfAccount];
		if (!ctx_.tx.getSigningPubKey().empty())
		{
			if (!sleSchema->isFieldPresent(sfSchemaAdmin))
			{
				return tefBAD_SCHEMAADMIN;
			}
			if (sleSchema->getAccountID(sfSchemaAdmin) != ctx_.tx.getAccountID(sfAccount))
			{
				return tefBAD_SCHEMAADMIN;
			}			
		}
		else
		{
			if (sleSchema->getAccountID(sfAccount) != account)
			{
				return tefBAD_SCHEMAACCOUNT;
			}
			auto const ret = checkMulsignValid(sleSchema->getFieldArray(sfValidators), ctx_.tx.getFieldArray(sfSigners));
			if (!isTesSuccess(ret))
				return ret;
		}
		if (!ctx_.view().dirRemove(
                keylet::ownerDir(sleSchema->getAccountID(sfAccount)),
                (*sleSchema)[sfOwnerNode],
                sleSchema->key(),
                true))
        {
            return tefBAD_LEDGER;
        }

		ctx_.view().erase(sleSchema);
		return tesSUCCESS;
	}
}
