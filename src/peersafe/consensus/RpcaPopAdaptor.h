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


#ifndef PEERSAFE_CONSENSUS_RPCAPOPADAPTOR_H_INCLUDE
#define PEERSAFE_CONSENSUS_RPCAPOPADAPTOR_H_INCLUDE


#include <peersafe/consensus/Adaptor.h>
#include <peersafe/protocol/STViewChange.h>
#include <ripple/protocol/STValidationSet.h>

namespace ripple {

class RpcaPopAdaptor : public Adaptor
{
public:
    RpcaPopAdaptor(
        Schema& app,
        std::unique_ptr<FeeVote>&& feeVote,
        LedgerMaster& ledgerMaster,
        InboundTransactions& inboundTransactions,
        ValidatorKeys const& validatorKeys,
        beast::Journal journal,
        LocalTxs& localTxs);

    virtual ~RpcaPopAdaptor() = default;

    /** Process the accepted ledger.

        @param result The result of consensus
        @param prevLedger The closed ledger consensus worked from
        @param closeResolution The resolution used in agreeing on an
                                effective closeTime
        @param rawCloseTimes The unrounded closetimes of ourself and our
                                peers
        @param mode Our participating mode at the time consensus was
                    declared
        @param consensusJson Json representation of consensus state
    */
    void
    onAccept(
        Result const& result,
        RCLCxLedger const& prevLedger,
        NetClock::duration const& closeResolution,
        ConsensusCloseTimes const& rawCloseTimes,
        ConsensusMode const& mode,
        Json::Value&& consensusJson);

    std::shared_ptr<Ledger const>
    checkLedgerAccept(LedgerInfo const& info) override final;

    /** Propose the given position to my peers.

        @param proposal Our proposed position
    */
    void
    propose(RCLCxPeerPos::Proposal const& proposal);

    /** Validate the given ledger and share with peers as necessary

        @param ledger The ledger to validate
        @param txns The consensus transaction set
        @param proposing Whether we were proposing transactions while
                            generating this ledger.  If we are not proposing,
                            a validation can still be sent to inform peers that
                            we know we aren't fully participating in consensus
                            but are still around and trying to catch up.
    */
    void
    validate(RCLCxLedger const& ledger, RCLTxSet const& txns, bool proposing);

    bool
    peerValidation(std::shared_ptr<PeerImp>& peer, STValidation::ref val);

    /** Number of proposers that have validated a ledger descended from
        requested ledger.

        @param ledger The current working ledger
        @param h The hash of the preferred working ledger
        @return The number of validating peers that have validated a ledger
                descended from the preferred working ledger.
    */
    std::size_t
    proposersFinished(RCLCxLedger const& ledger, LedgerHash const& h) const;

    /** Get the ID of the previous ledger/last closed ledger(LCL) on the
        network

        @param ledgerID ID of previous ledger used by consensus
        @param ledger Previous ledger consensus has available
        @param mode Current consensus mode
        @return The id of the last closed network

        @note ledgerID may not match ledger.id() if we haven't acquired
                the ledger matching ledgerID from the network
        */
    uint256
    getPrevLedger(
        uint256 ledgerID,
        RCLCxLedger const& ledger,
        ConsensusMode mode);
    bool
    peerAcquirValidation(STViewChange::ref viewChange);
    bool
    sendAcquirValidation(std::shared_ptr<STValidationSet> const& validationSet);
    bool
    peerValidationData(STValidationSet::ref vaildationSet);
    std::vector<std::shared_ptr<STValidation>>
    getLastValidationsFromDB(std::uint32_t seq, uint256 id);
    std::vector<std::shared_ptr<STValidation>>
    getLastValidations(std::uint32_t seq, uint256 id);

protected:
    /** Report that the consensus process built a particular ledger */
    void
    consensusBuilt(
        std::shared_ptr<Ledger const> const& ledger,
        uint256 const& consensusHash,
        Json::Value consensus);

private:
    /** Accept a new ledger based on the given transactions.

        @ref onAccept
    */
    virtual void
    doAccept(
        Result const& result,
        RCLCxLedger const& prevLedger,
        NetClock::duration closeResolution,
        ConsensusCloseTimes const& rawCloseTimes,
        ConsensusMode const& mode,
        Json::Value&& consensusJson) = 0;

    /** Handle a new validation

        Also sets the trust status of a validation based on the validating
       node's public key and this node's current UNL.

        @param app Application object containing validations and ledgerMaster
        @param val The validation to add
        @param source Name associated with validation used in logging

        @return Whether the validation should be relayed
    */
    bool
    handleNewValidation(STValidation::ref val, std::string const& source);

    auto
    checkLedgerAccept(uint256 const& hash, std::uint32_t seq)
        -> std::pair<std::shared_ptr<Ledger const> const, bool>;   
};

}  // namespace ripple

#endif
