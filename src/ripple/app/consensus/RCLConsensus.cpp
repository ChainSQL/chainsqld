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

#include <ripple/app/consensus/RCLConsensus.h>
#include <ripple/core/ConfigSections.h>
#include <algorithm>
#include <mutex>
#include <peersafe/consensus/hotstuff/HotstuffAdaptor.h>
#include <peersafe/consensus/hotstuff/HotstuffConsensus.h>
#include <peersafe/consensus/pop/PopAdaptor.h>
#include <peersafe/consensus/pop/PopConsensus.h>
#include <peersafe/consensus/rpca/RpcaAdaptor.h>
#include <peersafe/consensus/rpca/RpcaConsensus.h>
#include <peersafe/schema/PeerManager.h>
#include <peersafe/schema/Schema.h>
#if USE_TBB
#ifdef _CRTDBG_MAP_ALLOC
#pragma push_macro("free")
#undef free
#include <tbb/parallel_for.h>
#pragma pop_macro("free")
#else
#include <tbb/parallel_for.h>
#endif
#include <peersafe/app/tx/ParallelApply.h>
#include <tbb/blocked_range.h>
#include <tbb/concurrent_vector.h>
#endif

namespace ripple {

RCLConsensus::RCLConsensus(
    Schema& app,
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
        parms_.txPOOL_CAPACITY = app.config().loadConfig(
            SECTION_CONSENSUS, "max_txs_in_pool", parms_.txPOOL_CAPACITY);

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
            consensus_ =
                std::make_shared<RpcaConsensus>(*adaptor_, clock, journal);
            app.getValidations().setValSeqExpires((*(RpcaAdaptor*)&*adaptor_).parms().ledgerMAX_CONSENSUS);
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
            consensus_ =
                std::make_shared<PopConsensus>(*adaptor_, clock, journal);
            app.getValidations().setValSeqExpires((*(PopAdaptor*)&*adaptor_).parms().timeoutCOUNT_ROLLBACK * (*(PopAdaptor*)&*adaptor_).parms().consensusTIMEOUT);
            break;
        case ConsensusType::HOTSTUFF:
            adaptor_ = std::make_shared<HotstuffAdaptor>(
                app,
                std::move(feeVote),
                ledgerMaster,
                inboundTransactions,
                validatorKeys,
                journal,
                localTxs,
                parms_);
            consensus_ =
                std::make_shared<HotstuffConsensus>(*adaptor_, clock, journal);
            app.getValidations().setValSeqExpires((*(HotstuffAdaptor*)&*adaptor_).parms().timeoutCOUNT_ROLLBACK * (*(HotstuffAdaptor*)&*adaptor_).parms().consensusTIMEOUT);
            break;
        default:
            Throw<std::runtime_error>("bad consensus type");
            break;
    }
}

void
RCLConsensus::startRound(
    NetClock::time_point const& now,
    RCLCxLedger::ID const& prevLgrId,
    RCLCxLedger const& prevLgr,
    hash_set<NodeID> const& nowUntrusted,
    hash_set<NodeID> const& nowTrusted)
{
    ScopedLockType _{mutex_};
    consensus_->startRound(
        now,
        prevLgrId,
        prevLgr,
        nowUntrusted,
        adaptor_->preStartRound(prevLgr, nowTrusted));
}

void
RCLConsensus::timerEntry(NetClock::time_point const& now)
{
    try
    {
        ScopedLockType _{mutex_};
        consensus_->timerEntry(now);
    }
    catch (SHAMapMissingNode const& mn)
    {
        // This should never happen
        JLOG(j_.error()) << "Missing node during consensus process "
                         << mn.what();
        Rethrow();
    }
}

bool
RCLConsensus::peerConsensusMessage(
    std::shared_ptr<PeerImp>& peer,
    bool isTrusted,
    std::shared_ptr<protocol::TMConsensus> const& m)
{
    if (m->msgtype() == ConsensusMessageType::mtACQUIREBLOCK)
    {
        // Don't need mutex.
        return consensus_->peerConsensusMessage(peer, isTrusted, m);
    }

    ScopedLockType _{mutex_};
    return consensus_->peerConsensusMessage(peer, isTrusted, m);
}

Json::Value
RCLConsensus::getJson(bool full) const
{
    Json::Value ret;
    {
        ScopedLockType _{mutex_};
        ret = consensus_->getJson(full);
    }
    ret["validating"] = adaptor_->validating();
    ret["tx_pool_capacity"] = parms_.txPOOL_CAPACITY;
    return ret;
}

void
RCLConsensus::gotTxSet(NetClock::time_point const& now, RCLTxSet const& txSet)
{
    try
    {
        std::lock_guard _{mutex_};
        consensus_->gotTxSet(now, txSet);
    }
    catch (SHAMapMissingNode const& mn)
    {
        // This should never happen
        JLOG(j_.error()) << "During consensus gotTxSet: " << mn.what();
        Rethrow();
    }
}

void
RCLConsensus::onDeleteUntrusted(hash_set<NodeID> const& nowUntrusted)
{
    std::lock_guard _{mutex_};
    consensus_->onDeleteUntrusted(nowUntrusted);
}

//! @see Consensus::simulate
void
RCLConsensus::simulate(
    NetClock::time_point const& now,
    boost::optional<std::chrono::milliseconds> consensusDelay)
{
    std::lock_guard _{mutex_};
    consensus_->simulate(now, consensusDelay);
}

bool
RCLConsensus::checkLedgerAccept(std::shared_ptr<Ledger const> const& ledger)
{
    return !!adaptor_->checkLedgerAccept(ledger->info());
}

ConsensusType
RCLConsensus::stringToConsensusType(std::string const& s)
{
    if (s == "RPCA" || s == "rpca")
        return ConsensusType::RPCA;
    if (s == "POP" || s == "pop")
        return ConsensusType::POP;
    if (s == "HOTSTUFF" || s == "hotstuff")
        return ConsensusType::HOTSTUFF;

    return ConsensusType::UNKNOWN;
}

std::string
RCLConsensus::conMsgTypeToStr(ConsensusMessageType t)
{
    switch (t)
    {
        case mtPROPOSESET:
            return "PROPOSESET";
        case mtVALIDATION:
            return "VALIDATION";
        case mtVIEWCHANGE:
            return "VIEWCHANGE";
        case mtPROPOSAL:
            return "PROPOSAL";
        case mtVOTE:
            return "VOTE";
        case mtACQUIREBLOCK:
            return "ACQUIREBLOCK";
        case mtBLOCKDATA:
            return "BLOCKDATA";
        case mtINITANNOUNCE:
            return "INITANNOUNCE";
        case mtEPOCHCHANGE:
            return "EPOCHCHANGE";
        default:
            break;
    }

    return "UNKNOWN";
}

}  // namespace ripple
