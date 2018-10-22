#include <peersafe/app/misc/SleOps.h>
#include <ripple/protocol/digest.h>
#include <ripple/protocol/TxFormats.h>
#include <ripple/app/tx/apply.h>
#include <ripple/ledger/ApplyViewImpl.h>
#include <ripple/app/misc/NetworkOPs.h>
#include <peersafe/app/tx/DirectApply.h>
#include <peersafe/app/misc/ContractHelper.h>
#include <ripple/basics/StringUtilities.h>
#include <ripple/app/ledger/LedgerMaster.h>
#include <peersafe/rpc/TableUtils.h>
#include <ripple/rpc/handlers/Handlers.h>
#include <peersafe/app/sql/TxStore.h>

namespace ripple {
    //just raw function for zxc, all paras should be tranformed in extvmFace modules.

    SleOps::SleOps(ApplyContext& ctx)
        :ctx_(ctx),
		bTransaction_(false),
		txHash_(uint256(0))
        , contractCacheCode_("contractCode", 100, 300, stopwatch(), ctx.app.journal("TaggedCache"))
    {
    }

    SLE::pointer SleOps::getSle(AccountID const & addr) const
    {        
        ApplyView& view = ctx_.view();
        auto const k = keylet::account(addr);
        return view.peek(k);
    }

	void SleOps::incNonce(AccountID const& addr)
	{
		SLE::pointer pSle = getSle(addr);
		if (pSle)
		{
			uint32 nonce = pSle->getFieldU32(sfNonce);
			{
				pSle->setFieldU32(sfNonce, ++nonce);
				ctx_.view().update(pSle);
			}
		}		
	}

	uint32 SleOps::getNonce(AccountID const& addr)
	{
		SLE::pointer pSle = getSle(addr);
		if (pSle)
			return pSle->getFieldU32(sfNonce);
		else
			return 0;
	}

	void SleOps::setNonce(AccountID const& addr, uint32 const& _newNonce)
	{
		SLE::pointer pSle = getSle(addr);
		if(pSle)
			pSle->setFieldU32(sfNonce, _newNonce);
	}

	bool SleOps::addressHasCode(AccountID const& addr)
	{
		SLE::pointer pSle = getSle(addr);
		if (pSle && pSle->isFieldPresent(sfContractCode))
			return true;
		return false;
	}

	void SleOps::setCode(AccountID const& addr, bytes&& code)
	{
		SLE::pointer pSle = getSle(addr);
		if(pSle)
			pSle->setFieldVL(sfContractCode,code);
	}

	bytes const& SleOps::code(AccountID const& addr) 	
    {
        Blob *pBlobCode = contractCacheCode_.fetch(addr).get();
        if (nullptr == pBlobCode)
        {         
            SLE::pointer pSle = getSle(addr);
			Blob blobCode;
			if (pSle)
				blobCode = pSle->getFieldVL(sfContractCode);
	
            auto p = std::make_shared<ripple::Blob>(blobCode);
            contractCacheCode_.canonicalize(addr, p);

            pBlobCode = p.get();
        }        
        return *pBlobCode;
	}

	uint256 SleOps::codeHash(AccountID const& addr)
	{
        bytes const& code = SleOps::code(addr);
		return sha512Half(code);
	}

	TER SleOps::transferBalance(AccountID const& _from, AccountID const& _to, uint256 const& _value)
	{
		if (_value == uint256(0))
			return tesSUCCESS;

		int64_t value = fromUint256(_value);
		auto ret = subBalance(_from, value);
		if(ret == tesSUCCESS)
			addBalance(_to, value);
		return ret;
	}

	TER SleOps::doPayment(AccountID const& _from, AccountID const& _to, uint256 const& _value)
	{
		int64_t value = fromUint256(_value);
		STTx paymentTx(ttPAYMENT,
			[&_from,&_to,&value](auto& obj)
		{
			obj.setAccountID(sfAccount, _from);
			obj.setAccountID(sfDestination, _to);
			obj.setFieldAmount(sfAmount, ZXCAmount(value));
		});
		auto ret = applyDirect(ctx_.app, ctx_.view(), paymentTx, ctx_.app.journal("Executive"));
		return ret;
	}

	bool SleOps::createContractAccount(AccountID const& _from, AccountID const& _to, uint256 const& _value)
	{
		// Open a ledger for editing.
		auto const k = keylet::account(_to);
		SLE::pointer sleDst = ctx_.view().peek(k);

		if (!sleDst)
		{
			// Create the account.
			sleDst = std::make_shared<SLE>(k);
			sleDst->setAccountID(sfAccount, _to);
			sleDst->setFieldU32(sfSequence, 1);
			ctx_.view().insert(sleDst);
		}

		if (_value != uint256(0))
		{
			transferBalance(_from, _to, _value);
		}

		return true;
	}

	void SleOps::addBalance(AccountID const& addr, int64_t const& amount)
	{
		SLE::pointer pSle = getSle(addr);
		if (pSle)
		{
			auto balance = pSle->getFieldAmount(sfBalance).zxc().drops();
			int64_t finalBanance = balance + amount;
			if (amount > 0)
			{
				pSle->setFieldAmount(sfBalance, ZXCAmount(finalBanance));
				ctx_.view().update(pSle);
			}			
		}
	}

	TER SleOps::subBalance(AccountID const& addr, int64_t const& amount)
	{
		SLE::pointer pSle = getSle(addr);
		if (pSle)
		{
			// This is the total reserve in drops.
			auto const uOwnerCount = pSle->getFieldU32(sfOwnerCount);
			auto const reserve = ctx_.view().fees().accountReserve(uOwnerCount);

			auto balance = pSle->getFieldAmount(sfBalance).zxc().drops();
			int64_t finalBanance = balance - amount;
			if (finalBanance > reserve)
			{
				pSle->setFieldAmount(sfBalance, ZXCAmount(finalBanance));
				ctx_.view().update(pSle);
			}
			else
				return tecUNFUNDED_PAYMENT;
		}
		return tesSUCCESS;
	}

	int64_t SleOps::balance(AccountID const& address)
	{
		SLE::pointer pSle = getSle(address);
		if (pSle)
			return pSle->getFieldAmount(sfBalance).zxc().drops();
		else
			return 0;
	}

	void SleOps::clearStorage(AccountID const& _contract)
	{
		SLE::pointer pSle = getSle(_contract);
		if (pSle)
		{
			pSle->makeFieldAbsent(sfContractCode);
			ctx_.view().update(pSle);
		}			
	}

	AccountID SleOps::calcNewAddress(AccountID sender, int nonce)
	{
		bytes data(sender.begin(), sender.end());
		data.push_back(nonce);

		ripesha_hasher rsh;
		rsh(data.data(), data.size());
		auto const d = static_cast<
			ripesha_hasher::result_type>(rsh);
		AccountID id;
		static_assert(sizeof(d) == id.size(), "");
		std::memcpy(id.data(), d.data(), d.size());
		return id;
	}

    void SleOps::PubContractEvents(const AccountID& contractID, uint256 const * aTopic, int iTopicNum, const Blob& byValue)
    {
        ctx_.app.getOPs().PubContractEvents(contractID, aTopic, iTopicNum, byValue);
    }

    void SleOps::kill(AccountID addr)
    {
		SLE::pointer pSle = getSle(addr);
		if (pSle)
			ctx_.view().erase(pSle);
    }

	int64_t SleOps::executeSQL(AccountID const& _account, AccountID const& _owner, TableOpType _iType, std::string _sTableName, std::string _sRaw)
	{
		return 0;
	}

	void SleOps::addCommonFields(STObject& obj,AccountID const& _account)
	{
		obj.setAccountID(sfAccount, _account);

		obj.setFieldU32(sfSequence, 0);
		obj.setFieldAmount(sfFee, STAmount());
		obj.setFieldVL(sfSigningPubKey, Slice(nullptr, 0));
	}

	std::pair<bool, STArray> SleOps::genTableFields(const ApplyContext &_ctx, AccountID const& _account, std::string _sTableName, std::string _tableNewName, bool bNewNameInDB)
	{
		STArray tables;
		STObject table(sfTable);
		table.setFieldVL(sfTableName, strCopy(_sTableName));
		if(!_tableNewName.empty())
			table.setFieldVL(sfTableNewName, strCopy(_tableNewName));
		if (bNewNameInDB)
		{
			auto nameInDB = generateNameInDB(_ctx.view().seq(), _account, _sTableName);
			table.setFieldH160(sfNameInDB, nameInDB);
		}
		else
		{
			auto ledgerSeq = _ctx.app.getLedgerMaster().getValidLedgerIndex();
			auto nameInDB = _ctx.app.getLedgerMaster().getNameInDB(ledgerSeq, _account, _sTableName);
			if (!nameInDB)
			{
				auto j = _ctx.app.journal("Executive");
				JLOG(j.info())
					<< "SleOps genTableFields getNameInDB failed,account="
					<< to_string(_account)
					<< ",tableName="
					<< _sTableName;
				return std::make_pair(false, tables);
			}
				
			table.setFieldVL(sfNameInDB, strCopy(to_string(nameInDB)));
		}
		
		tables.push_back(table);
		return std::make_pair(true, tables);

	}

	//table operation
	bool SleOps::createTable(AccountID const& _account, std::string const& _sTableName, std::string const& _raw)
	{
		const ApplyContext &_ctx = ctx_;
		bool bRel = false;
		STTx tx(ttTABLELISTSET,
			[&_account, &_sTableName, &_raw, &_ctx, &bRel](auto& obj)
		{

			obj.setFieldU16(sfOpType, T_CREATE);
			obj.setFieldVL(sfRaw, strCopy(_raw));
			auto tables = genTableFields(_ctx, _account, _sTableName, "", true);
			if (tables.first)
				obj.setFieldArray(sfTables, tables.second);
			else
				return;

			SleOps::addCommonFields(obj, _account);
			bRel = true;
		});
		if (!bRel) {
			auto j = ctx_.app.journal("Executive");
			JLOG(j.info())
				<< "SleOps dropTable,apply result: failed.";
			return bRel;
		}
		auto j = ctx_.app.journal("Executive");
		JLOG(j.warn()) <<
			"-----------createTable subTx: " << tx;
		if (bTransaction_) {
			std::vector<STTx>  txs = ctx_.app.getContractHelper().getTxsByHash(txHash_);
			if (txs.size() > 0)
				txs.at(0).addSubTx(tx);
			return tesSUCCESS;
		}
		auto ret = applyDirect(ctx_.app, ctx_.view(), tx, ctx_.app.journal("SleOps"));
		if (ret != tesSUCCESS)
		{
			auto j = ctx_.app.journal("Executive");
			JLOG(j.info())
				<< "SleOps createTable,apply result:"
				<< transToken(ret);
		}

		if (ctx_.view().flags() & tapForConsensus)
		{
			ctx_.tx.addSubTx(tx);
			//ctx_.app.getContractHelper().addTx(ctx_.tx.getTransactionID(), tx);
		}
		
		return ret == tesSUCCESS;
	}

	bool SleOps::dropTable(AccountID const& _account, std::string const& _sTableName)
	{
		const ApplyContext &_ctx = ctx_;
		bool bRel = false;
		STTx tx(ttTABLELISTSET,
			[&_account, &_sTableName, &_ctx, &bRel](auto& obj)
		{
			auto tables = genTableFields(_ctx, _account, _sTableName, "", false);
			if (tables.first)
				obj.setFieldArray(sfTables, tables.second);
			else
				return;

			SleOps::addCommonFields(obj, _account);

			obj.setFieldU16(sfOpType, T_DROP);
			obj.setAccountID(sfAccount, _account);
			bRel = true;
		});
		if (!bRel) {
			auto j = ctx_.app.journal("Executive");
			JLOG(j.info())
				<< "SleOps dropTable,apply result: failed.";
			return bRel;
		}
		if (bTransaction_) {
			std::vector<STTx>  txs = ctx_.app.getContractHelper().getTxsByHash(txHash_);
			if (txs.size() > 0)
				txs.at(0).addSubTx(tx);
			return tesSUCCESS;
		}
		auto ret = applyDirect(ctx_.app, ctx_.view(), tx, ctx_.app.journal("SleOps"));
		if (ret != tesSUCCESS)
		{
			auto j = ctx_.app.journal("Executive");
			JLOG(j.info())
				<< "SleOps dropTable,apply result:"
				<< transToken(ret);
		}

		if (ctx_.view().flags() & tapForConsensus)
		{
			ctx_.tx.addSubTx(tx);
		}

		return ret == tesSUCCESS;
	}

	bool SleOps::renameTable(AccountID const& _account, std::string const& _sTableName, std::string const& _sTableNewName)
	{
		const ApplyContext &_ctx = ctx_;
		bool bRel = false;
		STTx tx(ttTABLELISTSET,
			[&_account, &_sTableName, &_sTableNewName, &_ctx, &bRel](auto& obj)
		{
			auto tables = genTableFields(_ctx, _account, _sTableName, _sTableNewName, false);
			if (tables.first)
				obj.setFieldArray(sfTables, tables.second);
			else
				return;

			SleOps::addCommonFields(obj, _account);
			//
			obj.setFieldU16(sfOpType, T_RENAME);
			obj.setAccountID(sfAccount, _account);
			bRel = true;
		});
		if (!bRel) {
			auto j = ctx_.app.journal("Executive");
			JLOG(j.info())
				<< "SleOps dropTable,apply result: failed.";
			return bRel;
		}
		if (bTransaction_) {
			std::vector<STTx>  txs = ctx_.app.getContractHelper().getTxsByHash(txHash_);
			if (txs.size() > 0)
				txs.at(0).addSubTx(tx);
			return tesSUCCESS;
		}
		auto ret = applyDirect(ctx_.app, ctx_.view(), tx, ctx_.app.journal("SleOps"));
		if (ret != tesSUCCESS)
		{
			auto j = ctx_.app.journal("Executive");
			JLOG(j.info())
				<< "SleOps renameTable,apply result:"
				<< transToken(ret);
		}

		if (ctx_.view().flags() & tapForConsensus)
		{
			ctx_.tx.addSubTx(tx);
		}

		return ret == tesSUCCESS;
	}

	bool SleOps::grantTable(AccountID const& _account, AccountID const& _account2, std::string const& _sTableName, std::string const& _raw)
	{
		const ApplyContext &_ctx = ctx_;
		bool bRel = false;
		STTx tx(ttTABLELISTSET,
			[&_account, &_account2, &_sTableName, &_raw, &_ctx, &bRel](auto& obj)
		{
			auto tables = genTableFields(_ctx, _account, _sTableName, "", false);
			if (tables.first)
				obj.setFieldArray(sfTables, tables.second);
			else
				return;

			SleOps::addCommonFields(obj, _account);
			//
			obj.setFieldU16(sfOpType, T_GRANT);
			obj.setAccountID(sfAccount, _account);
			obj.setAccountID(sfUser, _account2);
			obj.setFieldVL(sfRaw, strCopy(_raw));
			bRel = true;
		});
		if (!bRel) {
			auto j = ctx_.app.journal("Executive");
			JLOG(j.info())
				<< "SleOps dropTable,apply result: failed.";
			return bRel;
		}
		if (bTransaction_) {
			std::vector<STTx>  txs = ctx_.app.getContractHelper().getTxsByHash(txHash_);
			if (txs.size() > 0)
				txs.at(0).addSubTx(tx);
			return tesSUCCESS;
		}
		auto ret = applyDirect(ctx_.app, ctx_.view(), tx, ctx_.app.journal("SleOps"));
		if (ret != tesSUCCESS)
		{
			auto j = ctx_.app.journal("Executive");
			JLOG(j.info())
				<< "SleOps grantTable,apply result:"
				<< transToken(ret);
		}

		if (ctx_.view().flags() & tapForConsensus)
		{
			ctx_.tx.addSubTx(tx);
		}

		return ret == tesSUCCESS;
	}

	//CRUD operation
	bool SleOps::insertData(AccountID const& _account, AccountID const& _owner, std::string const& _sTableName, std::string const& _raw)
	{
		const ApplyContext &_ctx = ctx_;
		bool bRel = false;
		STTx tx(ttSQLSTATEMENT,
			[&_account, &_owner, &_sTableName, &_raw, &_ctx, &bRel](auto& obj)
		{
			auto tables = genTableFields(_ctx, _owner, _sTableName, "", false);
			if (tables.first)
				obj.setFieldArray(sfTables, tables.second);
			else
				return;

			SleOps::addCommonFields(obj, _account);
			//
			obj.setFieldU16(sfOpType, R_INSERT);
			obj.setAccountID(sfAccount, _account);
			obj.setAccountID(sfOwner, _owner);
			obj.setFieldVL(sfRaw, strCopy(_raw));
			bRel = true;
		});
		if (!bRel) {
			auto j = ctx_.app.journal("Executive");
			JLOG(j.info())
				<< "SleOps dropTable,apply result: failed.";
			return bRel;
		}
		if (bTransaction_) {
			std::vector<STTx>  txs = ctx_.app.getContractHelper().getTxsByHash(txHash_);
			if (txs.size() > 0)
				txs.at(0).addSubTx(tx);
			return tesSUCCESS;
		}
		auto ret = applyDirect(ctx_.app, ctx_.view(), tx, ctx_.app.journal("SleOps"));
		if (ret != tesSUCCESS)
		{
			auto j = ctx_.app.journal("Executive");
			JLOG(j.info())
				<< "SleOps insertData,apply result:"
				<< transToken(ret);
		}

		if (ctx_.view().flags() & tapForConsensus)
		{
			ctx_.tx.addSubTx(tx);
		}

		return ret == tesSUCCESS;
	}

	bool SleOps::deleteData(AccountID const& _account, AccountID const& _owner, std::string const& _sTableName, std::string const& _raw)
	{
		const ApplyContext &_ctx = ctx_;
		bool bRel = false;
		STTx tx(ttSQLSTATEMENT,
			[&_account, &_owner, &_sTableName, &_raw, &_ctx, &bRel](auto& obj)
		{
			auto tables = genTableFields(_ctx, _owner, _sTableName, "", false);
			if (tables.first)
				obj.setFieldArray(sfTables, tables.second);
			else
				return;

			SleOps::addCommonFields(obj, _account);
			//
			obj.setFieldU16(sfOpType, R_DELETE);
			obj.setAccountID(sfAccount, _account);
			obj.setAccountID(sfOwner, _owner);
			std::string _sRaw = "[" + _raw + "]";
			obj.setFieldVL(sfRaw, strCopy(_sRaw));
			bRel = true;
		});
		if (!bRel) {
			auto j = ctx_.app.journal("Executive");
			JLOG(j.info())
				<< "SleOps dropTable,apply result: failed.";
			return bRel;
		}
		if (bTransaction_) {
			std::vector<STTx>  txs = ctx_.app.getContractHelper().getTxsByHash(txHash_);
			if (txs.size() > 0)
				txs.at(0).addSubTx(tx);
			return tesSUCCESS;
		}
		auto ret = applyDirect(ctx_.app, ctx_.view(), tx, ctx_.app.journal("SleOps"));
		if (ret != tesSUCCESS)
		{
			auto j = ctx_.app.journal("Executive");
			JLOG(j.info())
				<< "SleOps deleteData,apply result:"
				<< transToken(ret);
		}

		if (ctx_.view().flags() & tapForConsensus)
		{
			ctx_.tx.addSubTx(tx);
		}

		return ret == tesSUCCESS;
	}

	bool SleOps::updateData(AccountID const& _account, AccountID const& _owner, std::string const& _sTableName, std::string const& _getRaw, std::string const& _updateRaw)
	{
		const ApplyContext &_ctx = ctx_;
		bool bRel = false;
		STTx tx(ttSQLSTATEMENT,
			[&_account, &_owner, &_sTableName, &_getRaw, &_updateRaw, &_ctx, &bRel](auto& obj)
		{
			auto tables = genTableFields(_ctx, _owner, _sTableName, "", false);
			if (tables.first)
				obj.setFieldArray(sfTables, tables.second);
			else
				return;

			SleOps::addCommonFields(obj, _account);
			//
			obj.setFieldU16(sfOpType, R_UPDATE);
			obj.setAccountID(sfAccount, _account);
			obj.setAccountID(sfOwner, _owner);
			std::string _sRaw = "[" + _updateRaw + "," + _getRaw + "]";
			obj.setFieldVL(sfRaw, strCopy(_sRaw));
			bRel = true;
		});
		if (!bRel) {
			auto j = ctx_.app.journal("Executive");
			JLOG(j.info())
				<< "SleOps dropTable,apply result: failed.";
			return bRel;
		}
		if (bTransaction_) {
			std::vector<STTx>  txs = ctx_.app.getContractHelper().getTxsByHash(txHash_);
			if (txs.size() > 0)
				txs.at(0).addSubTx(tx);
			return tesSUCCESS;
		}
		auto ret = applyDirect(ctx_.app, ctx_.view(), tx, ctx_.app.journal("SleOps"));
		if (ret != tesSUCCESS)
		{
			auto j = ctx_.app.journal("Executive");
			JLOG(j.info())
				<< "SleOps deleteData,apply result:"
				<< transToken(ret);
		}

		if (ctx_.view().flags() & tapForConsensus)
		{
			ctx_.tx.addSubTx(tx);
		}

		return ret == tesSUCCESS;
	}

	//Select related
	uint256 SleOps::getDataHandle(AccountID const& _owner, std::string const& _sTableName, std::string const& _raw)
	{
		uint256 handle = uint256(0);
		//
		Json::Value jvCommand, tableJson;
		jvCommand[jss::Owner] = to_string(_owner);
		jvCommand[jss::Table] = _sTableName;
		jvCommand[jss::Raw] = _raw;
		tableJson[jss::Table][jss::TableName] = to_string(_sTableName);

		auto ledgerSeq = ctx_.app.getLedgerMaster().getValidLedgerIndex();
		auto nameInDB = ctx_.app.getLedgerMaster().getNameInDB(ledgerSeq, _owner, _sTableName);
		if (!nameInDB)
		{
			auto j = ctx_.app.journal("Executive");
			JLOG(j.info())
				<< "SleOps genTableFields getNameInDB failed,account="
				<< to_string(_owner)
				<< ",tableName="
				<< _sTableName;
		}
		else
		{
			tableJson[jss::Table][jss::NameInDB] = to_string(nameInDB);
		}
		jvCommand[jss::Tables].append(tableJson);
		//
		Resource::Charge loadType = -1;
		Resource::Consumer c;
		RPC::Context context{ ctx_.app.journal("RPCHandler"), jvCommand, ctx_.app,
			loadType,ctx_.app.getOPs(), ctx_.app.getLedgerMaster(), c, Role::ADMIN };

		Json::Value jvResult = ripple::doGetRecord(context);
		//
		handle = ctx_.app.getContractHelper().genRandomUniqueHandle();
		//
		if(!jvResult.isNull())
			ctx_.app.getContractHelper().addRecord(handle, jvResult);
		//
		return handle;
	}
	uint256 SleOps::getDataRowCount(uint256 const& _handle)
	{
		Json::Value jvRes = ctx_.app.getContractHelper().getRecord(_handle);
		if(jvRes.isNull())
			return uint256(0);
		//
		Json::Value lines = jvRes[jss::result][jss::lines];
		return uint256(lines.size());
	}
	uint256 SleOps::getDataColumnCount(uint256 const& _handle)
	{
		Json::Value::UInt size = 0;
		Json::Value jvRes = ctx_.app.getContractHelper().getRecord(_handle);
		if (jvRes.isNull())
			return uint256(size);
		//
		Json::Value lines = jvRes[jss::result][jss::lines];
		if (lines.size() > 0)
			size = lines[Json::Value::UInt(0)].size();
		return uint256(size);
	}
	std::string	SleOps::getByKey(uint256 const& _handle, size_t row, std::string const& _key)
	{
		Json::Value jvRes = ctx_.app.getContractHelper().getRecord(_handle);
		if (jvRes.isNull())
			return "";
		//
		Json::Value lines = jvRes[jss::result][jss::lines];
		if (lines.size() > row+1)
			return lines[Json::Value::UInt(row)].get(_key.data(), "").toStyledString();
		return "";
	}
	std::string	SleOps::getByIndex(uint256 const& _handle, size_t row, size_t column)
	{
		Json::Value jvRes = ctx_.app.getContractHelper().getRecord(_handle);
		if (jvRes.isNull())
			return "";
		//
		Json::Value lines = jvRes[jss::result][jss::lines];
		if (lines.size() > row + 1) {
			Json::Value rowData = lines[Json::Value::UInt(row)];
			if (rowData.size() > column + 1)
				return rowData[Json::Value::UInt(column)].toStyledString();
		}
		return "";
	}
	void	SleOps::releaseResource(uint256 const& handle)	//release handle related resources
	{
		ctx_.app.getContractHelper().releaseHandle(handle);
	}

	//transaction related
	void	SleOps::transactionBegin()
	{
		bTransaction_ = true;
		const ApplyContext &_ctx = ctx_;
		STTx tx(ttSQLTRANSACTION,
			[](auto& obj)
		{
			obj.setFieldVL(sfStatements, strCopy("[]"));
		});
		txHash_ = tx.getTransactionID();
		ctx_.app.getContractHelper().addTx(txHash_, tx);
	}

	void	SleOps::transactionCommit()
	{
		if (!bTransaction_)
		{
			auto j = ctx_.app.journal("Executive");
			JLOG(j.info())
				<< "SleOps transactionCommit failed, because no exist 'transaction Begin'.";
			return;
		}
		//
		std::vector<STTx>  txs = ctx_.app.getContractHelper().getTxsByHash(txHash_);
		if (txs.size() > 0)
		{
			STTx tx = txs.at(0);
			auto ret = applyDirect(ctx_.app, ctx_.view(), tx, ctx_.app.journal("SleOps"));
			if (ret != tesSUCCESS)
			{
				auto j = ctx_.app.journal("Executive");
				JLOG(j.info())
					<< "SleOps createTable,apply result:"
					<< transToken(ret);
			}

			if (ctx_.view().flags() & tapForConsensus)
			{
				ctx_.tx.addSubTx(tx);
				//ctx_.app.getContractHelper().addTx(ctx_.tx.getTransactionID(), tx);
			}
		}
		//
		bTransaction_ = false;
		txHash_ = uint256(0);
	}    
}