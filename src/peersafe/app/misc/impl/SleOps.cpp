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


namespace ripple {
    //just raw function for zxc, all paras should be tranformed in extvmFace modules.

    SleOps::SleOps(ApplyContext& ctx)
        :ctx_(ctx)
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

		obj.setFieldU16(sfSequence, 0);
		obj.setFieldAmount(sfFee, STAmount());
		obj.setFieldVL(sfSigningPubKey, Slice(nullptr, 0));
	}

	std::pair<bool, STArray> SleOps::genTableFields(AccountID const& _account, std::string _sTableName, std::string _tableNewName, bool bNewNameInDB)
	{
		STArray tables;
		STObject table(sfTable);
		table.setFieldVL(sfTableName, strCopy(_sTableName));
		if(!_tableNewName.empty())
			table.setFieldVL(sfTableNewName, strCopy(_tableNewName));
		if (bNewNameInDB)
		{
			auto nameInDB = generateNameInDB(ctx_.view().seq(), _account, _sTableName);
			table.setFieldVL(sfNameInDB, strCopy(to_string(nameInDB)));
		}
		else
		{
			auto ledgerSeq = ctx_.app.getLedgerMaster().getValidLedgerIndex();
			auto nameInDB = ctx_.app.getLedgerMaster().getNameInDB(ledgerSeq, _account, _sTableName);
			if (!nameInDB)
			{
				auto j = ctx_.app.journal("Executive");
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

	//table opeartion
	bool SleOps::createTable(AccountID const& _account, std::string const& _sTableName, std::string const& _raw)
	{
		STTx tx(ttTABLELISTSET,
			[&_account, &_sTableName, &_raw](auto& obj)
		{

			obj.setFieldU16(sfOpType, T_CREATE);
			obj.setFieldVL(sfRaw, strCopy(_raw));
			auto tables = genTableFields(_account, _sTableName, "", true);
			if (tables.first)
				obj.setFieldArray(sfTables, tables);
			else
				return false;

			addCommonFields(obj, _account);
		});
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
		return true;
	}

	bool SleOps::renameTable(AccountID const& _account, std::string const& _sTableName, std::string const& _sTableNewName)
	{
		return true;
	}

	bool SleOps::grantTable(AccountID const& _account, AccountID const& _account2, std::string const& _raw)
	{
		return true;
	}

	//CRUD operation
	bool SleOps::insertData(AccountID const& _account, AccountID const& _owner, std::string const& _sTableName, std::string const& _raw)
	{
		return true;
	}

	bool SleOps::deleteData(AccountID const& _account, AccountID const& _owner, std::string const& _sTableName, std::string const& _raw)
	{
		return true;
	}

	bool SleOps::updateData(AccountID const& _account, AccountID const& _owner, std::string const& _sTableName, std::string const& _getRaw, std::string const& _updateRaw)
	{
		return true;
	}

	//Select related
	uint256 SleOps::getDataHandle(AccountID const& _owner, std::string const& _sTableName, std::string const& _raw)
	{
		return uint256(0);
	}
	uint256 SleOps::getDataLines(uint256 const& _handle)
	{
		return uint256(0);
	}
	uint256 SleOps::getDataColumns(uint256 const& _handle)
	{
		return uint256(0);
	}
	bytes	SleOps::getByKey(uint256 const& _handle, size_t row, std::string const& _key)
	{
		return Blob();
	}
	bytes	SleOps::getByIndex(uint256 const& handle, size_t row, size_t column)
	{
		return Blob();
	}
	void	SleOps::releaseResource()	//release hanle related resources
	{

	}

	//transaction related
	void	SleOps::transactionBegin()
	{

	}
	void	SleOps::transactionCommit()
	{

	}    
}