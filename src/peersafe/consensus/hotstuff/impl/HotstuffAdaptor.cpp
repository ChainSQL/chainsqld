//------------------------------------------------------------------------------
/*
    This file is part of rippled: https://github.com/ripple/rippled
    Copyright (c) 2012, 2013 Ripple Labs Inc.

    Permission to use, copy, modify, and/or distribute this software for any
    purpose  with  or without fee is hereby granted, provided that the above
    copyright notice and this permission notice appear in all copies.

    THE  SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
    WITH  REGARD  TO  THIS  SOFTWARE  INCLUDING  ALL  IMPLIED  WARRANTIES  OF
    MERCHANTABILITY  AND  FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
    ANY  SPECIAL ,  DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
    WHATSOEVER  RESULTING  FROM  LOSS  OF USE, DATA OR PROFITS, WHETHER IN AN
    ACTION  OF  CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
    OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
//==============================================================================


#include <ripple/basics/make_lock.h>
#include <ripple/core/ConfigSections.h>
#include <ripple/app/ledger/TransactionMaster.h>
#include <ripple/app/ledger/LocalTxs.h>
#include <peersafe/consensus/ConsensusBase.h>
#include <peersafe/consensus/hotstuff/HotstuffAdaptor.h>
#include <peersafe/serialization/hotstuff/ExecutedBlock.h>


namespace ripple {


HotstuffAdaptor::HotstuffAdaptor(
    Application& app,
    std::unique_ptr<FeeVote>&& feeVote,
    LedgerMaster& ledgerMaster,
    InboundTransactions& inboundTransactions,
    ValidatorKeys const & validatorKeys,
    beast::Journal journal,
    LocalTxs& localTxs)
    : Adaptor(
        app,
        std::move(feeVote),
        ledgerMaster,
        inboundTransactions,
        validatorKeys,
        journal,
        localTxs)
{
    if (app_.config().exists(SECTION_HCONSENSUS))
    {
        parms_.consensusTIMEOUT = std::chrono::seconds{
            std::max(
                (int)parms_.consensusTIMEOUT.count(),
                app.config().loadConfig(SECTION_HCONSENSUS, "time_out", 0)) };
    }
}

HotstuffAdaptor::Author HotstuffAdaptor::GetValidProposer(Round round) const
{
    auto const& validators = app_.validators().validators();
    assert(validators.size() > 0);

    return validators[round % validators.size()];
}

std::shared_ptr<SHAMap> HotstuffAdaptor::onExtractTransactions(RCLCxLedger const& prevLedger, ConsensusMode mode)
{
    const bool wrongLCL = mode == ConsensusMode::wrongLedger;
    const bool proposing = mode == ConsensusMode::proposing;

    notify(protocol::neCLOSING_LEDGER, prevLedger, !wrongLCL);

    // Tell the ledger master not to acquire the ledger we're probably building
    ledgerMaster_.setBuildingLedger(prevLedger.seq() + 1);

    H256Set txs;
    topTransactions(parms_.maxTXS_IN_LEDGER, prevLedger.seq() + 1, txs);

    auto initialSet = std::make_shared<SHAMap>(
        SHAMapType::TRANSACTION, app_.family(), SHAMap::version{ 1 });
    initialSet->setUnbacked();

    // Build SHAMap containing all transactions in our open ledger
    for (auto const& txID : txs)
    {
        auto tx = app_.getMasterTransaction().fetch(txID, false);
        if (!tx)
        {
            JLOG(j_.error()) << "fetch transaction " + to_string(txID) + "failed";
            continue;
        }

        JLOG(j_.trace()) << "Adding open ledger TX " << txID;
        Serializer s(2048);
        tx->getSTransaction()->add(s);
        initialSet->addItem(
            SHAMapItem(tx->getID(), std::move(s)),
            true,
            false);
    }

    // Add pseudo-transactions to the set
    if ((app_.config().standalone() || (proposing && !wrongLCL)) &&
        ((prevLedger.seq() % 256) == 0))
    {
        //TODO
    }

    // Now we need an immutable snapshot
    return std::move(initialSet->snapShot(false));
}

void HotstuffAdaptor::broadcast(STProposal const& proposal)
{
    Blob p = proposal.getSerialized();

    protocol::TMConsensus consensus;

    consensus.set_msg(&p[0], p.size());
    consensus.set_msgtype(ConsensusMessageType::mtPROPOSAL);

    JLOG(j_.info()) << "broadcast PROPOSAL";

    signAndSendMessage(consensus);
}

void HotstuffAdaptor::broadcast(STVote const& vote)
{
    Blob v = vote.getSerialized();

    protocol::TMConsensus consensus;

    consensus.set_msg(&v[0], v.size());
    consensus.set_msgtype(ConsensusMessageType::mtVOTE);

    JLOG(j_.info()) << "broadcast VOTE";

    signAndSendMessage(consensus);
}

void HotstuffAdaptor::sendVote(PublicKey const& pubKey, STVote const& vote)
{
    Blob v = vote.getSerialized();

    protocol::TMConsensus consensus;

    consensus.set_msg(&v[0], v.size());
    consensus.set_msgtype(ConsensusMessageType::mtVOTE);

    JLOG(j_.info()) << "send VOTE to leader " << pubKey;

    signAndSendMessage(pubKey, consensus);
}

void HotstuffAdaptor::acquireBlock(PublicKey const& pubKey, uint256 const& hash)
{
    protocol::TMConsensus consensus;

    consensus.set_msg(hash.data(), hash.bytes);
    consensus.set_msgtype(ConsensusMessageType::mtACQUIREBLOCK);

    JLOG(j_.info()) << "acquiring Executedblock " << hash << " from " << pubKey;

    signAndSendMessage(pubKey, consensus);
}

void HotstuffAdaptor::sendBLock(std::shared_ptr<PeerImp> peer, hotstuff::ExecutedBlock const& block)
{
    protocol::TMConsensus consensus;

    Buffer b(std::move(serialization::serialize(block)));

    consensus.set_msg(b.data(), b.size());
    consensus.set_msgtype(ConsensusMessageType::mtBLOCKDATA);

    JLOG(j_.info()) << "send ExecutedBlock to peer " << peer->getNodePublic();

    signMessage(consensus);

    auto const m = std::make_shared<Message>(
        consensus, protocol::mtCONSENSUS);

    peer->send(m);
}

bool HotstuffAdaptor::doAccept(typename Ledger_t::ID const& lgrId)
{
    auto ledger = ledgerMaster_.getLedgerByHash(lgrId);
    if (!ledger)
    {
        return false;
    }

    if (ledgerMaster_.getCurrentLedger()->seq() < ledger->seq() + 1)
    {
        {
            // Build new open ledger
            auto lock = make_lock(app_.getMasterMutex(), std::defer_lock);
            auto sl = make_lock(ledgerMaster_.peekMutex(), std::defer_lock);
            std::lock(lock, sl);

            auto const lastVal = ledgerMaster_.getValidatedLedger();
            boost::optional<Rules> rules;
            if (lastVal)
                rules.emplace(*lastVal, app_.config().features);
            else
                rules.emplace(app_.config().features);

            CanonicalTXSet retriableTxs{ beast::zero };
            app_.openLedger().accept(
                app_,
                *rules,
                ledger,
                localTxs_.getTxSet(),
                false,
                retriableTxs,
                tapNONE,
                "consensus",
                [&](OpenView& view, beast::Journal j) {
                // Stuff the ledger with transactions from the queue.
                return app_.getTxQ().accept(app_, view);
            });
        }

        ledgerMaster_.updateConsensusTime();

        // Tell directly connected peers that we have a new LCL
        notify(protocol::neACCEPTED_LEDGER, ledger, mode_ != ConsensusMode::wrongLedger);

        ledgerMaster_.switchLCL(ledger);

        updatePoolAvoid(ledger->txMap(), ledger->seq());

        app_.getOPs().endConsensus();
    }

    return true;
}

}