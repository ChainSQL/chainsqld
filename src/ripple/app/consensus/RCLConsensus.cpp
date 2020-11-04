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


#include <ripple/core/ConfigSections.h>
#include <ripple/app/consensus/RCLConsensus.h>
#include <peersafe/consensus/rpca/RpcaAdaptor.h>
#include <peersafe/consensus/rpca/RpcaConsensus.h>
#include <peersafe/consensus/pop/PopAdaptor.h>
#include <peersafe/consensus/pop/PopConsensus.h>
#include <peersafe/consensus/hotstuff/HotstuffAdaptor.h>
#include <peersafe/consensus/hotstuff/HotstuffConsensus.h>


namespace ripple {


RCLConsensus::RCLConsensus(
    Application& app,
    std::unique_ptr<FeeVote>&& feeVote,
    LedgerMaster& ledgerMaster,
    LocalTxs& localTxs,
    InboundTransactions& inboundTransactions,
    ConsensusBase::clock_type const& clock,
    ValidatorKeys const& validatorKeys,
    beast::Journal journal)
    : j_(journal)
{
    if (app.config().exists(SECTION_CONSENSUS))
    {
        parms_.txPOOL_CAPACITY = app.config().loadConfig(SECTION_CONSENSUS, "max_txs_in_pool", parms_.txPOOL_CAPACITY);

        auto result = app.config().section(SECTION_CONSENSUS).find("type");
        if (result.second)
        {
            type_ = stringToConsensusType(result.first);
        }

        JLOG(j_.warn()) << "Consensus engine: " << result.first;
    }

    switch (type_)
    {
    case ConsensusType::RPCA:
        adaptor_ = std::make_shared<RpcaAdaptor>(
            app,
            std::move(feeVote),
            ledgerMaster,
            inboundTransactions,
            validatorKeys,
            journal,
            localTxs);
        consensus_ = std::make_shared<RpcaConsensus>(*adaptor_, clock, journal);
        break;
    case ConsensusType::POP:
        adaptor_ = std::make_shared<PopAdaptor>(
            app,
            std::move(feeVote),
            ledgerMaster,
            inboundTransactions,
            validatorKeys,
            journal,
            localTxs,
            parms_);
        consensus_ = std::make_shared<PopConsensus>(*adaptor_, clock, journal);
        break;
    case ConsensusType::HOTSTUFF:
        adaptor_ = std::make_shared<HotstuffAdaptor>(
            app,
            std::move(feeVote),
            ledgerMaster,
            inboundTransactions,
            validatorKeys,
            journal,
            localTxs);
        consensus_ = std::make_shared<HotstuffConsensus>(*adaptor_, clock, journal);
        break;
    default:
        Throw<std::runtime_error>("bad consensus type");
        break;
    }
}

void RCLConsensus::startRound(
    NetClock::time_point const& now,
    RCLCxLedger::ID const& prevLgrId,
    RCLCxLedger const& prevLgr,
    hash_set<NodeID> const& nowUntrusted)
{
    ScopedLockType _{ mutex_ };
    consensus_->startRound(
        now, prevLgrId, prevLgr, nowUntrusted, adaptor_->preStartRound(prevLgr));
}

void RCLConsensus::timerEntry(NetClock::time_point const& now)
{
    try
    {
        ScopedLockType _{mutex_};
        consensus_->timerEntry(now);
    }
    catch (SHAMapMissingNode const& mn)
    {
        // This should never happen
        JLOG(j_.error()) << "Missing node during consensus process " << mn;
        Rethrow();
    }
}

bool RCLConsensus::peerConsensusMessage(
    std::shared_ptr<PeerImp>& peer,
    bool isTrusted,
    std::shared_ptr<protocol::TMConsensus> const& m)
{
    if (m->msgtype() != ConsensusMessageType::mtBLOCKDATA)
    {
        ScopedLockType _{ mutex_ };
        return consensus_->peerConsensusMessage(peer, isTrusted, m);
    }
    else
    {
        // Don't take lock when handle mtBLOCKDATA message.
        // Fix this special handing use async(currenttly used sync) acquire hotstuff block.
        return consensus_->peerConsensusMessage(peer, isTrusted, m);
    }
}

Json::Value RCLConsensus::getJson(bool full) const
{
    Json::Value ret;
    {
        ScopedLockType _{ mutex_ };
        ret = consensus_->getJson(full);
    }
    ret["validating"] = adaptor_->validating();
    return ret;
}

void RCLConsensus::gotTxSet(NetClock::time_point const& now, RCLTxSet const& txSet)
{
    try
    {
        ScopedLockType _{mutex_};
        consensus_->gotTxSet(now, txSet);
    }
    catch (SHAMapMissingNode const& mn)
    {
        // This should never happen
        JLOG(j_.error()) << "Missing node during consensus process " << mn;
        Rethrow();
    }
}

//! @see Consensus::simulate
void RCLConsensus::simulate(
    NetClock::time_point const& now,
    boost::optional<std::chrono::milliseconds> consensusDelay)
{
    ScopedLockType _{mutex_};
    consensus_->simulate(now, consensusDelay);
}

bool RCLConsensus::checkLedgerAccept(std::shared_ptr<Ledger const> const& ledger)
{
    return !!adaptor_->checkLedgerAccept(ledger->info());
}

ConsensusType RCLConsensus::stringToConsensusType(std::string const& s)
{
    if (s == "RPCA")        return ConsensusType::RPCA;
    if (s == "POP")         return ConsensusType::POP;
    if (s == "HOTSTUFF")    return ConsensusType::HOTSTUFF;

    return ConsensusType::UNKNOWN;
}

}
