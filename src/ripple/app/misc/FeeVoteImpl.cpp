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

#include <ripple/app/ledger/Ledger.h>
#include <ripple/app/main/Application.h>
#include <ripple/app/misc/FeeVote.h>
#include <peersafe/schema/Schema.h>
#include <ripple/basics/BasicConfig.h>
#include <ripple/beast/utility/Journal.h>
#include <ripple/protocol/STValidation.h>
#include <ripple/protocol/st.h>
#include <ripple/core/ConfigSections.h>

namespace ripple {

namespace detail {

template <typename value_type>
class VotableValue
{
private:
    //using value_type = ZXCAmount;
    value_type const mCurrent;  // The current setting
    value_type const mTarget;   // The setting we want
    std::map<value_type, int> mVoteMap;

public:
    VotableValue(value_type current, value_type target)
        : mCurrent(current), mTarget(target)
    {
        // Add our vote
        ++mVoteMap[mTarget];
    }

    void
    addVote(value_type vote)
    {
        ++mVoteMap[vote];
    }

    void
    noVote()
    {
        addVote(mCurrent);
    }

    value_type
    getVotes() const;
};

template <typename value_type>
auto
VotableValue<value_type>::getVotes() const -> value_type
{
    value_type ourVote = mCurrent;
    int weight = 0;
    for (auto const& [key, val] : mVoteMap)
    {
        // Take most voted value between current and target, inclusive
        if (/* (key <= std::max(mTarget, mCurrent)) &&
            (key >= std::min(mTarget, mCurrent)) && */
            (val > weight))
        {
            ourVote = key;
            weight = val;
        }
    }

    return ourVote;
}

}  // namespace detail

//------------------------------------------------------------------------------

class FeeVoteImpl : public FeeVote
{
private:
    Setup target_;
    beast::Journal const journal_;

public:
    FeeVoteImpl(Setup const& setup, beast::Journal journal);

    void
    doValidation(Fees const& lastFees, STValidation& val) override;

    void
    doVoting(
        std::shared_ptr<ReadView const> const& lastClosedLedger,
        std::vector<std::shared_ptr<STValidation>> const& parentValidations,
        std::shared_ptr<SHAMap> const& initialPosition) override;
};

//--------------------------------------------------------------------------

FeeVoteImpl::FeeVoteImpl(Setup const& setup, beast::Journal journal)
    : target_(setup), journal_(journal)
{
}

void
FeeVoteImpl::doValidation(Fees const& lastFees, STValidation& v)
{
    // Values should always be in a valid range (because the voting process
    // will ignore out-of-range values) but if we detect such a case, we do
    // not send a value.
    if (lastFees.base != target_.reference_fee)
    {
        JLOG(journal_.info())
            << "Voting for base fee of " << target_.reference_fee;

        if (auto const f = target_.reference_fee.dropsAs<std::uint64_t>())
            v.setFieldU64(sfBaseFee, *f);
    }

    if (lastFees.accountReserve(0) != target_.account_reserve)
    {
        JLOG(journal_.info())
            << "Voting for base reserve of " << target_.account_reserve;

        if (auto const f = target_.account_reserve.dropsAs<std::uint32_t>())
            v.setFieldU32(sfReserveBase, *f);
    }

    if (lastFees.increment != target_.owner_reserve)
    {
        JLOG(journal_.info())
            << "Voting for reserve increment of " << target_.owner_reserve;

        if (auto const f = target_.owner_reserve.dropsAs<std::uint32_t>())
            v.setFieldU32(sfReserveIncrement, *f);
    }


	if (lastFees.drops_per_byte != target_.drops_per_byte)
	{
		JLOG(journal_.info()) <<
			"Voting for per zxc size " << target_.drops_per_byte;
		v.setFieldU64(sfDropsPerByte, target_.drops_per_byte);
	}

     if (lastFees.gas_price != target_.gas_price)
        {
            JLOG(journal_.info())
                << "Voting for base gas_price of " << target_.gas_price;
            v.setFieldU64(sfGasPrice, target_.gas_price);
        }
}

void
FeeVoteImpl::doVoting(
    std::shared_ptr<ReadView const> const& lastClosedLedger,
    std::vector<std::shared_ptr<STValidation>> const& set,
    std::shared_ptr<SHAMap> const& initialPosition)
{
    // LCL must be flag ledger
    assert(isFlagLedger(lastClosedLedger->seq()));

    detail::VotableValue baseFeeVote(
        lastClosedLedger->fees().base, target_.reference_fee);

    detail::VotableValue baseReserveVote(
        lastClosedLedger->fees().accountReserve(0), target_.account_reserve);

    detail::VotableValue incReserveVote(
        lastClosedLedger->fees().increment, target_.owner_reserve);


	detail::VotableValue<std::uint64_t> dropsPerByteVote(
		lastClosedLedger->fees().drops_per_byte, target_.drops_per_byte);

    detail::VotableValue<std::uint64_t> gasPriceVote(
            lastClosedLedger->fees().gas_price, target_.gas_price);

    for (auto const& val : set)
    {
        if (val->isTrusted())
        {
            if (val->isFieldPresent(sfBaseFee))
            {
                using zxctype = ZXCAmount::value_type;
                auto const vote = val->getFieldU64(sfBaseFee);
                if (vote <= std::numeric_limits<zxctype>::max() &&
                    isLegalAmount(ZXCAmount{unsafe_cast<zxctype>(vote)}))
                    baseFeeVote.addVote(
                        ZXCAmount{unsafe_cast<ZXCAmount::value_type>(vote)});
                else
                    // Invalid amounts will be treated as if they're
                    // not provided. Don't throw because this value is
                    // provided by an external entity.
                    baseFeeVote.noVote();
            }
            else
            {
                baseFeeVote.noVote();
            }

            if (val->isFieldPresent(sfReserveBase))
            {
                baseReserveVote.addVote(
                    ZXCAmount{val->getFieldU32(sfReserveBase)});
            }
            else
            {
                baseReserveVote.noVote();
            }

            if (val->isFieldPresent(sfReserveIncrement))
            {
                incReserveVote.addVote(
                    ZXCAmount{val->getFieldU32(sfReserveIncrement)});
            }
            else
            {
                incReserveVote.noVote();
            }


			if (val->isFieldPresent(sfDropsPerByte))
			{
				dropsPerByteVote.addVote(val->getFieldU64(sfDropsPerByte));
			}
			else
			{
				dropsPerByteVote.noVote();
			}

            if (val->isFieldPresent(sfGasPrice))
            {
                gasPriceVote.addVote(val->getFieldU64(sfGasPrice));
            }
            else
            {
                gasPriceVote.noVote();
            }

        }
    }

    // choose our positions
    // If any of the values are invalid, send the current values.
    auto const baseFee = baseFeeVote.getVotes().dropsAs<std::uint64_t>(
        lastClosedLedger->fees().base);
    auto const baseReserve = baseReserveVote.getVotes().dropsAs<std::uint32_t>(
        lastClosedLedger->fees().accountReserve(0));
    auto const incReserve = incReserveVote.getVotes().dropsAs<std::uint32_t>(
        lastClosedLedger->fees().increment);
    FeeUnit32 feeUnits = target_.reference_fee_units;
	std::uint64_t const dropsPerByte = dropsPerByteVote.getVotes();
    std::uint64_t const gasPrice = gasPriceVote.getVotes();
    auto const seq = lastClosedLedger->info().seq + 1;

    // add transactions to our position
    if ((baseFee != lastClosedLedger->fees().base) ||
            (baseReserve != lastClosedLedger->fees().accountReserve(0)) ||
            (incReserve != lastClosedLedger->fees().increment) ||
		   (dropsPerByte != lastClosedLedger->fees().drops_per_byte) ||
           (gasPrice != lastClosedLedger->fees().gas_price))
    {
        JLOG(journal_.warn()) <<
            "We are voting for a fee change: " << baseFee <<
            "/" << baseReserve <<
            "/" << incReserve    <<
			"/" << dropsPerByte;

        STTx feeTx (ttFEE,
            [seq,baseFee,baseReserve,incReserve,feeUnits, dropsPerByte, gasPrice](auto& obj)
            {
                obj[sfAccount] = AccountID();
                obj[sfLedgerSequence] = seq;
                obj[sfBaseFee] = baseFee;
                obj[sfReserveBase] = baseReserve;
                obj[sfReserveIncrement] = incReserve;
                obj[sfReferenceFeeUnits] = feeUnits.fee();
				obj[sfDropsPerByte] = dropsPerByte;
                obj[sfGasPrice] = gasPrice;
            });

        uint256 txID = feeTx.getTransactionID();

        JLOG(journal_.warn()) << "Vote: " << txID;

        Serializer s;
        feeTx.add(s);

        auto tItem = std::make_shared<SHAMapItem>(txID, s.peekData());

        if (!initialPosition->addGiveItem(std::move(tItem), true, false))
        {
            JLOG(journal_.warn()) << "Ledger already had fee change";
        }
    }
}

//------------------------------------------------------------------------------

FeeVote::Setup
setup_FeeVote(Config const& config)
{
    FeeVote::Setup setup;

    setup.reference_fee_units = config.TRANSACTION_FEE_BASE;
    setup.reference_fee = config.FEE_DEFAULT;
    setup.account_reserve = config.FEE_ACCOUNT_RESERVE;
    setup.owner_reserve = config.FEE_OWNER_RESERVE;
    setup.drops_per_byte = config.DROPS_PER_BYTE;
    setup.gas_price = config.GAS_PRICE;

    auto section = config.section(ConfigSection::voting());
    {
        std::uint64_t temp;
        if (set(temp, "reference_fee", section) &&
            temp <= std::numeric_limits<ZXCAmount::value_type>::max())
            setup.reference_fee = temp;
    }
    {
        std::uint32_t temp;
        if (set(temp, "account_reserve", section))
            setup.account_reserve = temp;
        if (set(temp, "owner_reserve", section))
            setup.owner_reserve = temp;
    }
    {
        std::uint32_t temp;
        if (set(temp, "drops_per_byte", section))
            setup.drops_per_byte = temp;

        // drops_per_byte range in [0,10^6]
        if (setup.drops_per_byte > 1000000)
        {
            setup.drops_per_byte = (1000000 / 1024);
        }
    }
    {
        std::uint64_t temp;
        if (set(temp, "gas_price", section))
            setup.gas_price = temp;
    }
    return setup;
}

std::unique_ptr<FeeVote>
make_FeeVote(FeeVote::Setup const& setup, beast::Journal journal)
{
    return std::make_unique<FeeVoteImpl>(setup, journal);
}

}  // namespace ripple
