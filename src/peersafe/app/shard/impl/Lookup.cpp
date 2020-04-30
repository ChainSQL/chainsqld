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

#include <peersafe/app/shard/Lookup.h>
#include <peersafe/app/shard/ShardManager.h>
#include <peersafe/app/shard/FinalLedger.h>
#include <ripple/app/ledger/LedgerMaster.h>

#include <ripple/core/Config.h>
#include <ripple/core/ConfigSections.h>

#include <ripple/overlay/Peer.h>
#include <ripple/overlay/impl/PeerImp.h>

#include <ripple/app/misc/Transaction.h>

namespace ripple {

Lookup::Lookup(ShardManager& m, Application& app, Config& cfg, beast::Journal journal)
    : mShardManager(m)
    , app_(app)
    , journal_(journal)
    , cfg_(cfg)
{
    // TODO initial peers and validators

	mValidators = std::make_unique<ValidatorList>(
			app_.validatorManifests(), app_.publisherManifests(), app_.timeKeeper(),
			journal_, cfg_.VALIDATION_QUORUM);


	std::vector<std::string>  publisherKeys;
	// Setup trusted validators
	if (!mValidators->load(
		app_.getValidationPublicKey(),
		cfg_.section(SECTION_LOOKUP_PUBLIC_KEYS).values(),
		publisherKeys))
	{
		//JLOG(m_journal.fatal()) <<
		//	"Invalid entry in validator configuration.";
		//return false;
	}

}

void Lookup::checkSaveLedger()
{
	LedgerIndex ledgerIndexToSave = app_.getLedgerMaster().getValidLedgerIndex() + 1;
	if (mShardManager.shardCount() == mMapMicroLedgers[ledgerIndexToSave].size())
	{
		//reset tx-meta transactionIndex field
		resetMetaIndex(ledgerIndexToSave);

		//save this ledger
		saveLedger(ledgerIndexToSave);
	}
}

void Lookup::resetMetaIndex(LedgerIndex seq)
{
	auto vecHashes = mMapFinalLedger[seq]->getTxHashes();
	for (int i = 0; i < vecHashes.size(); i++)
	{
		for (int shardIndex = 1; shardIndex < mShardManager.shardCount(); shardIndex++)
		{
			if (mMapMicroLedgers[seq][shardIndex]->hasTxWithMeta(vecHashes[i]))
				mMapMicroLedgers[seq][shardIndex]->setMetaIndex(vecHashes[i], i,app_.journal("TxMeta"));
		}
	}
}

void Lookup::saveLedger(LedgerIndex seq)
{
	auto finalLedger = mMapFinalLedger[seq];
	//build new ledger
	auto previousLedger = app_.getLedgerMaster().getValidatedLedger();
	auto ledgerInfo = finalLedger->getLedgerInfo();
	auto ledgerToSave =
		std::make_shared<Ledger>(*previousLedger, ledgerInfo.closeTime);
	//apply state
	finalLedger->getRawStateTable().apply(*ledgerToSave);
	//build tx-map
	for (auto const& item : finalLedger->getTxHashes())
	{
		for (int shardIndex = 1; shardIndex < mShardManager.shardCount(); shardIndex++)
		{
			if (mMapMicroLedgers[seq][shardIndex]->hasTxWithMeta(item))
			{
				auto txWithMeta = mMapMicroLedgers[seq][shardIndex]->getTxWithMeta(item);
				auto tx = txWithMeta.first;
				auto meta = txWithMeta.second;
				//txWithMeta.second->getAsObject().add(*meta);
				ledgerToSave->rawTxInsert(item, tx, meta);
			}
		}
		
	}


	//check hash
	assert(ledgerInfo.accountHash == ledgerToSave->stateMap().getHash().as_uint256());

	ledgerToSave->updateSkipList();
	{
		// Write the final version of all modified SHAMap
		// nodes to the node store to preserve the new Ledger
		int asf = ledgerToSave->stateMap().flushDirty(
			hotACCOUNT_NODE, ledgerToSave->info().seq);
		int tmf = ledgerToSave->txMap().flushDirty(
			hotTRANSACTION_NODE, ledgerToSave->info().seq);
		JLOG(journal_.debug()) << "Flushed " << asf << " accounts and " << tmf
			<< " transaction nodes";
	}
	ledgerToSave->unshare();
	ledgerToSave->setAccepted(ledgerInfo.closeTime, ledgerInfo.closeTimeResolution, true, app_.config());

	//save ledger
	app_.getLedgerMaster().accept(ledgerToSave);
}

void Lookup::timerEntry()
{

	if (!mTransactions.empty()) {

		app_.getJobQueue().addJob(jtRELAYTXS, "Lookup.relayTxs",
			[this](Job&) {
			relayTxs();
		});
	}
}

void Lookup::relayTxs()
{

	std::lock_guard <decltype(mTransactionsMutex)> lock(mTransactionsMutex);
	std::map < unsigned int, protocol::TMTransactions > mapShardIndexTxs;

	for (auto tx : mTransactions) {


		// tx shard
		auto txCur = tx->getSTransaction();
		auto account = txCur->getAccountID(sfAccount);
		std::string strAccountID = toBase58(account);
		auto shardIndex			 = getTxShardIndex(strAccountID, mShardManager.shardCount());

		Serializer s;
		tx->getSTransaction()->add(s);

		auto item = mapShardIndexTxs.find(shardIndex);
		if (item != mapShardIndexTxs.end()) {

			protocol::TMTransaction& tmTx= *(item->second).add_transactions();
			tmTx.set_rawtransaction(s.data(), s.size());
			tmTx.set_status(protocol::tsCURRENT);
			tmTx.set_receivetimestamp(app_.timeKeeper().now().time_since_epoch().count());

		}
		else {

			protocol::TMTransactions txs;
			protocol::TMTransaction& tmTx = *txs.add_transactions();
			tmTx.set_rawtransaction(s.data(), s.size());
			tmTx.set_status(protocol::tsCURRENT);
			tmTx.set_receivetimestamp(app_.timeKeeper().now().time_since_epoch().count());

			mapShardIndexTxs[shardIndex] = txs;
		}

	}


	for (auto item : mapShardIndexTxs) {
		mShardManager.node().sendTransaction(item.first, item.second);
	}


	mTransactions.clear();
}

void Lookup::addTxs(std::vector< std::shared_ptr<Transaction> >& txs)
{

	std::lock_guard <decltype(mTransactionsMutex)> lock(mTransactionsMutex);
	mTransactions.insert(mTransactions.end(), txs.begin(), txs.end());
}

void Lookup::addActive(std::shared_ptr<PeerImp> const& peer)
{
	std::lock_guard <decltype(mPeersMutex)> lock(mPeersMutex);
	mPeers.emplace_back(std::move(peer));
}

void Lookup::eraseDeactivate(Peer::id_t id)
{
	std::lock_guard <decltype(mPeersMutex)> lock(mPeersMutex);

	auto position = mPeers.begin();
	while (position != mPeers.end()) {

		auto spt = position->lock();
		if (spt->id() == id) {
			mPeers.erase(position);
			break;
		}

		position++;
	}
}

void Lookup::onMessage(protocol::TMMicroLedgerSubmit const& m)
{
	auto microWithMeta = std::make_shared<MicroLedger>(m);
	bool valid = microWithMeta->checkValidity(
        mShardManager.node().shardValidators().at(microWithMeta->shardID()),
		true);
	if (!valid)
	{
		return;
	}

	mMapMicroLedgers[microWithMeta->seq()][microWithMeta->shardID()] = microWithMeta;
	checkSaveLedger();
}

void Lookup::onMessage(protocol::TMFinalLedgerSubmit const& m)
{
	auto finalLedger = std::make_shared<FinalLedger>(m);
	bool valid = finalLedger->checkValidity(mShardManager.committee().validatorsPtr());

	if (valid)
	{
		mMapFinalLedger.emplace(finalLedger->getLedgerInfo().seq, finalLedger);
	}
	checkSaveLedger();
}



//void ShardManager::addActive(std::shared_ptr<PeerImp> const& peer)
//{
//	std::uint32_t peerRole = peer->getShardRole();
//	switch (peerRole) {
//		case (std::uint32_t)(ShardManager::LOOKUP) : {
//			mLookup->addActive(peer);
//		}
//													 break;
//													 case (std::uint32_t)(ShardManager::SHARD) : {
//														 mNode->addActive(peer);
//													 }
//																								 break;
//																								 case (std::uint32_t)(ShardManager::COMMITTEE) : {
//																									 mCommittee->addActive(peer);
//																								 }
//																																				 break;
//																																				 case (std::uint32_t)(ShardManager::SYNC) : {
//																																					 mSync->addActive(peer);
//																																				 }
//																																															break;
//																																				 default:
//																																					 break;
//	}
//}
//

//void ShardManager::relayTxs(std::vector< std::shared_ptr<Transaction> >& txs)
//{
//	for (auto tx : txs) {
//
//		auto txCur = tx->getSTransaction();
//		auto account = txCur->getAccountID(sfAccount);
//
//		std::string strAccountID = toBase58(account);
//		auto shardIndex = getShardIndex(strAccountID, shardCount());
//
//		protocol::TMTransaction msg;
//		Serializer s;
//
//		tx->getSTransaction()->add(s);
//		msg.set_rawtransaction(s.data(), s.size());
//		msg.set_status(protocol::tsCURRENT);
//		msg.set_receivetimestamp(app_.timeKeeper().now().time_since_epoch().count());
//
//		mNode->sendTransaction(shardIndex, msg);
//
//	}
//
//}



}
