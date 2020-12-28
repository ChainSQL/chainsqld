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

#include <BeastConfig.h>
#include <ripple/protocol/st.h>
#include <ripple/app/misc/FeeVote.h>
#include <ripple/app/main/Application.h>
#include <ripple/protocol/STValidation.h>
#include <ripple/basics/BasicConfig.h>
#include <ripple/beast/utility/Journal.h>

namespace ripple {


class FeeVoteImpl : public FeeVote
{
private:
    Setup target_;
    beast::Journal journal_;

public:
    FeeVoteImpl (Setup const& setup, beast::Journal journal);

    void
    doValidation (std::shared_ptr<ReadView const> const& lastClosedLedger,
        STObject& baseValidation) override;

    void
    doVoting (std::shared_ptr<ReadView const> const& lastClosedLedger,
        std::vector<STValidation::pointer> const& parentValidations,
        std::shared_ptr<SHAMap> const& initialPosition) override;
};

//--------------------------------------------------------------------------

FeeVoteImpl::FeeVoteImpl (Setup const& setup, beast::Journal journal)
    : target_ (setup)
    , journal_ (journal)
{
}

void
FeeVoteImpl::doValidation(
    std::shared_ptr<ReadView const> const& lastClosedLedger,
        STObject& baseValidation)
{
    if (lastClosedLedger->fees().base != target_.reference_fee)
    {
        JLOG(journal_.info()) <<
            "Voting for base fee of " << target_.reference_fee;

        baseValidation.setFieldU64 (sfBaseFee, target_.reference_fee);
    }

    if (lastClosedLedger->fees().accountReserve(0) != target_.account_reserve)
    {
        JLOG(journal_.info()) <<
            "Voting for base resrve of " << target_.account_reserve;

        baseValidation.setFieldU32(sfReserveBase, target_.account_reserve);
    }

    if (lastClosedLedger->fees().increment != target_.owner_reserve)
    {
        JLOG(journal_.info()) <<
            "Voting for reserve increment of " << target_.owner_reserve;

        baseValidation.setFieldU32 (sfReserveIncrement,
            target_.owner_reserve);
    }


	if (lastClosedLedger->fees().drops_per_byte != target_.drops_per_byte)
	{
		JLOG(journal_.info()) <<
			"Voting for per zxc size " << target_.drops_per_byte;
		baseValidation.setFieldU64(sfDropsPerByte,
			target_.drops_per_byte);
	}
}

void
FeeVoteImpl::doVoting(
    std::shared_ptr<ReadView const> const& lastClosedLedger,
    std::vector<STValidation::pointer> const& set,
    std::shared_ptr<SHAMap> const& initialPosition)
{
    // LCL must be flag ledger
    assert ((lastClosedLedger->info().seq % 256) == 0);

    detail::VotableInteger<std::uint64_t> baseFeeVote (
        lastClosedLedger->fees().base, target_.reference_fee);

    detail::VotableInteger<std::uint32_t> baseReserveVote (
        lastClosedLedger->fees().accountReserve(0).drops(), target_.account_reserve);

    detail::VotableInteger<std::uint32_t> incReserveVote (
        lastClosedLedger->fees().increment, target_.owner_reserve);

	detail::VotableInteger<std::uint64_t> dropsPerByteVote(
		lastClosedLedger->fees().drops_per_byte, target_.drops_per_byte);

    for (auto const& val : set)
    {
        if (val->isTrusted ())
        {
            if (val->isFieldPresent (sfBaseFee))
            {
                baseFeeVote.addVote (val->getFieldU64 (sfBaseFee));
            }
            else
            {
                baseFeeVote.noVote ();
            }

            if (val->isFieldPresent (sfReserveBase))
            {
                baseReserveVote.addVote (val->getFieldU32 (sfReserveBase));
            }
            else
            {
                baseReserveVote.noVote ();
            }

            if (val->isFieldPresent (sfReserveIncrement))
            {
                incReserveVote.addVote (val->getFieldU32 (sfReserveIncrement));
            }
            else
            {
                incReserveVote.noVote ();
            }

			if (val->isFieldPresent(sfDropsPerByte))
			{
				dropsPerByteVote.addVote(val->getFieldU64(sfDropsPerByte));
			}
			else
			{
				dropsPerByteVote.noVote();
			}
        }
    }

    // choose our positions
    std::uint64_t const baseFee = baseFeeVote.getVotes ();
    std::uint32_t const baseReserve = baseReserveVote.getVotes ();
    std::uint32_t const incReserve = incReserveVote.getVotes ();
    std::uint32_t const feeUnits = target_.reference_fee_units;

	std::uint64_t const dropsPerByte = dropsPerByteVote.getVotes();

    auto const seq = lastClosedLedger->info().seq + 1;

    // Always add psudeo transaction for submit our current fee voting
    JLOG(journal_.warn()) <<
        "We are voting for a fee change: " << baseFee <<
        "/" << baseReserve <<
        "/" << incReserve    <<
		"/" << dropsPerByte;

    auto const shardID = set[0]->getFieldU32(sfShardID);

    STTx feeTx (ttFEE,
        [shardID,seq,baseFee,baseReserve,incReserve,feeUnits, dropsPerByte](auto& obj)
        {
            obj[sfShardID] = shardID;
            obj[sfAccount] = AccountID();
            obj[sfLedgerSequence] = seq;
            obj[sfBaseFee] = baseFee;
            obj[sfReserveBase] = baseReserve;
            obj[sfReserveIncrement] = incReserve;
            obj[sfReferenceFeeUnits] = feeUnits;
			obj[sfDropsPerByte] = dropsPerByte;
        });

    uint256 txID = feeTx.getTransactionID ();

    JLOG(journal_.warn()) <<
        "Vote: " << txID;

    Serializer s;
    feeTx.add (s);

    auto tItem = std::make_shared<SHAMapItem> (txID, s.peekData ());

    if (!initialPosition->addGiveItem (tItem, true, false))
    {
        JLOG(journal_.warn()) <<
            "Ledger already had fee change";
    }
}

//------------------------------------------------------------------------------

FeeVote::Setup
setup_FeeVote (Section const& section)
{
    FeeVote::Setup setup;
    set (setup.reference_fee, "reference_fee", section);
    set (setup.account_reserve, "account_reserve", section);
    set (setup.owner_reserve, "owner_reserve", section);
	set(setup.drops_per_byte, "drops_per_byte", section);

	// drops_per_byte range in [1,10^6]
	if (setup.drops_per_byte == 0 || setup.drops_per_byte > 1000000) {
		setup.drops_per_byte = (1000000 / 1024);
	}
		
    return setup;
}

std::unique_ptr<FeeVote>
make_FeeVote (FeeVote::Setup const& setup, beast::Journal journal)
{
    return std::make_unique<FeeVoteImpl> (setup, journal);
}

} // ripple