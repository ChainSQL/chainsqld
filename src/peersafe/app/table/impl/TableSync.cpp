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

#include <ripple/app/ledger/LedgerMaster.h>
#include <ripple/app/ledger/TransactionMaster.h>
#include <ripple/app/ledger/AcceptedLedger.h>
#include <ripple/core/JobQueue.h>
#include <ripple/core/ConfigSections.h>
#include <ripple/overlay/Overlay.h>
#include <boost/optional/optional_io.hpp>
#include <ripple/core/DatabaseCon.h>
#include <ripple/json/json_reader.h>
#include <ripple/app/misc/NetworkOPs.h>
#include <peersafe/protocol/STEntry.h>
#include <peersafe/app/table/TableSync.h>
#include <peersafe/app/table/TableStatusDB.h>
#include <peersafe/app/sql/STTx2SQL.h>
#include <peersafe/app/sql/TxStore.h>
#include <peersafe/protocol/TableDefines.h>
#include <peersafe/app/util/TableSyncUtil.h>

namespace ripple {
TableSync::TableSync(Application& app, Config& cfg, beast::Journal journal)
    : app_(app)
    , journal_(journal)
    , cfg_(cfg)    
    , checkSkipNode_("SkipNode", 65536, 450, stopwatch(),
        app_.journal("TaggedCache"))
{
    bTableSyncThread_ = false;
    bLocalSyncThread_ = false;
    bInitTableItems_  = false;

    if (app.getTxStoreDBConn().GetDBConn() == nullptr || 
		app.getTxStoreDBConn().GetDBConn()->getSession().get_backend() == nullptr)
        bIsHaveSync_ = false;
    else
        bIsHaveSync_ = true;

    auto sync_section = cfg_.section(ConfigSection::autoSync());
    if (sync_section.values().size() > 0)
    {
        auto value = sync_section.values().at(0);
        bAutoLoadTable_ = atoi(value.c_str());
    }
    else
        bAutoLoadTable_ = false;

	auto press_switch = cfg_.section(ConfigSection::pressSwitch());
	if (press_switch.values().size() > 0)
	{
		auto value = press_switch.values().at(0);
		bPressSwitchOn_ = atoi(value.c_str());
	}
	else
		bPressSwitchOn_ = false;
}

TableSync::~TableSync()
{
}

void TableSync::GetTxRecordInfo(LedgerIndex iCurSeq, AccountID accountID, std::string sTableName, LedgerIndex &iLastSeq, uint256 &hash)
{
    auto ledger = app_.getLedgerMaster().getLedgerBySeq(iCurSeq);
    if (ledger == NULL)  return;

    auto key = keylet::table(accountID);

    auto const tablesle = ledger->read(key);
    if (!tablesle)                 return;

    const STArray & aTables = tablesle->getFieldArray(sfTableEntries);
    if (aTables.size() <= 0)       return ;

    auto iter(aTables.end());
    iter = std::find_if(aTables.begin(), aTables.end(),
        [iLastSeq, accountID, sTableName](STObject const &item) {
        uint160 uTxDBName = item.getFieldH160(sfNameInDB);
        auto sTxDBName = to_string(uTxDBName);
        return sTxDBName == sTableName;
    });
    if (iter == aTables.end())     return;

    iLastSeq = iter->getFieldU32(sfTxnLgrSeq);
    hash = iter->getFieldH256(sfTxnLedgerHash);
}

std::vector <uint256> TableSync::getTxsFromDb(uint32 TxnLgrSeq, std::string /*sAccountID*/)
{
    std::vector <uint256> txs;

    static std::string const prefix(
        R"(select TransID from Transactions WHERE )");

    std::string sql = boost::str(boost::format(
        prefix +
        (R"(LedgerSeq = '%d' and (TransType = 'SQLStatement' or TransType = 'TableListSet' or TransType = 'SQLTransaction');)"))
        % TxnLgrSeq);

    std::string stxnHash;
    {
        auto db = app_.getTxnDB().checkoutDb();

        soci::statement st = (db->prepare << sql,
            soci::into(stxnHash));
  
        st.execute();

        while (st.fetch())
        {
            txs.push_back(from_hex_text<uint256>(stxnHash));
        }
    }
    return txs;
}

bool TableSync::MakeTableDataReply(std::string sAccountID, bool bStop, uint32_t time, std::string sNickName, TableSyncItem::SyncTargetType eTargeType, LedgerIndex TxnLgrSeq, uint256 TxnLgrHash, LedgerIndex PreviousTxnLgrSeq, uint256 PrevTxnLedgerHash,std::string sNameInDB, protocol::TMTableData &m)
{
	m.set_tablename(sNameInDB);
	m.set_ledgerseq(TxnLgrSeq);
	m.set_lastledgerseq(PreviousTxnLgrSeq);
	m.set_lastledgerhash(to_string(PrevTxnLedgerHash));
	m.set_ledgercheckhash(to_string(TxnLgrHash));
    m.set_seekstop(bStop);
    m.set_account(sAccountID);    
    m.set_closetime(time);
	m.set_etargettype(eTargeType);
    m.set_nickname(sNickName);

    protocol::TMLedgerData ledgerData;
    auto ledger = app_.getLedgerMaster().getLedgerBySeq(TxnLgrSeq);

    std::vector<protocol::TMLedgerNode> node_vec;
    if (ledger)
    {
        m.set_ledgerhash(to_string(ledger->info().hash));

        std::shared_ptr<AcceptedLedger> alpAccepted =
            app_.getAcceptedLedgerCache().fetch(ledger->info().hash);
        if (!alpAccepted)
        {
            alpAccepted = std::make_shared<AcceptedLedger>(
                ledger, app_.accountIDCache(), app_.logs());
            app_.getAcceptedLedgerCache().canonicalize(
                ledger->info().hash, alpAccepted);
        }

        bool bHasTX = false;
        for (auto const& vt : alpAccepted->getMap())
        {
            if (vt.second->getResult() != tesSUCCESS)
            {
                continue;
            }

            std::shared_ptr<const STTx> stTx = vt.second->getTxn();

            if (!stTx->isChainSqlTableType() && stTx->getTxnType() != ttCONTRACT)  continue;

			std::vector<STTx> vecTxs = app_.getMasterTransaction().getTxs(*stTx, sNameInDB,ledger,0);
			if(vecTxs.size() == 0)
				continue;

   //         if (stTx.getTxnType() == ttSQLTRANSACTION) {
   //             Blob txs_blob = stTx.getFieldVL(sfStatements);
   //             std::string txs_str;

   //             txs_str.assign(txs_blob.begin(), txs_blob.end());
   //             Json::Value objs;
   //             Json::Reader().parse(txs_str, objs);

			//	bool bFound = false;
   //             for (auto obj : objs)
   //             {
   //                 auto const & sTxTable = obj["Tables"][0u]["Table"];
   //                 if (sNameInDB == sTxTable["NameInDB"].asString())
   //                 {
			//			bFound = true;
			//			break;
   //                 }
   //             }
			//	if (!bFound) continue;;
   //         }
			//else if (stTx.getTxnType() == ttCONTRACT)
			//{
			//	auto rawMeta = ledger->txRead(stTx.getTransactionID()).second;
			//	if(!rawMeta)
			//		continue;

			//	auto txMeta = std::make_shared<TxMeta>(stTx.getTransactionID(),
			//		ledger->seq(), *rawMeta, app_.journal("TableSync"));

			//	auto meta = txMeta->getNodes().back();
			//	if (!meta.isFieldPresent(sfContractTxs))
			//		continue;
			//	auto txs = meta.getFieldArray(sfContractTxs);

			//	bool bFound = false;
			//	for (auto txInner : txs)
			//	{
			//		if (txInner.getFieldU16(sfTransactionType) == ttTABLELISTSET || 
			//			txInner.getFieldU16(sfTransactionType) == ttSQLSTATEMENT)
			//		{
			//			auto const & sTxTables = txInner.getFieldArray(sfTables);
			//			if (sNameInDB == to_string(sTxTables[0].getFieldH160(sfNameInDB)))
			//			{
			//				bFound = true;
			//				break;
			//			}
			//		}
			//		else if (txInner.getFieldU16(sfTransactionType) == ttSQLTRANSACTION)
			//		{
			//			Blob txs_blob = txInner.getFieldVL(sfStatements);
			//			std::string txs_str;

			//			txs_str.assign(txs_blob.begin(), txs_blob.end());
			//			Json::Value objs;
			//			Json::Reader().parse(txs_str, objs);

			//			for (auto obj : objs)
			//			{
			//				auto const & sTxTable = obj["Tables"][0u]["Table"];
			//				if (sNameInDB == sTxTable["NameInDB"].asString())
			//				{
			//					bFound = true;
			//					break;
			//				}
			//			}
			//			if (bFound)
			//				break;
			//		}
			//	}

			//	if(!bFound)
			//		continue;
			//}
   //         else 
			//{
   //             auto const & sTxTables = stTx.getFieldArray(sfTables);
   //             if (sNameInDB != to_string(sTxTables[0].getFieldH160(sfNameInDB)))
   //             {
   //                 continue;
   //             }
   //         }

            protocol::TMLedgerNode* node = m.add_txnodes();

            Serializer s;
            (*stTx).add(s);

            node->set_nodedata(s.data(), s.size());

            bHasTX = true;
        }

        if (!bHasTX)
        {
            JLOG(journal_.error()) << "in MakeTableDataReply, no tx, ledger : " << TxnLgrSeq
                << " lashTxChecHash : " << to_string(TxnLgrHash)
                << " nameInDB : " << sNameInDB;
        }
    }
    else  //if not find ,do what?replay error?
    {
        JLOG(journal_.error()) << "in MakeTableDataReply, no ledger : " << TxnLgrSeq
            << " lashTxChecHash : " << to_string(TxnLgrHash)
            << " nameInDB : " << sNameInDB;
    }

    return true;
}

bool TableSync::SendSeekResultReply(std::string sAccountID, bool bStop, uint32 time, std::weak_ptr<Peer> const& wPeer, std::string sNickName , TableSyncItem::SyncTargetType eTargeType, LedgerIndex TxnLgrSeq, uint256 TxnLgrHash, LedgerIndex PreviousTxnLgrSeq, uint256 PrevTxnLedgerHash, std::string sNameInDB)
{
    protocol::TMTableData reply;

    if (!MakeTableDataReply(sAccountID, bStop, time, sNickName,eTargeType,TxnLgrSeq,TxnLgrHash,PreviousTxnLgrSeq,PrevTxnLedgerHash,sNameInDB , reply))
		return false;
	
    Message::pointer oPacket = std::make_shared<Message>(
        reply, protocol::mtTABLE_DATA);
  
    auto peer = wPeer.lock();
    if(peer != NULL)
    {
        peer->send(oPacket);
        return true;
    }
        
    return false;    
}
bool TableSync::MakeSeekEndReply(LedgerIndex iSeq, uint256 hash, LedgerIndex iLastSeq, uint256 lastHash, uint256 checkHash, std::string account, std::string tablename, std::string sNickName, uint32_t time, TableSyncItem::SyncTargetType eTargeType, protocol::TMTableData &reply)
{
    reply.set_ledgerhash(to_string(hash));
    reply.set_ledgerseq(iSeq);
    reply.set_lastledgerseq(iLastSeq);
    reply.set_lastledgerhash(to_string(lastHash));
    reply.set_seekstop(true);
    reply.set_account(account);
    reply.set_tablename(tablename);
    reply.set_ledgercheckhash(to_string(checkHash));
    reply.set_closetime(time);
	reply.set_etargettype(eTargeType);
    reply.set_nickname(sNickName);
    return true;
}

bool TableSync::SendSeekEndReply(LedgerIndex iSeq, uint256 hash, LedgerIndex iLastSeq, uint256 lastHash, uint256 checkHash, std::string account, std::string tablename, std::string nickName, uint32_t time, TableSyncItem::SyncTargetType eTargeType, std::weak_ptr<Peer> const& wPeer)
{
    bool ret = false;
    
    protocol::TMTableData reply;

    ret = MakeSeekEndReply(iSeq, hash, iLastSeq, lastHash, checkHash, account, tablename, nickName, time, eTargeType, reply);
    if (!ret)  return false;

    Message::pointer oPacket = std::make_shared<Message>(
        reply, protocol::mtTABLE_DATA);
    
    auto peer = wPeer.lock();
    if (peer)
    {
        peer->send(oPacket);
        ret = true;
    }
    return ret;
}
//not exceed 256 ledger every check time
void TableSync::SeekTableTxLedger(TableSyncItem::BaseInfo &stItemInfo, 
	TaggedCache<LedgerIndex, std::map<AccountID, std::shared_ptr<const ripple::SLE>>>& cache)
{    
    std::shared_ptr <TableSyncItem> pItem = GetRightItem(stItemInfo.accountID, stItemInfo.sTableNameInDB, stItemInfo.sNickName, stItemInfo.eTargetType);
    if (pItem == NULL) return;

    pItem->SetIsDataFromLocal(true);

    auto lastLedgerSeq = stItemInfo.u32SeqLedger;
    auto lastLedgerHash = stItemInfo.uHash;
    auto lashTxChecHash = stItemInfo.uTxHash;

    auto  lastTxChangeIndex = stItemInfo.uTxSeq;
    auto key = keylet::table(stItemInfo.accountID);

    auto blockCheckIndex = stItemInfo.u32SeqLedger;
    bool bSendEnd = false;
    LedgerIndex curLedgerIndex = 0;
    uint256 curLedgerHash;
    uint32 time = 0;

    for (int i = stItemInfo.u32SeqLedger + 1; i <= app_.getLedgerMaster().getPublishedLedger()->info().seq; i++)
    {
        if (!app_.getLedgerMaster().haveLedger(i))   
        {
            JLOG(journal_.info()) << "in local seekLedger, no ledger : " << i
                << " lashTxChecHash : " << lashTxChecHash
                << " nameInDB : " << stItemInfo.sTableNameInDB;
            break;
        }

        //check the 256th first , if no ,continue
        if (i > blockCheckIndex)
        {
            LedgerIndex uStopIndex = std::min(getCandidateLedger(i), app_.getLedgerMaster().getPublishedLedger()->info().seq);
            if (app_.getLedgerMaster().haveLedger(i, uStopIndex))
            {
                auto ledger = app_.getLedgerMaster().getLedgerBySeq(uStopIndex);
				if (ledger)
				{
					std::pair<bool, STEntry*> retPair;
					std::shared_ptr<const ripple::SLE> tablesle = nullptr;
					auto pMapTables = cache.fetch(uStopIndex).get();
					if (pMapTables == nullptr)
					{
						tablesle = ledger->read(key);
						auto pMapSle = std::make_shared<std::map<AccountID, std::shared_ptr<const ripple::SLE>>>();
						pMapSle->emplace(std::make_pair(stItemInfo.accountID,tablesle));
						cache.canonicalize(uStopIndex, pMapSle);
					}
					else
					{
						auto& mapTables = *pMapTables;
						if (mapTables.find(stItemInfo.accountID) != mapTables.end())
						{
							tablesle = mapTables[stItemInfo.accountID];
						}
						else
						{
							tablesle = ledger->read(key);
							mapTables.emplace(std::make_pair(stItemInfo.accountID, tablesle));
						}
					}
					
					if (tablesle)
					{
						auto & aTables = tablesle->getFieldArray(sfTableEntries);
						if (aTables.size() > 0)
						{
							retPair = TableSyncUtil::IsTableSLEChanged(aTables, lastTxChangeIndex, stItemInfo.sTableNameInDB, false);

						}
					}

					
					time = ledger->info().closeTime.time_since_epoch().count();

					//table sle found,but not changed, only update LedgerSeq in SyncTableState to uStopIndex.
					if (retPair.second == NULL && retPair.first)
					{
						i = uStopIndex;

						std::shared_ptr <protocol::TMTableData> pData = std::make_shared<protocol::TMTableData>();
             
						MakeSeekEndReply(uStopIndex, ledger->info().hash, lastLedgerSeq, lastLedgerHash, lashTxChecHash, to_string(stItemInfo.accountID), stItemInfo.sTableNameInDB, stItemInfo.sNickName,time, pItem->TargetType(), *pData);
						SendData(pItem, pData);

						lastLedgerSeq = i;
						lastLedgerHash = ledger->info().hash;

						bSendEnd = true; 
                        JLOG(journal_.info()) << "in local seekLedger, this ledger does not include tx : " << uStopIndex
                            << " lashTxChecHash : " << lashTxChecHash
                            << " nameInDB : " << stItemInfo.sTableNameInDB;                            
						continue;
					}

					blockCheckIndex = uStopIndex;     
				}
            }
        }
        
        bSendEnd = false;

        auto ledger = app_.getLedgerMaster().getLedgerBySeq(i);
        if (ledger == NULL)
        {
            bSendEnd = true;
            JLOG(journal_.info()) << "in local seekLedger, no ledger in second check, ledger : " << i 
                << " lashTxChecHash : " << lashTxChecHash
                << " nameInDB : " << stItemInfo.sTableNameInDB;
            break;
        }

		time = ledger->info().closeTime.time_since_epoch().count();
		std::pair<bool, STEntry*> retPair;

		std::shared_ptr<const ripple::SLE> tablesle = nullptr;
		auto pMapTables = cache.fetch(i).get();
		if (pMapTables != nullptr && pMapTables->find(stItemInfo.accountID) != pMapTables->end())
		{
			tablesle = (*pMapTables)[stItemInfo.accountID];
		}
		else
		{
			tablesle = ledger->read(key);
		}
       
        if (tablesle)
        {
            auto & aTables = tablesle->getFieldArray(sfTableEntries);
            if (aTables.size() > 0)
            {
				retPair = TableSyncUtil::IsTableSLEChanged(aTables, lastTxChangeIndex, stItemInfo.sTableNameInDB, true);
            }
        }

        if (retPair.second != NULL || !retPair.first)
        {
            std::shared_ptr <protocol::TMTableData> pData = std::make_shared<protocol::TMTableData>();
            protocol::TMTableData reply;
           
			bool bRet = false;
			if (retPair.second != NULL)
			{
				STEntry* pEntry = retPair.second;
				auto TxnLgrSeq = pEntry->getFieldU32(sfTxnLgrSeq);
				auto TxnLgrHash = pEntry->getFieldH256(sfTxnLedgerHash);
				auto PreviousTxnLgrSeq = pEntry->getFieldU32(sfPreviousTxnLgrSeq);
				auto PrevTxnLedgerHash = pEntry->getFieldH256(sfPrevTxnLedgerHash);
				auto uTxDBName = pEntry->getFieldH160(sfNameInDB);
				bRet = MakeTableDataReply(toBase58(stItemInfo.accountID), false, time, stItemInfo.sNickName, pItem->TargetType(), TxnLgrSeq, TxnLgrHash, PreviousTxnLgrSeq, PrevTxnLedgerHash,to_string(uTxDBName), *pData);
				lashTxChecHash = TxnLgrHash;
			}
			else
			{
				bRet = MakeTableDataReply(toBase58(stItemInfo.accountID), false, time, stItemInfo.sNickName, pItem->TargetType(),i, ledger->info().hash, lastTxChangeIndex, lashTxChecHash,pItem->TableNameInDB(), *pData);
				lashTxChecHash = ledger->info().hash;

			}
            if (!bRet)
            {
                JLOG(journal_.info()) << "in local seekLedger, fail to MakeTableDataReply, ledger : " << i
                    << " lashTxChecHash : " << lashTxChecHash
                    << " nameInDB : " << stItemInfo.sTableNameInDB;
                break;
            }
            SendData(pItem, pData);

            lastLedgerSeq = i;
            lastTxChangeIndex = i;
            lastLedgerHash = ledger->info().hash;
            //lashTxChecHash = ledger->info().hash;

			//table dropped,break
			if (!retPair.first)
				break;
        }

        curLedgerIndex = i;
        curLedgerHash = ledger->info().hash;

        if (getCandidateLedger(i) == i)
        {
            std::shared_ptr <protocol::TMTableData> pData = std::make_shared<protocol::TMTableData>();
            MakeSeekEndReply(curLedgerIndex, curLedgerHash, lastLedgerSeq, lastLedgerHash, lashTxChecHash, to_string(stItemInfo.accountID), stItemInfo.sTableNameInDB, stItemInfo.sNickName,time, pItem->TargetType(), *pData);

            SendData(pItem, pData);

            JLOG(journal_.info()) << "in local seekLedger, getCandidateLedger, end reply , ledger : " << i
                << " lashTxChecHash : " << lashTxChecHash
                << " nameInDB : " << stItemInfo.sTableNameInDB;
        }

        bSendEnd = false;
    }

    if (!bSendEnd && curLedgerIndex != 0)
    {
        std::shared_ptr <protocol::TMTableData> pData = std::make_shared<protocol::TMTableData>();
        MakeSeekEndReply(curLedgerIndex, curLedgerHash, lastLedgerSeq, lastLedgerHash, lashTxChecHash, to_string(stItemInfo.accountID), stItemInfo.sTableNameInDB, stItemInfo.sNickName, time, pItem->TargetType(), *pData);

        SendData(pItem, pData);

        JLOG(journal_.info()) << "in local seekLedger, end of fun , ledger : " << curLedgerIndex
            << " lashTxChecHash : " << lashTxChecHash
            << " nameInDB : " << stItemInfo.sTableNameInDB;
    }   

    pItem->SetIsDataFromLocal(false);

    pItem->SetSyncState(TableSyncItem::SYNC_BLOCK_STOP);
}

void TableSync::SeekTableTxLedger(std::shared_ptr <protocol::TMGetTable> const& m, std::weak_ptr<Peer> const& wPeer)
{
    bool bGetLost = m->getlost();

    LedgerIndex checkIndex = m->ledgerseq();
    LedgerIndex lastTxChangeIndex = 0;
    LedgerIndex stopIndex = m->ledgerstopseq(); 
    LedgerIndex iLastFindSeq = m->ledgerseq();  
	TableSyncItem::SyncTargetType eTargetType = (TableSyncItem::SyncTargetType)(m->etargettype());
    std::string sNickName = m->has_nickname() ? m->nickname() : "";

    uint256 checkHash, uLashFindHash, lastTxChangeHash;
    if(m->has_ledgerhash())
        checkHash = from_hex_text<uint256>(m->ledgerhash().data());
    uLashFindHash = checkHash;
    
    //check the seq and the hash is valid
    if (!app_.getLedgerMaster().haveLedger(checkIndex))  return;
    if (checkHash.isNonZero() && app_.getLedgerMaster().getHashBySeq(checkIndex) != checkHash) return;

    AccountID ownerID(*ripple::parseBase58<AccountID>(m->account()));

    lastTxChangeIndex = m->ledgercheckseq();
    lastTxChangeHash = from_hex_text<uint256>(m->ledgercheckhash());
    //GetTxRecordInfo(checkIndex, ownerID, m->tablename(), lastTxChangeIndex, lastTxChangeHash);

    //find from the next one
    checkIndex++;

    //check the 256th ledger at first
    LedgerIndex iBlockEnd = getCandidateLedger(checkIndex);
    if (stopIndex == 0) stopIndex = iBlockEnd;

    auto key = keylet::table(ownerID);

    if (app_.getLedgerMaster().haveLedger(checkIndex, stopIndex))
    {
        auto ledger = app_.getLedgerMaster().getLedgerBySeq(stopIndex);        
		std::pair<bool, STEntry*> retPair;
        auto tablesle = ledger->read(key);
        if (tablesle)
        {
            auto & aTables = tablesle->getFieldArray(sfTableEntries);
            if (aTables.size() > 0)
            {
				retPair = TableSyncUtil::IsTableSLEChanged(aTables, lastTxChangeIndex, m->tablename(), false);
            }
        }
        
        if (retPair.second == NULL && retPair.first)
        {
            auto time = ledger->info().closeTime.time_since_epoch().count();
            this->SendSeekEndReply(stopIndex, ledger->info().hash, iLastFindSeq, uLashFindHash, lastTxChangeHash, m->account(), m->tablename(), sNickName, time, eTargetType, wPeer);
            return;
        }
    }
        
    for (LedgerIndex i = checkIndex; i <= stopIndex; i++)
    {
        auto ledger = app_.getLedgerMaster().getLedgerBySeq(i);
        if (!ledger)   break;

		std::pair<bool, STEntry*> retPair;
        auto tablesle = ledger->read(key);
        if (tablesle)
        {
            auto & aTables = tablesle->getFieldArray(sfTableEntries);
            if (aTables.size() > 0)
            {
				retPair = TableSyncUtil::IsTableSLEChanged(aTables, lastTxChangeIndex, m->tablename(), true);
            }
        }

        auto time = ledger->info().closeTime.time_since_epoch().count();
		if (retPair.second != NULL)
        {   
            this->SendSeekResultReply(m->account(), i == stopIndex,time,wPeer, sNickName, eTargetType, ledger->info().seq, retPair.second->getFieldH256(sfTxnLedgerHash),lastTxChangeIndex, lastTxChangeHash,m->tablename());
            iLastFindSeq = i;
            uLashFindHash = ledger->info().hash;
            lastTxChangeIndex = i;
			//lastTxChangeHash = ledger->info().hash;//retPair.second->getFieldH256(sfTxnLedgerHash);
			lastTxChangeHash = retPair.second->getFieldH256(sfTxnLedgerHash);
        }
        else if(iBlockEnd == i || (!bGetLost && i == stopIndex) || !retPair.first)
        {       
            this->SendSeekEndReply(i, ledger->info().hash, iLastFindSeq, uLashFindHash, lastTxChangeHash, m->account(), m->tablename(), sNickName, time, eTargetType, wPeer);
            break;
        }
        else
        {
            continue;
        }
    }
}
bool TableSync::SendSyncRequest(AccountID accountID, std::string sTableName, LedgerIndex iStartSeq, uint256 iStartHash, LedgerIndex iCheckSeq, uint256 iCheckHash, LedgerIndex iStopSeq, bool bGetLost, std::shared_ptr <TableSyncItem> pItem)
{
    protocol::TMGetTable tmGT;
    tmGT.set_account(to_string(accountID));
    tmGT.set_tablename(sTableName);
    tmGT.set_ledgerseq(iStartSeq);
    tmGT.set_ledgerhash(to_string(iStartHash)); 
    tmGT.set_ledgerstopseq(iStopSeq);
    tmGT.set_ledgercheckseq(iCheckSeq);
    tmGT.set_ledgercheckhash(to_string(iCheckHash));
    tmGT.set_getlost(bGetLost);
	tmGT.set_etargettype(pItem->TargetType());
    tmGT.set_nickname(pItem->GetNickName());

    pItem->SendTableMessage(std::make_shared<Message>(tmGT, protocol::mtGET_TABLE));
    return true;
}

bool TableSync::InsertSnycDB(std::string TableName, std::string TableNameInDB, std::string Owner,LedgerIndex LedgerSeq, uint256 LedgerHash, bool IsAutoSync, std::string time,uint256 chainId)
{
    return app_.getTableStatusDB().InsertSnycDB(TableName, TableNameInDB, Owner, LedgerSeq, LedgerHash, IsAutoSync, time, chainId);
}

bool TableSync::ReadSyncDB(std::string nameInDB, LedgerIndex &txnseq, uint256 &txnhash, LedgerIndex &seq, uint256 &hash, uint256 &txnupdatehash)
{
    return app_.getTableStatusDB().ReadSyncDB(nameInDB, txnseq, txnhash, seq, hash, txnupdatehash);
}

//void TableSync::parseFormline

std::pair<std::shared_ptr<TableSyncItem>, std::string> TableSync::CreateOneItem(TableSyncItem::SyncTargetType eTargeType, std::string line)
{	
    std::shared_ptr<TableSyncItem> pItem = NULL;
	std::string owner, tablename, userId, secret, condition, user;

    //trim space lies in the head and tail.
    line.erase(line.find_last_not_of(' ') + 1);
    line.erase(0, line.find_first_not_of(' '));

	size_t pos = line.find(' ');
	if (pos == std::string::npos)
		return std::make_pair(pItem, "para is null.");

	owner = line.substr(0, pos);

	line = line.substr(pos);
	pos = line.find_first_not_of(' ');
	if (pos == std::string::npos)
		return std::make_pair(pItem, "para is less than 2.");

	line = line.substr(pos);
	pos = line.find_first_of(' ');
	tablename = line.substr(0, pos);

	if (pos != std::string::npos)
	{
		line = line.substr(pos);
		pos = line.find_first_not_of(' ');
		do
		{
			line = line.substr(pos);
			pos = line.find_first_of(' ');
			std::string tmp = line.substr(0, pos);

			//not support secret after condition,but support the contrary
			if (tmp[0] == 'x' || tmp[0] == 'p')
				secret = tmp;
			else if (tmp[0] == 'z')
				user = tmp;
			else
			{
				if (tmp[0] == '~')
					condition = tmp;
				else
				{
					//date time
                    condition = tmp;
                    int symbolPos = condition.find('-');
                    int underPos = condition.find('_');
                    if (symbolPos != std::string::npos)
                    {
                        if (underPos != std::string::npos)
                            condition.replace(underPos, 1, " ");
                        else
                            condition = "";
                    }
				}
			}	
			if (pos != std::string::npos)
			{
				line = line.substr(pos);
				pos = line.find_first_not_of(' ');
			}
		} while (pos != std::string::npos);
	}

	//test owner is valid
	AccountID accountID, userAccountId;
	SecretKey secret_key;    
    PublicKey public_key;

    auto oAccountID = ripple::parseBase58<AccountID>(owner);
    if (boost::none == oAccountID)
    {
        sLastErr_ = tablename + ":owner invalid!";
        return std::make_pair(pItem, sLastErr_);
    }
    else accountID = *oAccountID;

    if (HardEncryptObj::getInstance() != NULL)
    {
        try
        {
            
            if(!user.empty() && !secret.empty())
            {
				auto pUser = ripple::parseBase58<AccountID>(user);
				if (boost::none == pUser)
					return std::make_pair(pItem, tablename + ":user invalid!");
				userAccountId = *pUser;
                std::string privateKeyStrDe58 = decodeBase58Token(secret, TOKEN_ACCOUNT_SECRET);
                SecretKey tempSecKey(Slice(privateKeyStrDe58.c_str(), strlen(privateKeyStrDe58.c_str())));
                secret_key = tempSecKey;
            }
        }
        catch (std::exception const& e)
        {
            JLOG(journal_.warn()) <<
                "AccountID|userAccountId|secret exception" << e.what();
            sLastErr_ = tablename + " exception :" + e.what();
            return std::make_pair(pItem, sLastErr_);
        }
    }
    else
    {
        try
        {   
            if (secret.size() > 0)
            {
                //create secret key from given secret
                auto seed = parseBase58<Seed>(secret);
                if (seed)
                {
                    KeyType keyType = KeyType::secp256k1;
                    std::pair<PublicKey, SecretKey> key_pair = generateKeyPair(keyType, *seed);
                    public_key = key_pair.first;
                    secret_key = key_pair.second;
                    userAccountId = calcAccountID(public_key);
                }
            }
        }
        catch (std::exception const& e)
        {
            JLOG(journal_.warn()) <<
                "AccountID|userAccountId|secret exception" << e.what();
            sLastErr_ = tablename + " exception :" + e.what();
            return std::make_pair(pItem, sLastErr_);
        }
    }
	

    if (eTargeType != TableSyncItem::SyncTarget_audit)
    {
        if (isExist(listTableInfo_, accountID, tablename, eTargeType))
        {
            JLOG(journal_.warn()) << tablename <<
                "has been created, target type is " << eTargeType;
            sLastErr_ = tablename + " has been created";
            return std::make_pair(pItem, sLastErr_);
        }
    }		
	
	if (eTargeType == TableSyncItem::SyncTarget_db)
	{
		pItem = std::make_shared<TableSyncItem>(app_, journal_, cfg_, eTargeType);
        //pItem->Init(accountID, tablename, userAccountId, secret_key, condition, false);
	}
	else if (eTargeType == TableSyncItem::SyncTarget_dump)
	{
		if(secret.size() > 0 && !userAccountId)  return std::make_pair(pItem, "secret is invalid.");
        pItem = std::make_shared<TableDumpItem>(app_, journal_, cfg_, eTargeType);
	}
    else if (eTargeType == TableSyncItem::SyncTarget_audit)
    {
        if (secret.size() > 0 && !userAccountId)  return std::make_pair(pItem, "secret is invalid.");
        pItem = std::make_shared<TableAuditItem>(app_, journal_, cfg_, eTargeType);
    }
    else
    {
        assert(0);
    }
    pItem->Init(accountID, tablename, userAccountId, secret_key, condition, false);
    if (eTargeType != TableSyncItem::SyncTarget_db)
    {
        //pItem->Init(accountID, tablename, userAccountId, secret_key, condition, false);
        auto retPair = pItem->InitPassphrase();
        if (!retPair.first)			                return std::make_pair(std::shared_ptr<TableSyncItem>(NULL), retPair.second);
    }

	return std::make_pair(pItem, "");
}

bool TableSync::CreateTableItems()
{
    bool ret = false;
    try
    {
        auto section = cfg_.section(ConfigSection::syncTables());
        const std::vector<std::string> lines = section.lines();       

        //1.read data from config
        for (std::string line : lines)
		{
			auto ret = CreateOneItem(TableSyncItem::SyncTarget_db, line);			
            if (ret.first != NULL)
            {
                std::lock_guard<std::mutex> lock(mutexlistTable_);
                listTableInfo_.push_back(ret.first);
				//this set can only be modified here
				setTableInCfg.emplace(to_string(ret.first->GetAccount()) + ret.first->GetTableName());
            }
        }
		
        //2.read from state table
		auto ledger = app_.getLedgerMaster().getValidatedLedger();
		//read chainId
		uint256 chainId = TableSyncUtil::GetChainId(ledger.get());

        std::list<std::tuple<std::string, std::string, std::string, bool> > list;
        app_.getTableStatusDB().GetAutoListFromDB(chainId, list); 

        std::string owner, tablename, time;
        bool isAutoSync;

        for (auto tuplelem : list)
        {            
            std::tie(owner, tablename, time, isAutoSync) = tuplelem;

            AccountID accountID;
            try
            {
				auto pOwner = ripple::parseBase58<AccountID>(owner);
				if (pOwner)
					accountID = *pOwner;
				else
				{
					JLOG(journal_.error()) <<
						"ripple::parseBase58<AccountID> parse failed" << owner;
					continue;
				}
            }
            catch (std::exception const& e)
            {
                JLOG(journal_.warn()) <<
                    "ripple::parseBase58<AccountID> exception" << e.what();
            }

            std::shared_ptr <TableSyncItem> pItem = NULL;
            pItem = GetRightItem(accountID, tablename, "", TableSyncItem::SyncTarget_db,false);
            if (pItem != NULL)
            {   
                //if time > cond_time then set stop
                if (pItem->GetCondition().utime > 0 &&
                    pItem->GetCondition().utime < std::stoi(time))
                {
                    listTableInfo_.remove(pItem);
                    app_.getTableStatusDB().UpdateStateDB(owner, tablename, false);//update audoSync flag
                }
            }       
            else
			{
				std::shared_ptr<TableSyncItem> pAutoSynItem = std::make_shared<TableSyncItem>(app_, journal_, cfg_);
				pAutoSynItem->Init(accountID, tablename, time, true);
				listTableInfo_.push_back(pAutoSynItem);
            }           
        }
        ret = true;
    }
    catch (std::exception const& e)
    {
        JLOG(journal_.error()) <<
            "create table item exception " << e.what();
        ret = false;
    }
    return ret;
}

bool TableSync::isExist(std::list<std::shared_ptr <TableSyncItem>>  listTableInfo_, AccountID accountID, std::string sTableName, TableSyncItem::SyncTargetType eTargeType)
{
    std::lock_guard<std::mutex> lock(mutexlistTable_);
    std::list<std::shared_ptr <TableSyncItem>>::iterator iter = std::find_if(listTableInfo_.begin(), listTableInfo_.end(),
        [accountID, sTableName, eTargeType](std::shared_ptr <TableSyncItem> pItem) {

		bool bExist = (pItem->GetTableName() == sTableName) &&
			                  (pItem->GetAccount() == accountID) &&
			                  (pItem->TargetType() == eTargeType) &&
			                  (pItem->GetSyncState() != TableSyncItem::SYNC_DELETING && pItem->GetSyncState() != TableSyncItem::SYNC_REMOVE);

        return bExist;
    });
    if (iter == listTableInfo_.end())     return false;
    return true;
}

bool TableSync::SendData(std::shared_ptr <TableSyncItem> pItem, std::shared_ptr <protocol::TMTableData> const& m)
{
    if (pItem == NULL)  return false;

    protocol::TMTableData& data = *m;

    uint256 uhash = from_hex_text<uint256>(data.ledgerhash());
    auto ledgerSeq = data.ledgerseq();

    LedgerIndex iCurSeq, iTxSeq;
    uint256 iCurHash, iTxHash;
    pItem->GetSyncLedger(iCurSeq, iCurHash);
    pItem->GetSyncTxLedger(iTxSeq, iTxHash);
    //consecutive
    auto tmp = std::make_pair(ledgerSeq, data);
    auto str = to_string(iTxHash);
    if (data.txnodes().size() > 0)
    {
        if (data.lastledgerseq() == iTxSeq && data.lastledgerhash() == to_string(iTxHash))
        {  
            pItem->PushDataToWholeDataQueue(tmp);
            if (!data.seekstop())
            {
                pItem->TransBlock2Whole(ledgerSeq, uhash);
            }
            pItem->TryOperateSQL();
        }
        else
        {
            pItem->PushDataToBlockDataQueue(tmp);
        }
    }
    else
    {
        if (data.lastledgerseq() == iCurSeq && data.lastledgerhash() == to_string(iCurHash))
        {
            pItem->PushDataToWholeDataQueue(tmp);
            pItem->TryOperateSQL();
        }
        else
        {
            pItem->PushDataToBlockDataQueue(tmp);
        }
    }

    return true;
}

bool TableSync::GotSyncReply(std::shared_ptr <protocol::TMTableData> const& m, std::weak_ptr<Peer> const& wPeer)
{
    protocol::TMTableData& data = *m;

    AccountID accountID(*ripple::parseBase58<AccountID>(data.account()));
    uint256 uhash = from_hex_text<uint256>(data.ledgerhash());
    auto ledgerSeq = data.ledgerseq();

    std::string sNickName = data.has_nickname() ? data.nickname() : "";
    std::shared_ptr <TableSyncItem> pItem = GetRightItem(accountID, data.tablename(), sNickName, (TableSyncItem::SyncTargetType)data.etargettype());
    
    if (pItem == NULL)   return false;
     
    std::lock_guard<std::mutex> lock(pItem->WriteDataMutex());

    LedgerIndex iCurSeq;
    uint256 iCurHash, uLocalHash;
    pItem->GetSyncLedger(iCurSeq, iCurHash);
    if (ledgerSeq <= iCurSeq)  return false;

    auto peer = wPeer.lock();
    if (peer != NULL)
    {
        pItem->SetPeer(peer);
        JLOG(journal_.trace()) <<
            "got GotSyncReply from " << peer->getRemoteAddress() << " ledgerSeq " << ledgerSeq;
    }

    pItem->UpdateDataTm();
    pItem->SetSyncState(TableSyncItem::SYNC_WAIT_DATA);    
    pItem->ClearFailList(); 

    uLocalHash = GetLocalHash(ledgerSeq);
    if(uLocalHash.isZero())
    {
        auto tmp = std::make_pair(ledgerSeq, data);
        pItem->PushDataToWaitCheckQueue(tmp);

        if (pItem->GetCheckLedgerState() != TableSyncItem::SYNC_WAIT_LEDGER)
        {   
            LedgerIndex iDstSeq = getCandidateLedger(ledgerSeq);
            uint256 iDstHhash = app_.getLedgerMaster().getHashBySeqEx(iDstSeq);
            if (SendLedgerRequest(iDstSeq, iDstHhash, pItem))
                pItem->SetLedgerState(TableSyncItem::SYNC_WAIT_LEDGER);
            pItem->UpdateLedgerTm();
            return false;
        }
    }
    else
    {
        if (uhash != uLocalHash)  return false;

        if (pItem->GetCheckLedgerState() != TableSyncItem::SYNC_WAIT_LEDGER
            && ledgerSeq == getCandidateLedger(ledgerSeq))
        {
            pItem->SetLedgerState(TableSyncItem::SYNC_GOT_LEDGER);
        }

        bool bRet = SendData(pItem, m);

        return bRet;
    }

    return true;
}

bool TableSync::ReStartOneTable(AccountID accountID, std::string sNameInDB,std::string sTableName, bool bDrop, bool bCommit)
{
    auto pItem = GetRightItem(accountID, sNameInDB, "", TableSyncItem::SyncTarget_db);
    if (pItem != NULL)
    {
		if (bDrop)
		{
			pItem->ReSetContexAfterDrop();
		}
		else
		{
			pItem->ReInit();
		}
    }
    else
    {
        bool bInTempList = false;
        auto it = std::find_if(listTempTable_.begin(), listTempTable_.end(),
            [sNameInDB](std::string sName) {
            return sName == sNameInDB;
        });
        bInTempList = it != listTempTable_.end();

        if (bCommit && bInTempList && bAutoLoadTable_)
        {
            std::shared_ptr<TableSyncItem> pItem = std::make_shared<TableSyncItem>(app_, journal_, cfg_);
            pItem->Init(accountID, sTableName, true);
            pItem->SetTableNameInDB(sNameInDB);
            {
                std::lock_guard<std::mutex> lock(mutexlistTable_);
                listTableInfo_.push_back(pItem);
            }
        }

        std::lock_guard<std::mutex> lock(mutexTempTable_);
        listTempTable_.remove(sNameInDB);
        return true;
    }
    return true;
}

bool TableSync::StopOneTable(AccountID accountID, std::string sNameInDB, bool bNewTable)
{
    auto pItem = GetRightItem(accountID, sNameInDB, "", TableSyncItem::SyncTarget_db);
    if (pItem == NULL)
    {
        if (bNewTable)
        {
            std::lock_guard<std::mutex> lock(mutexTempTable_);
            listTempTable_.push_back(sNameInDB);
            return true;
        }
        else
        {
            return false;
        }
    }
    else
    {
        return pItem->StopSync();
    }
    
    return false;
}

bool TableSync::CheckTheReplyIsValid(std::shared_ptr <protocol::TMTableData> const& m)
{
    return true;
}

bool TableSync::IsNeedSyn(std::shared_ptr <TableSyncItem> pItem)
{
    if (!bIsHaveSync_)
    {
        if (pItem->TargetType() == TableSyncItem::SyncTarget_db || 
            pItem->TargetType() == TableSyncItem::SyncTarget_audit)     return false;
    }
    
    if (pItem->GetSyncState() == TableSyncItem::SYNC_STOP)              return false;
       
    return true;
}
bool TableSync::ClearNotSyncItem()
{
	std::lock_guard<std::mutex> lock(mutexlistTable_);	

	listTableInfo_.remove_if([this](std::shared_ptr <TableSyncItem> pItem) {
		return pItem->GetSyncState() == TableSyncItem::SYNC_REMOVE || 
			   pItem->GetSyncState() == TableSyncItem::SYNC_STOP;
	});
	return true;
}
bool TableSync::IsNeedSyn()
{
    std::lock_guard<std::mutex> lock(mutexlistTable_);
    std::list<std::shared_ptr <TableSyncItem>>::iterator it = std::find_if(listTableInfo_.begin(), listTableInfo_.end(),
        [this](std::shared_ptr <TableSyncItem> pItem) {
        return this->IsNeedSyn(pItem);
    });

    return it != listTableInfo_.end();
}

void TableSync::TryTableSync()
{
    if (!bInitTableItems_ && bIsHaveSync_)
    {
		if (app_.getLedgerMaster().getValidLedgerIndex() > 1)
		{
			CreateTableItems();
			bInitTableItems_ = true;
		}
    }

    ClearNotSyncItem();

    if (!IsNeedSyn())                 return;

    if (!bTableSyncThread_)
    {
        bTableSyncThread_ = true;
        app_.getJobQueue().addJob( jtTABLESYNC, "tableSync", [this](Job&) { TableSyncThread(); });
    }
}

void TableSync::TableSyncThread()
{
    TableSyncItem::BaseInfo stItem;
    std::string PreviousCommit;
    std::list<std::shared_ptr <TableSyncItem>> tmList;
    {
        std::lock_guard<std::mutex> lock(mutexlistTable_);
        for (std::list<std::shared_ptr <TableSyncItem>>::iterator iter = listTableInfo_.begin(); iter != listTableInfo_.end(); ++iter)
        {
            tmList.push_back(*iter);
        }
    }

	bool bNeedReSync = false;
	bool bNeedLocalSync = false;
	auto iter = tmList.begin();
    while (iter != tmList.end())
    {
		auto pItem = *iter;
        if (!IsNeedSyn(pItem))   continue;

        pItem->GetBaseInfo(stItem);         
        switch (stItem.eState)
        {           
        case TableSyncItem::SYNC_REINIT:
        {
            LedgerIndex TxnLedgerSeq = 0, LedgerSeq = 1;
            uint256 TxnLedgerHash, LedgerHash, TxnUpdateHash;

            ReadSyncDB(stItem.sTableNameInDB, TxnLedgerSeq, TxnLedgerHash, LedgerSeq, LedgerHash, TxnUpdateHash);

			pItem->SetPara(stItem.sTableNameInDB, LedgerSeq, LedgerHash, TxnLedgerSeq, TxnLedgerHash, TxnUpdateHash);
			pItem->SetSyncState(TableSyncItem::SYNC_BLOCK_STOP);
            break;
		}
		case TableSyncItem::SYNC_DELETING:
		{
			//delete a table
			std::string sNameInDB;
			if (pItem->IsNameInDBExist(stItem.sTableName, to_string(stItem.accountID), true, sNameInDB))
			{
				//pItem->DeleteTable(sNameInDB);
				pItem->DoUpdateSyncDB(to_string(stItem.accountID), sNameInDB, true, PreviousCommit);
			}
			if (pItem->getAutoSync())
				pItem->SetSyncState(TableSyncItem::SYNC_REMOVE);
			else
				pItem->SetSyncState(TableSyncItem::SYNC_INIT);
			break;
		}
        case TableSyncItem::SYNC_INIT:
        {
			JLOG(journal_.info()) << "TableSyncThread SYNC_INIT,tableName=" << stItem.sTableName << ",owner=" << to_string(stItem.accountID);

			if (app_.getLedgerMaster().getValidLedgerIndex() == 0)
				break;
			auto stBaseInfo = app_.getLedgerMaster().getTableBaseInfo(app_.getLedgerMaster().getValidLedgerIndex(), stItem.accountID, stItem.sTableName);            
            std::string nameInDB = to_string(stBaseInfo.nameInDB);
            
			if (stItem.eTargetType == TableSyncItem::SyncTarget_db)
			{
				if (stBaseInfo.nameInDB.isNonZero()) //local read nameInDB is not zero
				{
                    LedgerIndex TxnLedgerSeq = 0, LedgerSeq = 1;
                    uint256 TxnLedgerHash, LedgerHash, TxnUpdateHash;

					//if exist in SyncTableState(not if deleted and created again)
					if (pItem->IsExist(stItem.accountID, nameInDB))
					{
						pItem->RenameRecord(stItem.accountID, nameInDB, stItem.sTableName);

                        ReadSyncDB(nameInDB, TxnLedgerSeq, TxnLedgerHash, LedgerSeq, LedgerHash, TxnUpdateHash);

						//for example recreate
						if (stBaseInfo.createLgrSeq > TxnLedgerSeq)
						{
							std::string cond, PreviousCommit;
							LedgerSeq = stBaseInfo.createLgrSeq;
							LedgerHash = stBaseInfo.createdLedgerHash;
							TxnLedgerSeq = 0;
							TxnLedgerHash = uint256();
							TxnUpdateHash = uint256();														
							pItem->DoUpdateSyncDB(to_string(stItem.accountID), nameInDB, to_string(TxnLedgerHash), to_string(TxnLedgerSeq), to_string(LedgerHash), to_string(LedgerSeq), to_string(TxnUpdateHash),cond, PreviousCommit);
						}
					}
					else
					{
						// get nameInDB from SyncTableState
						std::string localNameInDB;
						if (pItem->IsNameInDBExist(stItem.sTableName, to_string(stItem.accountID), true, localNameInDB))
						{
							pItem->DoUpdateSyncDB(to_string(stItem.accountID), localNameInDB, true, PreviousCommit);
							pItem->DeleteTable(localNameInDB);
						}

                        LedgerSeq = stBaseInfo.createLgrSeq;
                        LedgerHash = stBaseInfo.createdLedgerHash;
                        TxnLedgerSeq = 0;
                        TxnLedgerHash = uint256();
						bool bAutoSync = true;
						std::string temKey = to_string(stItem.accountID) + stItem.sTableName;
						if(setTableInCfg.end() != std::find(setTableInCfg.begin(), setTableInCfg.end(), temKey))
						{
							bAutoSync = false;
						}
						pItem->SetDeleted(false);
						auto chainId = TableSyncUtil::GetChainId(app_.getLedgerMaster().getValidatedLedger().get());
                        InsertSnycDB(stItem.sTableName, nameInDB, to_string(stItem.accountID), LedgerSeq, LedgerHash, bAutoSync, "",chainId);
						app_.getTableStatusDB().UpdateSyncDB(to_string(stItem.accountID), nameInDB, to_string(TxnLedgerHash), to_string(TxnLedgerSeq), to_string(LedgerHash), to_string(LedgerSeq), "", "", "");
                    }
					pItem->SetPara(nameInDB, LedgerSeq, LedgerHash, TxnLedgerSeq, TxnLedgerHash, TxnUpdateHash);

					if (pItem->InitPassphrase().first)
					{
						pItem->SetSyncState(TableSyncItem::SYNC_BLOCK_STOP);
						bNeedReSync = true;
						JLOG(journal_.info()) << "InitPassphrase success,tableName=" << stItem.sTableName << ",owner=" << to_string(stItem.accountID);
					}
					else
					{
						pItem->SetSyncState(TableSyncItem::SYNC_STOP);
					}
				}
				else if(!stItem.isDeleted)
				{
					std::string sNameInDB;
					if (pItem->IsNameInDBExist(stItem.sTableName, to_string(stItem.accountID), true, sNameInDB))
					{
						if (stItem.isAutoSync)
						{
							//will drop on the next ledger
							pItem->SetSyncState(TableSyncItem::SYNC_DELETING);
						}
						else
						{
							pItem->DoUpdateSyncDB(to_string(stItem.accountID), sNameInDB, true, PreviousCommit);
							pItem->SetDeleted(true);
						}
					}						
					break;
				}
			}
			else
			{
				if (stBaseInfo.nameInDB.isNonZero())
				{
					if (stBaseInfo.createLgrSeq > stItem.u32SeqLedger)
					{						
						pItem->SetPara(nameInDB, stBaseInfo.createLgrSeq, stBaseInfo.createdLedgerHash, stItem.uTxSeq, stItem.uTxHash, stItem.uTxUpdateHash);
                        //pItem->GetBaseInfo(stItem);
					}
					else
					{
						pItem->SetTableNameInDB(nameInDB);
					}                   
                    
					pItem->SetSyncState(TableSyncItem::SYNC_BLOCK_STOP);
				}
				else
				{
					break;
				}
			}            
        }        
        case TableSyncItem::SYNC_BLOCK_STOP:
            if (app_.getLedgerMaster().haveLedger(stItem.u32SeqLedger+1) || app_.getLedgerMaster().lastCompleteIndex() <= stItem.u32SeqLedger + 1)
            {
                pItem->SetSyncState(TableSyncItem::SYNC_WAIT_LOCAL_ACQUIRE);
				if (!bNeedLocalSync)
				{
					//TryLocalSync();
					bNeedLocalSync = true;
				}
            }            
            else
            { 
                LedgerIndex refIndex = getCandidateLedger(stItem.u32SeqLedger+1);
                refIndex = std::min(refIndex, app_.getLedgerMaster().getValidLedgerIndex());
                
                if (SendSyncRequest(stItem.accountID, stItem.sTableNameInDB, stItem.u32SeqLedger, stItem.uHash, stItem.uTxSeq, stItem.uTxHash, refIndex, false, pItem))
                {
                    JLOG(journal_.trace()) <<
                        "In SYNC_BLOCK_STOP,SendSyncRequest sTableName " << stItem.sTableName << " LedgerSeq " << stItem.u32SeqLedger;
                }

                pItem->UpdateDataTm();
                pItem->SetSyncState(TableSyncItem::SYNC_WAIT_DATA);
            }
            break;
        case TableSyncItem::SYNC_WAIT_DATA:            
            if (pItem->IsGetDataExpire() && stItem.lState != TableSyncItem::SYNC_WAIT_LEDGER)
            {
                TableSyncItem::BaseInfo stRange;
                if(!pItem->GetRightRequestRange(stRange)) break;

                bool bGetLost = false;
                bGetLost = app_.getLedgerMaster().getValidLedgerIndex() > stRange.uStopSeq;
                stRange.uStopSeq = std::min(stRange.uStopSeq, app_.getLedgerMaster().getValidLedgerIndex());
  
                if (app_.getLedgerMaster().haveLedger(stRange.u32SeqLedger) || app_.getLedgerMaster().lastCompleteIndex() <= stItem.u32SeqLedger)
                {
                    pItem->SetSyncState(TableSyncItem::SYNC_WAIT_LOCAL_ACQUIRE);
					if (!bNeedLocalSync)
					{
						//TryLocalSync();
						bNeedLocalSync = true;
					}
                }
                else
                {
                    //if (stRange.uStopSeq == stRange.u32SeqLedger)
                    //{
                        //stRange.u32SeqLedger/0;
                    //}
                    SendSyncRequest(stItem.accountID, stItem.sTableNameInDB, stRange.u32SeqLedger, stRange.uHash, stRange.uTxSeq, stRange.uTxHash, stRange.uStopSeq, bGetLost, pItem);
                }                
                pItem->UpdateDataTm();
            }
            break;
        case TableSyncItem::SYNC_WAIT_LOCAL_ACQUIRE:
        case TableSyncItem::SYNC_LOCAL_ACQUIRING:
            break;
        default:
            break;
        }   

        if (stItem.lState == TableSyncItem::SYNC_WAIT_LEDGER)
        {
            auto b256thExist = this->Is256thLedgerExist(stItem.u32SeqLedger + 1);
            if (b256thExist)
            {
                pItem->SetLedgerState(TableSyncItem::SYNC_GOT_LEDGER);
                pItem->DealWithWaitCheckQueue([pItem, this](TableSyncItem::sqldata_type const& pairData) {
                    uint256 ledgerHash = from_hex_text<uint256>(pairData.second.ledgerhash());
                    auto ledgerSeq = pairData.second.ledgerseq();
                    uint256 uLocalHash = GetLocalHash(ledgerSeq);
                    if (uLocalHash == ledgerHash)
                    {
                        SendData(pItem, std::make_shared<protocol::TMTableData>(pairData.second));
                        return true;
                    }
                    return false;
                });
            }

            if (pItem->IsGetLedgerExpire())
            {
                LedgerIndex iDstSeq = getCandidateLedger(stItem.u32SeqLedger);
                uint256 iDstHhash = app_.getLedgerMaster().getHashBySeqEx(iDstSeq);
                if (SendLedgerRequest(iDstSeq, iDstHhash, pItem))
                    pItem->SetLedgerState(TableSyncItem::SYNC_WAIT_LEDGER);
                pItem->UpdateLedgerTm();
            }
        }
		iter++;
    }

    if (bNeedLocalSync)
    {
        TryLocalSync();
    }

    bTableSyncThread_ = false;

	if (bNeedReSync) {
		TryTableSync();
	}
}

void TableSync::TryLocalSync()
{
    if (!bLocalSyncThread_)
    {
        bLocalSyncThread_ = true;
        app_.getJobQueue().addJob(jtTABLELOCALSYNC, "tableLocalSync", [this](Job&) { LocalSyncThread(); });
    }
}
void TableSync::LocalSyncThread()
{
    TableSyncItem::BaseInfo stItem;
    std::list<std::shared_ptr <TableSyncItem>> tmList;
    {
        std::lock_guard<std::mutex> lock(mutexlistTable_);
        for (std::list<std::shared_ptr <TableSyncItem>>::iterator iter = listTableInfo_.begin(); iter != listTableInfo_.end(); ++iter)
        {
            tmList.push_back(*iter);
        }
    }
	TaggedCache<LedgerIndex, std::map<AccountID, std::shared_ptr<const SLE>>> cache("TableLocalSync",100,10, stopwatch(), journal_);
    for (auto pItem : tmList)
    {
        pItem->GetBaseInfo(stItem);
        if (stItem.eState == TableSyncItem::SYNC_WAIT_LOCAL_ACQUIRE)
        {           
            pItem->SetSyncState(TableSyncItem::SYNC_LOCAL_ACQUIRING);
            pItem->StartLocalLedgerRead();
            SeekTableTxLedger(stItem,cache);
            pItem->StopLocalLedgerRead();
        }
    }  
	bLocalSyncThread_ = false;
}

bool TableSync::Is256thLedgerExist(LedgerIndex index)
{
    LedgerIndex iDstSeq = getCandidateLedger(index);

    auto ledger = app_.getLedgerMaster().getLedgerBySeq(iDstSeq);
    if (ledger != NULL) return true;

    if (checkSkipNode_.fetch(iDstSeq) == nullptr)  return false;

    return true;
}

std::pair<bool, std::string>
	TableSync::InsertListDynamically(AccountID accountID, std::string sTableName, std::string sNameInDB, LedgerIndex seq, uint256 uHash,uint32 time, uint256 chainId)
{
    if (!bIsHaveSync_)                return std::make_pair(false,"Table is not configured to sync.");

    std::lock_guard<std::mutex> lock(mutexCreateTable_);
    
    bool ret = false;
	std::string err = "";
    try
    {
        //check formal tables
        if (isExist(listTableInfo_, accountID, sTableName, TableSyncItem::SyncTarget_db))
            return std::make_pair(false,"Table exist in listTableInfo");
        //check temp tables
        auto it = std::find_if(listTempTable_.begin(), listTempTable_.end(),
            [sNameInDB](std::string sName) {
            return sName == sNameInDB;
        });
        if (it != listTempTable_.end())
            return std::make_pair(false,err);

        std::shared_ptr<TableSyncItem> pItem = std::make_shared<TableSyncItem>(app_, journal_, cfg_);
        //std::string PreviousCommit;
        pItem->Init(accountID, sTableName,true);
        ret = InsertSnycDB(sTableName, sNameInDB, to_string(accountID), seq, uHash, true, to_string(time), chainId);
		if(ret)
		{
			ret = true;
            std::lock_guard<std::mutex> lock(mutexlistTable_);
            listTableInfo_.push_back(pItem);   
			JLOG(journal_.info()) <<
				"InsertListDynamically listTableInfo_ add item,tableName=" << sTableName <<",owner="<< to_string(accountID);
        }
    }
    catch (std::exception const& e)
    {
        JLOG(journal_.error()) <<
            "InsertSnycDB exception " << e.what();
        ret = false;
		err = e.what();
    }
    return std::make_pair(ret,err);
}

uint256 TableSync::GetLocalHash(LedgerIndex ledgerSeq)
{
    uint256 hash;
    hash = app_.getLedgerMaster().getHashBySeqEx(ledgerSeq);
    if (hash.isNonZero())   return hash;

    std::lock_guard<std::mutex> lock(mutexSkipNode_);
    LedgerIndex i256thSeq = getCandidateLedger(ledgerSeq);
    auto BlobData = checkSkipNode_.fetch(i256thSeq);
    if (BlobData)
    {
        uint256 key; //need parse form BlobData
        std::shared_ptr<SLE> pSLE = std::make_shared<SLE>(SerialIter{ BlobData->data(), BlobData->size() }, key);

        if (pSLE)
        {
            int diff = i256thSeq - ledgerSeq;
            STVector256 vec = pSLE->getFieldV256(sfHashes);
            hash = vec[vec.size() - diff];
        }
    }

    return hash;
}

bool TableSync::CheckSyncDataBy256thLedger(std::shared_ptr <TableSyncItem> pItem, LedgerIndex ledgerSeq,uint256 ledgerHash)
{  
    uint256 hash = app_.getLedgerMaster().getHashBySeqEx(ledgerSeq);

   uint256 checkHash;

   if (hash.isZero())
   {
        std::lock_guard<std::mutex> lock(mutexSkipNode_);
        LedgerIndex i256thSeq =  getCandidateLedger(ledgerSeq);
        auto BlobData = checkSkipNode_.fetch(i256thSeq);//1.query from local cache
        if (BlobData)
        { 
            uint256 key; //need parse form BlobData
            std::shared_ptr<SLE> pSLE = std::make_shared<SLE>(SerialIter{ BlobData->data(), BlobData->size() },key);

            if (pSLE)
            {
                int diff = i256thSeq - ledgerSeq;
                STVector256 vec = pSLE->getFieldV256(sfHashes);
                checkHash = vec[vec.size() - diff];                
            } 
        }
        else
        {
            if (pItem->GetCheckLedgerState() != TableSyncItem::SYNC_WAIT_LEDGER)
            {
                if (SendLedgerRequest(ledgerSeq, ledgerHash, pItem))
                    pItem->SetLedgerState(TableSyncItem::SYNC_WAIT_LEDGER);
                pItem->UpdateLedgerTm();
            }            
        }
    }
   else
   {
       checkHash = hash;
   }    

   if (checkHash == ledgerHash)
       return true;
   else
       return false;
}

bool TableSync::SendLedgerRequest(LedgerIndex iSeq, uint256 hash, std::shared_ptr <TableSyncItem> pItem)
{       
    protocol::TMGetLedger tmGL;
    tmGL.set_ledgerseq(iSeq);
    tmGL.set_ledgerhash(to_string(hash));
    tmGL.set_itype(protocol::liSKIP_NODE);
    tmGL.set_querydepth(3); // We probably need the whole thing

    Message::pointer oPacket = std::make_shared<Message>(
        tmGL, protocol::mtGET_LEDGER);
    pItem->SendTableMessage(oPacket);
    return true;
}
bool TableSync::GotLedger(std::shared_ptr <protocol::TMLedgerData> const& m)
{
    if (m->nodes().size() != 1)     return false;
    auto & node = m->nodes().Get(0);
    Blob blob;
    blob.assign(node.nodedata().begin(), node.nodedata().end());
    
    auto sleNew = std::make_shared<SLE>(
        SerialIter{ node.nodedata().data(), node.nodedata().size() }, keylet::skip().key);
    
    std::lock_guard<std::mutex> lock(mutexSkipNode_);
    auto p = std::make_shared<ripple::Blob>(blob);
    checkSkipNode_.canonicalize(m->ledgerseq(), p);

    return true;
}

std::shared_ptr <TableSyncItem> TableSync::GetRightItem(AccountID accountID, std::string sTableName, std::string sNickName, TableSyncItem::SyncTargetType eTargeType, bool bByNameInDB/* = true*/)
{
    std::lock_guard<std::mutex> lock(mutexlistTable_);
    auto iter(listTableInfo_.end());
    iter = std::find_if(listTableInfo_.begin(), listTableInfo_.end(),
        [accountID, sTableName, sNickName, eTargeType, bByNameInDB](std::shared_ptr <TableSyncItem>  pItem) {
		std::string sCheckName = bByNameInDB ? pItem->TableNameInDB() : pItem->GetTableName();
        return pItem->GetAccount() == accountID && sCheckName == sTableName && sNickName == pItem->GetNickName() && pItem->TargetType() == eTargeType;
    });

    if (iter == listTableInfo_.end())     return NULL;

    return *iter;
}


// check and sync table
void TableSync::CheckSyncTableTxs(std::shared_ptr<Ledger const> const& ledger)
{    
	if (ledger == NULL)    return;

    std::shared_ptr<AcceptedLedger> alpAccepted =
        app_.getAcceptedLedgerCache().fetch(ledger->info().hash);
    if (!alpAccepted)
    {
        alpAccepted = std::make_shared<AcceptedLedger>(
            ledger, app_.accountIDCache(), app_.logs());
        app_.getAcceptedLedgerCache().canonicalize(
            ledger->info().hash, alpAccepted);
    }

	std::map<uint160, bool> mapTxDBNam2Exist;

	for (auto const &item : alpAccepted->getMap())
	{
        if (item.second->getResult() != tesSUCCESS)
        {
            continue;
        }

		try
		{
            std::shared_ptr<const STTx> pSTTX = item.second->getTxn();
			auto vec = app_.getMasterTransaction().getTxs(*pSTTX, "", ledger, 0);
			auto time = ledger->info().closeTime.time_since_epoch().count();
			//read chainId
			uint256 chainId = TableSyncUtil::GetChainId(ledger.get());

			for (auto& tx : vec)
			{
				if (tx.isFieldPresent(sfOpType))
				{
					AccountID accountID = tx.getAccountID(sfAccount);
					auto tables         = tx.getFieldArray(sfTables);
					uint160 uTxDBName   = tables[0].getFieldH160(sfNameInDB);
					auto tableBlob      = tables[0].getFieldVL(sfTableName);
					std::string tableName;
					tableName.assign(tableBlob.begin(), tableBlob.end());

					if (!bIsHaveSync_)
					{
						app_.getOPs().pubTableTxs(accountID, tableName, *pSTTX, std::make_tuple("db_noDbConfig", "", ""), false);
						break;
					}

					auto opType = tx.getFieldU16(sfOpType);
					if (opType == T_CREATE)
					{
						std::string temKey = to_string(accountID) + tableName;
						bool bInSyncTables = true;
						if (setTableInCfg.end() == std::find(setTableInCfg.begin(), setTableInCfg.end(), temKey)) {
							bInSyncTables = false;
						}

						// not in [auto_sync] && [sync_tables]
						if (!bAutoLoadTable_ && !bInSyncTables) {

							app_.getOPs().pubTableTxs(accountID, tableName, *pSTTX, std::make_tuple("db_noAutoSync", "", ""), false);
							break;
						}

                        if (OnCreateTableTx(tx, ledger, time, chainId))
                        {
                            mapTxDBNam2Exist[uTxDBName] = true;
                        }
					}
					else if (opType == T_DROP || opType == R_INSERT || opType == R_UPDATE
						|| opType == R_DELETE)
					{

						bool bDBTableExist = false;
						if (mapTxDBNam2Exist.find(uTxDBName) == mapTxDBNam2Exist.end()) 
						{
							bDBTableExist = STTx2SQL::IsTableExistBySelect(app_.getTxStoreDBConn().GetDBConn(), "t_" + to_string(uTxDBName));
							mapTxDBNam2Exist[uTxDBName] = bDBTableExist;
						}
						else 
						{
							bDBTableExist = mapTxDBNam2Exist[uTxDBName];
						}
				
						if (!bDBTableExist)
						{
							app_.getOPs().pubTableTxs(accountID, tableName, *pSTTX, std::make_tuple("db_noTableExistInDB", "", ""), false);
							break;
						}

					}
				}
			}
		}
		catch (std::exception const&)
		{
			JLOG(journal_.warn()) << "Txn " << item.second->getTransactionID() << " throws";
		}
	}
}

bool TableSync::OnCreateTableTx(STTx const& tx, std::shared_ptr<Ledger const> const& ledger,uint32 time,uint256 const& chainId)
{
	AccountID accountID = tx.getAccountID(sfAccount);
	auto tables = tx.getFieldArray(sfTables);
	uint160 uTxDBName = tables[0].getFieldH160(sfNameInDB);

	auto tableBlob = tables[0].getFieldVL(sfTableName);
	std::string tableName;
	tableName.assign(tableBlob.begin(), tableBlob.end());
	auto insertRes = InsertListDynamically(accountID, tableName, to_string(uTxDBName), ledger->info().seq - 1, ledger->info().parentHash, time, chainId);
	if (!insertRes.first)
	{
		JLOG(journal_.error()) << "Insert to list dynamically failed,tableName=" << tableName << ",owner = " << to_string(accountID);

		std::tuple<std::string, std::string, std::string> result = std::make_tuple("db_error", "", insertRes.second);
		app_.getOPs().pubTableTxs(accountID, tableName, tx, result, false);
	}

    return insertRes.first;
}
//////////////////
bool TableSync::IsAutoLoadTable()
{
    return bAutoLoadTable_;
}

//for press-test
std::string TableSync::GetPressTableName()
{
	if(!pressRealName_.empty())
		return pressRealName_;

	auto pAccount = ripple::parseBase58<AccountID>("zHb9CJAWyB4zj91VRWn96DkukG4bwdtyTh");
	AccountID account = *pAccount;
	auto ledger = app_.getLedgerMaster().getValidatedLedger();
	auto id = keylet::table(account);
	auto const tablesle = ledger->read(id);
	if (tablesle == nullptr)
		return "";
	auto aTableEntries = tablesle->getFieldArray(sfTableEntries);
	std::string sCheckName = "press_time";
	auto iter(aTableEntries.end());
	iter = std::find_if(aTableEntries.begin(), aTableEntries.end(),
		[sCheckName](STObject const &item) {
		if (!item.isFieldPresent(sfTableName))  return false;
		auto sTableName = strCopy(item.getFieldVL(sfTableName));
		return sTableName == sCheckName;
	});
	if (iter == aTableEntries.end())
		return "";
	STEntry* pEntry = (STEntry*)(&(*iter));
	auto nameInDB = pEntry->getFieldH160(sfNameInDB);
	pressRealName_ = "t_" + to_string(nameInDB);

	return pressRealName_;
}

bool TableSync::IsPressSwitchOn()
{
	return bPressSwitchOn_;
}

void TableSync::SetHaveSyncFlag(bool haveSync)
{
    bIsHaveSync_ = haveSync;
}

void TableSync::sweep()
{
    checkSkipNode_.sweep();
}

std::pair<bool, std::string> TableSync::StartDumpTable(std::string sPara, std::string sPath, TableDumpItem::funDumpCB funCB)
{
    auto ret = CreateOneItem(TableSyncItem::SyncTarget_dump, sPara);
    if (ret.first != NULL)
    {
        std::shared_ptr<TableDumpItem> pDumpItem = std::static_pointer_cast<TableDumpItem>(ret.first);
        auto retPair = pDumpItem->SetDumpPara(sPath, funCB);
		if (!retPair.first)   
            return std::make_pair(false, retPair.second);
        else
        {
            std::lock_guard<std::mutex> lock(mutexlistTable_);
            listTableInfo_.push_back(ret.first);

            return std::make_pair(true, ret.second);
        }        
    }
    return std::make_pair(false, ret.second);
	
}
std::pair<bool, std::string> TableSync::StopDumpTable(AccountID accountID, std::string sTableName)
{
	std::lock_guard<std::mutex> lock(mutexlistTable_);
	auto iter(listTableInfo_.end());
	iter = std::find_if(listTableInfo_.begin(), listTableInfo_.end(),
		[accountID, sTableName](std::shared_ptr <TableSyncItem>  pItem) {
		return pItem->GetAccount() == accountID && pItem->GetTableName() == sTableName && pItem->TargetType() == TableSyncItem::SyncTarget_dump && pItem->GetSyncState() != TableSyncItem::SYNC_STOP;
	});

	if (iter == listTableInfo_.end())	
	{		
		return std::make_pair(false, "can't find the talbe in dump tasks.");;
	}
    
    std::shared_ptr<TableDumpItem> pDumpItem = std::static_pointer_cast<TableDumpItem>(*iter);
    auto retStop = pDumpItem->StopTask();
    if (!retStop.first)
    {
        return retStop;
    }
    
	(*iter)->SetSyncState(TableSyncItem::SYNC_STOP);

	return std::make_pair(true, "");
}

bool TableSync::GetCurrentDumpPos(AccountID accountID, std::string sTableName, TableSyncItem::taskInfo &info)
{
    std::lock_guard<std::mutex> lock(mutexlistTable_);
    auto iter(listTableInfo_.end());
    iter = std::find_if(listTableInfo_.begin(), listTableInfo_.end(),
        [accountID, sTableName](std::shared_ptr <TableSyncItem>  pItem) {
        return pItem->GetAccount() == accountID && pItem->GetTableName() == sTableName && pItem->TargetType() == TableSyncItem::SyncTarget_dump && pItem->GetSyncState() != TableSyncItem::SYNC_STOP;
    });

    if (iter == listTableInfo_.end())  return false;

    std::shared_ptr<TableDumpItem> pDumpItem = std::static_pointer_cast<TableDumpItem>(*iter);
    pDumpItem->GetCurrentPos(info);

    return true;

}
std::pair<bool, std::string> TableSync::StartAuditTable(std::string sPara, std::string sSql, std::string sPath)
{
    if (!bIsHaveSync_)
    {
        return std::make_pair(false, "fail to open a database connection.");
    }

    {
        std::lock_guard<std::mutex> lock(mutexlistTable_);
        auto iter(listTableInfo_.end());
        iter = std::find_if(listTableInfo_.begin(), listTableInfo_.end(),
            [sPath](std::shared_ptr <TableSyncItem>  pItem) {
            if (pItem->TargetType() == TableSyncItem::SyncTarget_audit && pItem->GetSyncState() != TableSyncItem::SYNC_STOP)
            {
                std::shared_ptr<TableAuditItem> pAuditItem = std::static_pointer_cast<TableAuditItem>(pItem);
                std::string sAuditPath = pAuditItem->GetOutputPath();
                if (sPath == sAuditPath)
                {
                    return true;
                }
            }
			return false;
        });

        if (iter != listTableInfo_.end())
        {
            return std::make_pair(false, "the output file is used by another process.");
        }
    }

    auto ret = CreateOneItem(TableSyncItem::SyncTarget_audit, sPara);
    if (ret.first != NULL)
    {
        std::shared_ptr<TableAuditItem> pAuditItem = std::static_pointer_cast<TableAuditItem>(ret.first);
        auto retPair = pAuditItem->SetAuditPara(sSql, sPath);
        if (!retPair.first)
            return std::make_pair(false, retPair.second);
        else
        {
            std::lock_guard<std::mutex> lock(mutexlistTable_);
            listTableInfo_.push_back(ret.first);

            return std::make_pair(true, retPair.second);
        }
    }

    return std::make_pair(false, ret.second);
}
std::pair<bool, std::string> TableSync::StopAuditTable(std::string sNickName)
{
    std::lock_guard<std::mutex> lock(mutexlistTable_);
    auto iter(listTableInfo_.end());
    iter = std::find_if(listTableInfo_.begin(), listTableInfo_.end(),
        [sNickName](std::shared_ptr <TableSyncItem>  pItem) {
        return pItem->GetNickName() == sNickName && pItem->TargetType() == TableSyncItem::SyncTarget_audit && pItem->GetSyncState() != TableSyncItem::SYNC_STOP;
    });

    if (iter == listTableInfo_.end())
    {
        return std::make_pair(false, "can't find the talbe in audit tasks.");;
    }
    std::shared_ptr<TableAuditItem> pAuditItem = std::static_pointer_cast<TableAuditItem>(*iter);
    auto retStop = pAuditItem->StopTask();
    if (!retStop.first)
    {
        return retStop;
    }

    (*iter)->SetSyncState(TableSyncItem::SYNC_STOP);
    return std::make_pair(true, "");
}

bool TableSync::GetCurrentAuditPos(std::string sNickName, TableSyncItem::taskInfo &info)
{
    std::lock_guard<std::mutex> lock(mutexlistTable_);
    auto iter(listTableInfo_.end());
    iter = std::find_if(listTableInfo_.begin(), listTableInfo_.end(),
        [sNickName](std::shared_ptr <TableSyncItem>  pItem) {
        return pItem->GetNickName() == sNickName && pItem->TargetType() == TableSyncItem::SyncTarget_audit && pItem->GetSyncState() != TableSyncItem::SYNC_STOP;
    });

    if (iter == listTableInfo_.end())  return false;
  
    std::shared_ptr<TableAuditItem> pAuditItem = std::static_pointer_cast<TableAuditItem>(*iter);    
    pAuditItem->GetCurrentPos(info);

    return true;
}

}
