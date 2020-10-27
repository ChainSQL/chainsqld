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


#include <peersafe/consensus/hotstuff/impl/Config.h>
#include <peersafe/consensus/hotstuff/impl/RecoverData.h>
#include <peersafe/consensus/hotstuff/HotstuffConsensus.h>


namespace ripple {


HotstuffConsensus::HotstuffConsensus(
    ripple::Adaptor& adaptor,
    clock_type const& clock,
    beast::Journal j)
    : ConsensusBase(clock, j)
    , adaptor_(*(HotstuffAdaptor*)(&adaptor))
{
    hotstuff::Config config;

    config.id = adaptor_.valPublic();

    // TODO
    // HotstuffConsensusParms to hotstuff::Config

    hotstuff_ = hotstuff::Hotstuff::Builder(adaptor_.getIOService(), j)
        .setConfig(config)
        .setCommandManager(this)
        .setStateCompute(this)
        .setValidatorVerifier(this)
        .setNetWork(this)
        .setProposerElection(&adaptor_)
        .build();
}

void HotstuffConsensus::startRound(
    NetClock::time_point const& now,
    typename Ledger_t::ID const& prevLedgerID,
    Ledger_t prevLedger,
    hash_set<NodeID> const& nowUntrusted,
    bool proposing)
{
    ConsensusMode startMode = proposing ? ConsensusMode::proposing : ConsensusMode::observing;

    newRound(prevLedgerID, prevLedger, startMode);

    hotstuff_->start(hotstuff::RecoverData{ prevLedger.ledger_->info() });
}

void HotstuffConsensus::timerEntry(NetClock::time_point const& now)
{
    (void)now;

    if (mode_.get() == ConsensusMode::wrongLedger)
    {
        if (auto newLedger = adaptor_.acquireLedger(prevLedgerID_))
        {
            JLOG(j_.info()) << "Have the consensus ledger " << newLedger->seq() << ":" << prevLedgerID_;
            adaptor_.removePoolTxs(
                newLedger->ledger_->txMap(),
                newLedger->ledger_->info().seq,
                newLedger->ledger_->info().parentHash);

            newRound(prevLedgerID_, *newLedger, ConsensusMode::switchedLedger);
        }
        return;
    }
}

bool HotstuffConsensus::peerConsensusMessage(
    std::shared_ptr<PeerImp>& peer,
    bool isTrusted,
    std::shared_ptr<protocol::TMConsensus> const& m)
{
    switch (m->msgtype())
    {
    case ConsensusMessageType::mtPROPOSAL:
        peerProposal(peer, isTrusted, m);
    case ConsensusMessageType::mtVOTE:
        peerVote(peer, isTrusted, m);
    default:
        break;
    }

    return false;
}

void HotstuffConsensus::gotTxSet(NetClock::time_point const& now, TxSet_t const& txSet)
{
    (void)now;

    auto id = txSet.id();

    if (acquired_.find(id) == acquired_.end())
    {
        acquired_.emplace(id, txSet);
    }

    for (auto it : proposalCache_[id])
    {
        hotstuff_->vote(it.second->block());
    }
}

Json::Value HotstuffConsensus::getJson(bool full) const
{
    Json::Value ret(Json::objectValue);

    return ret;
}

boost::optional<hotstuff::Command> HotstuffConsensus::extract(hotstuff::BlockData &blockData)
{
    if (!adaptor_.isPoolAvailable())
    {
        return boost::none;
    }

    auto txSet = adaptor_.onExtractTransactions(previousLedger_, mode_.get());

    uint256 cmd = txSet->getHash().as_uint256();

    if (acquired_.emplace(cmd, txSet).second)
    {
        adaptor_.relay(txSet);
    }

    //--------------------------------------------------------------------
    std::set<TxID> failed;

    // Put transactions into a deterministic, but unpredictable, order
    CanonicalTXSet retriableTxs{ cmd };
    JLOG(j_.debug()) << "Building canonical tx set: " << retriableTxs.key();

    for (auto const& item : *txSet)
    {
        try
        {
            retriableTxs.insert(std::make_shared<STTx const>(SerialIter{ item.slice() }));
            JLOG(j_.debug()) << "    Tx: " << item.key();
        }
        catch (std::exception const&)
        {
            failed.insert(item.key());
            JLOG(j_.warn()) << "    Tx: " << item.key() << " throws!";
        }
    }

    auto built = adaptor_.buildLCL(
        previousLedger_,
        retriableTxs,
        adaptor_.closeTime(),
        true,
        closeResolution_,
        std::chrono::milliseconds{ 0 },
        failed);

    blockData.getLedgerInfo() = built.ledger_->info();

    return cmd;
}

bool HotstuffConsensus::compute(const hotstuff::Block& block, LedgerInfo& result)
{
    if (block.block_data().epoch == 0 && block.block_data().round == 0)
    {
        return true;
    }

    auto payload = block.block_data().payload;
    if (!payload)
    {
        JLOG(j_.warn()) << "Proposal missing payload";
        return false;
    }

    auto const ait = acquired_.find(payload->cmd);
    if (ait == acquired_.end())
    {
        JLOG(j_.warn()) << "txSet " << payload->cmd << " not acquired";
        return false;
    }

    //--------------------------------------------------------------------
    std::set<TxID> failed;

    // Put transactions into a deterministic, but unpredictable, order
    CanonicalTXSet retriableTxs{ payload->cmd };
    JLOG(j_.debug()) << "Building canonical tx set: " << retriableTxs.key();

    for (auto const& item : *ait->second.map_)
    {
        try
        {
            retriableTxs.insert(std::make_shared<STTx const>(SerialIter{ item.slice() }));
            JLOG(j_.debug()) << "    Tx: " << item.key();
        }
        catch (std::exception const&)
        {
            failed.insert(item.key());
            JLOG(j_.warn()) << "    Tx: " << item.key() << " throws!";
        }
    }

    LedgerInfo const& info = block.getLedgerInfo();

    auto built = adaptor_.buildLCL(
        previousLedger_,
        retriableTxs,
        info.closeTime,
        true,
        info.closeTimeResolution,
        std::chrono::milliseconds{ 0 },
        failed);

    if (info.hash == built.id())
    {
        result = built.ledger_->info();
        // Tell directly connected peers that we have a new LCL
        adaptor_.notify(protocol::neCLOSING_LEDGER, built, adaptor_.mode() != ConsensusMode::wrongLedger);
        return true;
    }

    JLOG(j_.warn()) << "built ledger conflict with proposed ledger" ;

    return false;
}

bool HotstuffConsensus::verify(const LedgerInfo& info, const LedgerInfo& prevInfo)
{
    return info.seq == prevInfo.seq + 1
        && info.parentHash == prevInfo.hash;
}

int HotstuffConsensus::commit(const hotstuff::Block& block)
{
    LedgerInfo const& info = block.getLedgerInfo();

    if (auto ledger = adaptor_.checkLedgerAccept(info))
    {
        adaptor_.doValidLedger(ledger);
    }

    return 0;
}

const hotstuff::Author& HotstuffConsensus::Self() const
{
    return adaptor_.valPublic();
}

bool HotstuffConsensus::signature(const uint256& digest, hotstuff::Signature& signature)
{
    signature = signDigest(adaptor_.valPublic(), adaptor_.valSecret(), digest);
    return true;
}

const bool HotstuffConsensus::verifySignature(
    const hotstuff::Author& author,
    const hotstuff::Signature& signature,
    const uint256& digest) const
{
    return verifyDigest(author, digest, Slice(signature.data(), signature.size()), false);
}

const bool HotstuffConsensus::verifyLedgerInfo(
    const hotstuff::BlockInfo& commit_info,
    const hotstuff::HashValue& consensus_data_hash,
    const std::map<hotstuff::Author, hotstuff::Signature>& signatures)
{
    // 1. Check previous vote whether the consensus threshold has been reached
    for (auto iter = signatures.begin(); iter != signatures.end(); iter++)
    {
        boost::optional<PublicKey> pubKey = adaptor_.getTrustedKey(iter->first);
        if (!pubKey)
        {
            return false;
        }

        if (!verifyDigest(
            iter->first,
            consensus_data_hash,
            Slice(iter->second.data(), iter->second.size()),
            false))
        {
            return false;
        }
    }

    if (signatures.size() < adaptor_.getQuorum())
    {
        return false;
    }

    // 2. Do something check for previous commit_info
    if (commit_info.ledger_info.seq > previousLedger_.seq())
    {
        auto ledger = adaptor_.getLedgerByHash(commit_info.ledger_info.hash);
        if (ledger)
        {
            newRound(commit_info.ledger_info.hash,
                ledger,
                adaptor_.preStartRound(ledger) ? ConsensusMode::proposing : ConsensusMode::observing);
            return true;
        }
        else
        {
            prevLedgerID_ = commit_info.ledger_info.hash;
            //handleWrongLedger(netLgr);

            mode_.set(ConsensusMode::observing, adaptor_);
            JLOG(j_.info()) << "Bowing out of consensus";

            if (auto newLedger = adaptor_.acquireLedger(prevLedgerID_))
            {
                JLOG(j_.info()) << "Have the consensus ledger " << newLedger->seq() << ":" << prevLedgerID_;
                adaptor_.removePoolTxs(
                    newLedger->ledger_->txMap(),
                    newLedger->ledger_->info().seq,
                    newLedger->ledger_->info().parentHash);

                newRound(prevLedgerID_, *newLedger, ConsensusMode::switchedLedger);
            }
            else
            {
                mode_.set(ConsensusMode::wrongLedger, adaptor_);
            }
        }
    }

    return false;
}

const bool HotstuffConsensus::checkVotingPower(const std::map<hotstuff::Author, hotstuff::Signature>& signatures) const
{
    return signatures.size() >= adaptor_.getQuorum();
}

void HotstuffConsensus::broadcast(const hotstuff::Block& block, const hotstuff::SyncInfo& syncInfo)
{
    auto proposal = std::make_shared<STProposal>(block, syncInfo, adaptor_.valPublic());
    
    adaptor_.broadcast(*proposal);

    hotstuff_->vote(block);
}

void HotstuffConsensus::broadcast(const hotstuff::Vote& vote, const hotstuff::SyncInfo& syncInfo)
{
}

void HotstuffConsensus::sendVote(const hotstuff::Author& author, const hotstuff::Vote& vote, const hotstuff::SyncInfo& syncInfo)
{
    if (author == adaptor_.valPublic())
    {
        hotstuff_->handleVote(vote, syncInfo);
    }
    else
    {
        auto stVote = std::make_shared<STVote>(vote, syncInfo, adaptor_.valPublic());
        adaptor_.sendVote(author, *stVote);
    }
}

// -------------------------------------------------------------------
// Private member functions

void HotstuffConsensus::peerProposal(
    std::shared_ptr<PeerImp>& peer,
    bool isTrusted,
    std::shared_ptr<protocol::TMConsensus> const& m)
{
    if (!isTrusted)
    {
        JLOG(j_.warn()) << "drop UNTRUSTED proposal";
        return;
    }

    try
    {
        STProposal::pointer proposal;

        SerialIter sit(makeSlice(m->msg()));
        PublicKey const publicKey{ makeSlice(m->signerpubkey()) };

        proposal = std::make_shared<STProposal>(sit, publicKey);

        // Check message public key same with proposal public key.
        if (proposal->nodePublic() != proposal->block().block_data().author())
        {
            JLOG(j_.warn()) << "message public key different with proposal public key, drop proposal";
            return;
        }

        if (hotstuff_->handleProposal(proposal->block(), proposal->syncInfo()) == 0)
        {
            peerProposalInternal(proposal);
        }
    }
    catch (std::exception const& e)
    {
        JLOG(j_.warn()) << "Proposal: Exception, " << e.what();
        peer->charge(Resource::feeInvalidRequest);
    }
}

void HotstuffConsensus::peerProposalInternal(STProposal::ref proposal)
{
    LedgerInfo const& info = proposal->block().getLedgerInfo();

    if (info.seq < previousLedger_.seq() + 1)
    {
        JLOG(j_.warn()) << "proposal is fall behind";
        return;
    }

    if (info.seq > previousLedger_.seq() + 1)
    {
        JLOG(j_.warn()) << "we are fall behind";
        prevLedgerID_ = info.parentHash;
        return;
    }

    auto payload = proposal->block().block_data().payload;
    if (!payload)
    {
        JLOG(j_.warn()) << "proposal missing payload";
        return;
    }

    auto const ait = acquired_.find(payload->cmd);
    if (ait == acquired_.end())
    {
        JLOG(j_.debug()) << "Don't have tx set for proposal:" << payload->cmd;

        proposalCache_[payload->cmd][payload->author] = proposal;

        // acquireTxSet will return the set if it is available, or
        // spawn a request for it and return none/nullptr.  It will call
        // gotTxSet once it arrives
        if (auto set = adaptor_.acquireTxSet(payload->cmd))
        {
            gotTxSet(adaptor_.closeTime(), *set);
        }
        return;
    }

    hotstuff_->vote(proposal->block());
}

void HotstuffConsensus::peerVote(
    std::shared_ptr<PeerImp>& peer,
    bool isTrusted,
    std::shared_ptr<protocol::TMConsensus> const& m)
{
    if (!isTrusted)
    {
        JLOG(j_.warn()) << "drop UNTRUSTED vote";
        return;
    }

    try
    {
        STVote::pointer vote;

        SerialIter sit(makeSlice(m->msg()));
        PublicKey const publicKey{ makeSlice(m->signerpubkey()) };

        vote = std::make_shared<STVote>(sit, publicKey);

        // Check message public key same with vote public key.
        if (vote->nodePublic() != vote->vote().author())
        {
            JLOG(j_.warn()) << "message public key different with vote public key, drop vote";
            return;
        }

        hotstuff_->handleVote(vote->vote(), vote->syncInfo());
    }
    catch (std::exception const& e)
    {
        JLOG(j_.warn()) << "Vote: Exception, " << e.what();
        peer->charge(Resource::feeInvalidRequest);
    }
}

void HotstuffConsensus::newRound(
    RCLCxLedger::ID const& prevLgrId,
    RCLCxLedger const& prevLgr,
    ConsensusMode mode)
{
    mode_.set(mode, adaptor_);

    prevLedgerID_ = prevLgrId;
    previousLedger_ = prevLgr;

    acquired_.clear();
    proposalCache_.clear();
}


} // namespace ripple