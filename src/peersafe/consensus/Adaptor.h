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


#ifndef PEERSAFE_CONSENSUS_ADAPTOR_H_INCLUDE
#define PEERSAFE_CONSENSUS_ADAPTOR_H_INCLUDE


#include "ripple.pb.h"
#include <ripple/core/TimeKeeper.h>
#include <ripple/protocol/digest.h>
#include <ripple/app/consensus/RCLCxLedger.h>
#include <ripple/app/consensus/RCLCxPeerPos.h>
#include <ripple/app/consensus/RCLCxTx.h>
#include <ripple/app/consensus/RCLValidations.h>
#include <ripple/app/ledger/InboundTransactions.h>
#include <ripple/app/ledger/OpenLedger.h>
#include <ripple/app/ledger/LedgerMaster.h>
#include <ripple/app/misc/FeeVote.h>
#include <ripple/app/misc/NegativeUNLVote.h>
#include <ripple/app/misc/ValidatorList.h>
#include <peersafe/consensus/ConsensusTypes.h>
#include <peersafe/app/util/Common.h>
#include <peersafe/app/misc/TxPool.h>
#include <peersafe/protocol/STInitAnnounce.h>
#include <boost/optional.hpp>
#include <memory>


namespace ripple {

class Peer;
class ValidatorKeys;
class LocalTxs;


// Implements the Adaptor template interface required by Consensus.
class Adaptor
{
public:
    using Ledger_t = RCLCxLedger;
    using NodeID_t = NodeID;
    using NodeKey_t = PublicKey;
    using TxSet_t = RCLTxSet;
    using PeerPosition_t = RCLCxPeerPos;
    using Result = ConsensusResult<Adaptor>;

public:
    Schema& app_;
    beast::Journal j_;
    std::unique_ptr<FeeVote> feeVote_;
    LedgerMaster& ledgerMaster_;
    InboundTransactions& inboundTransactions_;

    NodeID const nodeID_;
    PublicKey const valPublic_;
    SecretKey const valSecret_;

    // A randomly selected non-zero value used to tag our validations
    std::uint64_t const valCookie_;

    // Ledger we most recently needed to acquire
    LedgerHash acquiringLedger_;

    // These members are queried via public accesors and are atomic for
    // thread safety.
    std::atomic<bool> validating_{false};
    std::atomic<std::size_t> prevProposers_{0};
    std::atomic<std::chrono::milliseconds> prevRoundTime_{
        std::chrono::milliseconds{0}};
    std::atomic<ConsensusMode> mode_{ConsensusMode::observing};

    NegativeUNLVote nUnlVote_;

    // The timestamp of the last validation we used
    NetClock::time_point lastValidationTime_;

    LocalTxs& localTxs_;

public:
    Adaptor(
        Schema& app,
        std::unique_ptr<FeeVote>&& feeVote,
        LedgerMaster& ledgerMaster,
        InboundTransactions& inboundTransactions,
        ValidatorKeys const& validatorKeys,
        beast::Journal journal,
        LocalTxs& localTxs);

    virtual ~Adaptor() = default;

    inline NodeID_t const&
    nodeID() const
    {
        return nodeID_;
    }

    inline PublicKey const&
    valPublic() const
    {
        return valPublic_;
    }

    inline SecretKey const&
    valSecret() const
    {
        return valSecret_;
    }

    inline bool
    validating() const
    {
        return validating_;
    }

    inline std::size_t
    prevProposers() const
    {
        return prevProposers_;
    }

    inline std::chrono::milliseconds
    prevRoundTime() const
    {
        return prevRoundTime_;
    }

    inline ConsensusMode
    mode() const
    {
        return mode_;
    }

    inline bool
    haveValidated() const
    {
        return ledgerMaster_.haveValidated();
    };

    inline LedgerIndex
    getValidLedgerIndex() const
    {
        return ledgerMaster_.getValidLedgerIndex();
    }

    inline std::shared_ptr<Ledger const>
    getValidatedLedger() const
    {
        return ledgerMaster_.getValidatedLedger();
    }

    inline std::shared_ptr<Ledger const>
    getLedgerByHash(uint256 const& hash) const
    {
        return ledgerMaster_.getLedgerByHash(hash);
    }

    inline std::shared_ptr<ReadView const>
    getCurrentLedger()
    {
        return app_.openLedger().current();
    }

    inline NetClock::time_point
    closeTime() const
    {
        return app_.timeKeeper().closeTime();
    }

    inline std::size_t
    getQuorum() const
    {
        return app_.validators().quorum();
    }

    inline PublicKey
    getMasterKey(PublicKey pk) const
    {
        return app_.validatorManifests().getMasterKey(pk);
    }

    inline boost::optional<PublicKey>
    getTrustedKey(PublicKey const& identity) const
    {
        return app_.validators().getTrustedKey(identity);
    }

    inline bool
    trusted(PublicKey const& identity) const
    {
        return app_.validators().trusted(identity);
    }

    inline int
    getPubIndex(PublicKey const& publicKey)
    {
        return app_.validators().getPubIndex(publicKey);
    }

    inline int
    getPubIndex()
    {
        return getPubIndex(valPublic_);
    }

    inline int
    getPubIndex(boost::optional<PublicKey> const& publicKey)
    {
        return publicKey ? getPubIndex(*publicKey) : 0;
    }

    inline Config&
    getAppConfig()
    {
        return app_.config();
    }

    inline JobQueue&
    getJobQueue() const
    {
        return app_.getJobQueue();
    }

    // Transaction pool interfaces for consensus
    inline bool
    isPoolAvailable()
    {
        return app_.getTxPool().isAvailable();
    }
    inline std::size_t
    getPoolTxCount()
    {
        return app_.getTxPool().getTxCountInPool();
    }
    inline std::size_t
    getPoolQueuedTxCount()
    {
        return app_.getTxPool().getQueuedTxCountInPool();
    }
    inline std::uint64_t
    topTransactions(uint64_t limit, LedgerIndex seq, H256Set& set)
    {
        return app_.getTxPool().topTransactions(limit, seq, set);
    }
    inline void
    removePoolTxs(
        SHAMap const& cSet,
        LedgerIndex ledgerSeq,
        uint256 const& prevHash)
    {
        app_.getTxPool().removeTxs(cSet, ledgerSeq, prevHash);
    }
    inline void
    updatePoolAvoid(SHAMap const& map, LedgerIndex seq)
    {
        app_.getTxPool().updateAvoid(map, seq);
    }
    inline void
    clearPoolAvoid(LedgerIndex seq)
    {
        app_.getTxPool().clearAvoid(seq);
    }
    inline Json::Value
    getSyncStatusJson()
    {
        return app_.getTxPool().syncStatusJson();
    }


    /** Share the given tx set to peers.

        @param set The TxSet to share.
    */
    inline void
    share(RCLTxSet const& set)
    {
        inboundTransactions_.giveSet(set.id(), set.map_, false);
    }

    /**
     * Determines how many validations are needed to fully validate a ledger
     *
     * @return Number of validations needed
     */
    inline std::size_t
    getNeededValidations()
    {
        return app_.config().standalone() ? 0 : app_.validators().quorum();
    }

    /** Called before kicking off a new consensus round.

        @param prevLedger Ledger that will be prior ledger for next round
        @return Whether we enter the round proposing
    */
    virtual bool
    preStartRound(
        RCLCxLedger const& prevLedger,
        hash_set<NodeID> const& nowTrusted);

    /** Notify peers of a consensus state change

        @param ne Event type for notification
        @param ledger The ledger at the time of the state change
        @param haveCorrectLCL Whether we believ we have the correct LCL.
    */
    virtual void
    notify(
        protocol::NodeEvent ne,
        RCLCxLedger const& ledger,
        bool haveCorrectLCL);

    virtual void
    InitAnnounce(
        STInitAnnounce const& initAnnounce,
        boost::optional<PublicKey> pubKey = boost::none);

    void
    signMessage(protocol::TMConsensus& consensus);

    void
    signAndSendMessage(protocol::TMConsensus& consensus);
    void
    signAndSendMessage(
        std::shared_ptr<Peer> peer,
        protocol::TMConsensus& consensus);
    void
    signAndSendMessage(
        PublicKey const& pubKey,
        protocol::TMConsensus& consensus);

    /** Acquire the transaction set associated with a proposal.

        If the transaction set is not available locally, will attempt
        acquire it from the network.

        @param setId The transaction set ID associated with the proposal
        @return Optional set of transactions, seated if available.
    */
    boost::optional<RCLTxSet>
    acquireTxSet(RCLTxSet::ID const& setId);

    /** Attempt to acquire a specific ledger.

        If not available, asynchronously acquires from the network.

        @param hash The ID/hash of the ledger acquire
        @return Optional ledger, will be seated if we locally had the ledger
    */
    virtual boost::optional<RCLCxLedger>
    acquireLedger(LedgerHash const& hash);

    virtual void
    touchAcquringLedger(LedgerHash const& prevLedgerHash);

    /** Build the new last closed ledger.

        Accept the given the provided set of consensus transactions and
        build the last closed ledger. Since consensus just agrees on which
        transactions to apply, but not whether they make it into the closed
        ledger, this function also populates retriableTxs with those that
        can be retried in the next round.

        @param previousLedger Prior ledger building upon
        @param retriableTxs On entry, the set of transactions to apply to
                            the ledger; on return, the set of transactions
                            to retry in the next round.
        @param closeTime The time the ledger closed
        @param closeTimeCorrect Whether consensus agreed on close time
        @param closeResolution Resolution used to determine consensus close
                                time
        @param roundTime Duration of this consensus rorund
        @param failedTxs Populate with transactions that we could not
                            successfully apply.
        @return The newly built ledger
    */
    virtual RCLCxLedger
    buildLCL(
        RCLCxLedger const& previousLedger,
        CanonicalTXSet& retriableTxs,
        NetClock::time_point closeTime,
        bool closeTimeCorrect,
        NetClock::duration closeResolution,
        std::chrono::milliseconds roundTime,
        std::set<TxID>& failedTxs);

    virtual std::shared_ptr<Ledger const>
    checkLedgerAccept(LedgerInfo const& info);

    inline void
    doValidLedger(std::shared_ptr<Ledger const> const& ledger)
    {
        ledgerMaster_.doValid(ledger);
        ledgerMaster_.onConsensusReached(false, nullptr);
    }

    /** Notified of change in consensus mode

        @param before The prior consensus mode
        @param after The new consensus mode
    */
    void
    onModeChange(ConsensusMode before, ConsensusMode after);

    virtual TrustChanges
    onConsensusReached(
        bool waitingConsensusReach,
        Ledger_t previousLedger,
        uint64_t curTurn);

private:
};


// Helper class to ensure adaptor is notified whenver the ConsensusMode
// changes
class MonitoredMode
{
    ConsensusMode mode_;

public:
    MonitoredMode(ConsensusMode m) : mode_{m}
    {
    }

    ConsensusMode
    get() const
    {
        return mode_;
    }

    void
    set(ConsensusMode mode, Adaptor& a)
    {
        a.onModeChange(mode_, mode);
        mode_ = mode;
    }
};


inline uint256
consensusMessageUniqueId(protocol::TMConsensus const& m)
{
    return sha512Half(
        makeSlice(m.msg()), PublicKey{makeSlice(m.signerpubkey())});
}

}

#endif
