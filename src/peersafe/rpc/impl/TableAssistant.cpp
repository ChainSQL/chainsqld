//------------------------------------------------------------------------------
/*
 This file is part of chainsqld: https://github.com/chainsql/chainsqld
 Copyright (c) 2016-2018 Peersafe Technology Co., Ltd.
 
	chainsqld is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.
 
	chainsqld is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.
	You should have received a copy of the GNU General Public License
	along with cpp-ethereum.  If not, see <http://www.gnu.org/licenses/>.
 */
//==============================================================================

#include <BeastConfig.h>
#include <ripple/app/ledger/LedgerMaster.h>
#include <ripple/app/ledger/TransactionMaster.h>
#include <ripple/protocol/JsonFields.h>
#include <ripple/rpc/impl/RPCHelpers.h>
#include <ripple/net/RPCErr.h>
#include <ripple/protocol/digest.h>
#include <ripple/protocol/RippleAddress.h>

#include <ripple/protocol/STTx.h>
#include <ripple/core/JobQueue.h>
#include <ripple/json/json_reader.h>
#include <peersafe/rpc/impl/TableAssistant.h>
#include <peersafe/rpc/TableUtils.h>
#include <peersafe/protocol/TableDefines.h>
#include <peersafe/rpc/impl/TxCommonPrepare.h>
#include <peersafe/rpc/impl/TxTransactionPrepare.h>
#include <peersafe/rpc/TableUtils.h>


namespace ripple {

//#define EXPIRE_TIME 60 * 60
#define EXPIRE_TIME 300


TableAssistant::TableAssistant(Application& app, Config& cfg, beast::Journal journal)
	: app_(app)
	, journal_(journal)
	, cfg_(cfg)
{
	bTableCheckHashThread_ = false;
}

uint256 TableAssistant::getCheckHash(uint160 nameInDB)
{	
	std::lock_guard<std::mutex> lock(mutexMap_);
	auto it = m_map.find(nameInDB);
	if (it != m_map.end())
		return it->second->uTxCheckHash;
	else
		return beast::zero;
}

Json::Value TableAssistant::prepare(const std::string& secret,const std::string& publickey, Json::Value& tx_json, bool ws)
{
	//judge if sql_transaction
	auto func = std::bind(&TableAssistant::getCheckHash, this, std::placeholders::_1);
	if (tx_json.isMember(jss::TransactionType) && tx_json[jss::TransactionType].asString() == "SQLTransaction")
	{
		auto txPrepare = std::make_shared<TxTransactionPrepare>(app_,secret,publickey,tx_json,func,ws);
		return txPrepare->prepare();
	}
	else
	{
		auto txPrepare = std::make_shared<TxCommonPrepare>(app_,secret,publickey,tx_json,func,ws);
		return txPrepare->prepare();
	}
}

Json::Value TableAssistant::getDBName(const std::string& accountIdStr, const std::string& tableNameStr)
{
	Json::Value ret(Json::objectValue);
	ripple::AccountID accountID;
	auto jvAccepted = RPC::accountFromString(accountID, accountIdStr, false);

	if (jvAccepted)
		return jvAccepted;

	//first,we query from ledgerMaster
	auto nameInDB = app_.getLedgerMaster().getNameInDB(app_.getLedgerMaster().getValidLedgerIndex(), accountID, tableNameStr);
	if (!nameInDB) //not exist,then generate nameInDB
	{
		uint32_t ledgerSequence = 0;
		auto ledger = app_.getLedgerMaster().getValidatedLedger();
		if (ledger)
			ledgerSequence = ledger->info().seq;
		else
		{
			return rpcError(rpcGET_LGR_FAILED);
		}

		try
		{
			nameInDB = generateNameInDB(ledgerSequence, accountID, tableNameStr);
		}
		catch (std::exception const& e)
		{
			return RPC::make_error(rpcGENERAL, e.what());
		}
	}
	ret["nameInDB"] = to_string(nameInDB);
	return ret;
}

bool TableAssistant::Put(STTx const& tx)
{
	if (tx.getTxnType() == ttSQLTRANSACTION)
	{
		Blob txs_blob = tx.getFieldVL(sfStatements);
		std::string txs_str;

		ripple::AccountID accountID = tx.getAccountID(sfAccount);

		txs_str.assign(txs_blob.begin(), txs_blob.end());
		Json::Value objs;
		Json::Reader().parse(txs_str, objs);

		for (auto obj : objs)
		{
			auto tx_pair = STTx::parseSTTx(obj, accountID);
			if (tx_pair.first == nullptr)
			{
				auto j = app_.journal("TableAssistant");
				JLOG(j.debug())
					<< "Parse STTx error: " << tx_pair.second;
				return false;
			}

			if (!PutOne(*tx_pair.first, tx.getTransactionID()))
				return false;
		}
		return true;
	}
	else
	{
		return PutOne(tx, tx.getTransactionID());
	}
}

bool TableAssistant::PutOne(STTx const& tx, const uint256 &uHash)
{
    if (!isStrictModeOpType((TableOpType)tx.getFieldU16(sfOpType)))     return true;
 
	auto pTx = std::make_shared<txInfo>();

	pTx->uTxHash = uHash;

	if (tx.isFieldPresent(sfLastLedgerSequence))
		pTx->uTxLedgerVersion = tx.getFieldU32(sfLastLedgerSequence);  //uTxLedgerVersion
	if (pTx->uTxLedgerVersion <= 0)
	{
		pTx->uTxLedgerVersion = app_.getLedgerMaster().getValidLedgerIndex() + 5;
	}

	pTx->bStrictMode = tx.isFieldPresent(sfTxCheckHash);

	std::lock_guard<std::mutex> lock(mutexMap_);

	uint256 hashNew;
    if (tx.isFieldPresent(sfTables))
    {
        auto const & sTxTables = tx.getFieldArray(sfTables);
        uint160 uTxDBName = sTxTables[0].getFieldH160(sfNameInDB);
        auto it = m_map.find(uTxDBName);
        if (it == m_map.end())
        {
            std::string sTxTableName = strCopy(sTxTables[0].getFieldVL(sfTableName));
            ripple::AccountID ownerID;
            if (tx.isFieldPresent(sfOwner))  ownerID = tx.getAccountID(sfOwner);
            else                            ownerID = tx.getAccountID(sfAccount);

            auto ret = app_.getLedgerMaster().getLatestTxCheckHash(ownerID, sTxTableName);

            auto pCheck = std::make_shared<checkInfo>();
            pCheck->timer = clock_type::now();
            pCheck->sTableName = sTxTableName;
            pCheck->uTxBackupHash = ret.first;
            pCheck->accountID = ownerID;

            if (tx.getFieldU16(sfOpType) == T_CREATE)   hashNew = sha512Half(makeSlice(strCopy(tx.getFieldVL(sfRaw))));
            else
            {
                hashNew = sha512Half(makeSlice(strCopy(tx.getFieldVL(sfRaw))), pCheck->uTxBackupHash);
                if (tx.isFieldPresent(sfTxCheckHash))
                {
                    if (hashNew != tx.getFieldH256(sfTxCheckHash))	return false;
                }
            }
            pCheck->uTxCheckHash = hashNew;

            pTx->uTxCheckHash = hashNew;
            pCheck->listTx.push_back(pTx);

            auto iter_pair = m_map.insert(std::make_pair(uTxDBName, pCheck));
            assert(iter_pair.second);
            return iter_pair.second;
        }
        else
        {
            hashNew = sha512Half(makeSlice(strCopy(tx.getFieldVL(sfRaw))), it->second->uTxCheckHash);
            if (tx.isFieldPresent(sfTxCheckHash))
            {
                if (hashNew != tx.getFieldH256(sfTxCheckHash))	return false;
            }
            it->second->uTxCheckHash = hashNew;
            pTx->uTxCheckHash = hashNew;
            it->second->listTx.push_back(pTx);
        }
    }
	return true;
}

void TableAssistant::TryTableCheckHash()
{
	if (!bTableCheckHashThread_)
	{
		bTableCheckHashThread_ = true;
		app_.getJobQueue().addJob(jtTableCheckHash, "tableCheckHash", [this](Job&) { TableCheckHashThread(); });
	}
}

void TableAssistant::TableCheckHashThread()
{
	std::lock_guard<std::mutex> lock(mutexMap_);
	auto ledger = app_.getLedgerMaster().getValidatedLedger();
	auto iter = m_map.begin();
	while (iter != m_map.end())
	{
		auto sleHashRet = app_.getLedgerMaster().getLatestTxCheckHash(iter->second->accountID, iter->second->sTableName);
		auto &uCheckHash = sleHashRet.first;

        if (uCheckHash.isZero())
        {
            iter = m_map.erase(iter);
            continue;
        }

		auto &pListTx = iter->second->listTx;
		if (uCheckHash == iter->second->uTxBackupHash)
		{	
			auto itFind = std::find_if(pListTx.begin(), pListTx.end(),
				[uCheckHash,ledger, this](std::shared_ptr<txInfo> const &pItem) {				
				return !ledger->txMap().hasItem(pItem->uTxHash) && pItem->uTxLedgerVersion <= app_.getLedgerMaster().getValidLedgerIndex();
			});

			if (itFind != pListTx.end())
			{
				pListTx.erase(itFind, pListTx.end());				
			}			

			if (pListTx.size() <= 0)
			{
				iter->second->uTxCheckHash = uCheckHash;
			}
			else
			{
				iter->second->uTxCheckHash = (*pListTx.rbegin())->uTxCheckHash;
			}
		}
		else
		{			
			auto itFind = std::find_if(pListTx.begin(), pListTx.end(),
				[uCheckHash,ledger](std::shared_ptr<txInfo> const& pItem){
				return pItem->uTxCheckHash == uCheckHash && ledger->txMap().hasItem(pItem->uTxHash);
			});

			if (itFind != pListTx.end())
			{
				auto uFindTxHash = (*itFind)->uTxHash;
				//please consider transaction Tx, may have more than one uFindTxHash in the list
				pListTx.erase(pListTx.begin(),itFind);
				pListTx.remove_if([uFindTxHash](std::shared_ptr<txInfo> const& pItem){
					return pItem->uTxHash == uFindTxHash;
				});
			}
			else
			{
				pListTx.clear();
				iter->second->uTxCheckHash = uCheckHash;				
			}
			iter->second->uTxBackupHash = uCheckHash;
		}
		
		if(pListTx.size() != 0)
		{			
			iter->second->timer = clock_type::now();
			iter++;
		}			
		else
		{
			auto now = clock_type::now();
			using duration_type = std::chrono::duration<double>;
			duration_type time_span = std::chrono::duration_cast<duration_type>(now - iter->second->timer);
			if (time_span.count() > EXPIRE_TIME)
			{
				iter = m_map.erase(iter);
			}
			else
			{				
				iter++;
			}
		}
	}
	bTableCheckHashThread_ = false;
}

}
