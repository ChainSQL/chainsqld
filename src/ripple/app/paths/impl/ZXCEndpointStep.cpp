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

<<<<<<< HEAD:src/ripple/app/paths/impl/ZXCEndpointStep.cpp

=======
>>>>>>> release:src/ripple/app/paths/impl/XRPEndpointStep.cpp
#include <ripple/app/paths/Credit.h>
#include <ripple/app/paths/impl/AmountSpec.h>
#include <ripple/app/paths/impl/StepChecks.h>
#include <ripple/app/paths/impl/Steps.h>
#include <ripple/basics/IOUAmount.h>
#include <ripple/basics/Log.h>
#include <ripple/basics/XRPAmount.h>
#include <ripple/ledger/PaymentSandbox.h>
#include <ripple/protocol/Feature.h>
#include <ripple/protocol/Quality.h>
<<<<<<< HEAD:src/ripple/app/paths/impl/ZXCEndpointStep.cpp
#include <ripple/protocol/ZXCAmount.h>
=======
>>>>>>> release:src/ripple/app/paths/impl/XRPEndpointStep.cpp

#include <boost/container/flat_set.hpp>

#include <numeric>
#include <sstream>

namespace ripple {

template <class TDerived>
<<<<<<< HEAD:src/ripple/app/paths/impl/ZXCEndpointStep.cpp
class ZXCEndpointStep : public StepImp<
    ZXCAmount, ZXCAmount, ZXCEndpointStep<TDerived>>
=======
class XRPEndpointStep
    : public StepImp<XRPAmount, XRPAmount, XRPEndpointStep<TDerived>>
>>>>>>> release:src/ripple/app/paths/impl/XRPEndpointStep.cpp
{
private:
    AccountID acc_;
    bool const isLast_;
    beast::Journal const j_;

    // Since this step will always be an endpoint in a strand
    // (either the first or last step) the same cache is used
    // for cachedIn and cachedOut and only one will ever be used
    boost::optional<ZXCAmount> cache_;

    boost::optional<EitherAmount>
    cached() const
    {
        if (!cache_)
            return boost::none;
        return EitherAmount(*cache_);
    }

public:
<<<<<<< HEAD:src/ripple/app/paths/impl/ZXCEndpointStep.cpp
    ZXCEndpointStep (
        StrandContext const& ctx,
        AccountID const& acc)
            : acc_(acc)
            , isLast_(ctx.isLast)
            , j_ (ctx.j) {}

    AccountID const& acc () const
=======
    XRPEndpointStep(StrandContext const& ctx, AccountID const& acc)
        : acc_(acc), isLast_(ctx.isLast), j_(ctx.j)
    {
    }

    AccountID const&
    acc() const
>>>>>>> release:src/ripple/app/paths/impl/XRPEndpointStep.cpp
    {
        return acc_;
    }

    boost::optional<std::pair<AccountID, AccountID>>
    directStepAccts() const override
    {
        if (isLast_)
            return std::make_pair(zxcAccount(), acc_);
        return std::make_pair(acc_, zxcAccount());
    }

    boost::optional<EitherAmount>
    cachedIn() const override
    {
        return cached();
    }

    boost::optional<EitherAmount>
    cachedOut() const override
    {
        return cached();
    }

    DebtDirection
    debtDirection(ReadView const& sb, StrandDirection dir) const override
    {
        return DebtDirection::issues;
    }

    std::pair<boost::optional<Quality>, DebtDirection>
    qualityUpperBound(ReadView const& v, DebtDirection prevStepDir)
        const override;

<<<<<<< HEAD:src/ripple/app/paths/impl/ZXCEndpointStep.cpp
    std::pair<ZXCAmount, ZXCAmount>
    revImp (
=======
    std::pair<XRPAmount, XRPAmount>
    revImp(
>>>>>>> release:src/ripple/app/paths/impl/XRPEndpointStep.cpp
        PaymentSandbox& sb,
        ApplyView& afView,
        boost::container::flat_set<uint256>& ofrsToRm,
        ZXCAmount const& out);

<<<<<<< HEAD:src/ripple/app/paths/impl/ZXCEndpointStep.cpp
    std::pair<ZXCAmount, ZXCAmount>
    fwdImp (
=======
    std::pair<XRPAmount, XRPAmount>
    fwdImp(
>>>>>>> release:src/ripple/app/paths/impl/XRPEndpointStep.cpp
        PaymentSandbox& sb,
        ApplyView& afView,
        boost::container::flat_set<uint256>& ofrsToRm,
        ZXCAmount const& in);

    std::pair<bool, EitherAmount>
    validFwd(PaymentSandbox& sb, ApplyView& afView, EitherAmount const& in)
        override;

    // Check for errors and violations of frozen constraints.
    TER
    check(StrandContext const& ctx) const;

protected:
<<<<<<< HEAD:src/ripple/app/paths/impl/ZXCEndpointStep.cpp
    ZXCAmount
    zxcLiquidImpl (ReadView& sb, std::int32_t reserveReduction) const
    {
        return ripple::zxcLiquid (sb, acc_, reserveReduction, j_);
=======
    XRPAmount
    xrpLiquidImpl(ReadView& sb, std::int32_t reserveReduction) const
    {
        return ripple::xrpLiquid(sb, acc_, reserveReduction, j_);
>>>>>>> release:src/ripple/app/paths/impl/XRPEndpointStep.cpp
    }

    std::string
    logStringImpl(char const* name) const
    {
        std::ostringstream ostr;
        ostr << name << ": "
             << "\nAcc: " << acc_;
        return ostr.str();
    }

private:
    template <class P>
<<<<<<< HEAD:src/ripple/app/paths/impl/ZXCEndpointStep.cpp
    friend bool operator==(
        ZXCEndpointStep<P> const& lhs,
        ZXCEndpointStep<P> const& rhs);

    friend bool operator!=(
        ZXCEndpointStep const& lhs,
        ZXCEndpointStep const& rhs)
=======
    friend bool
    operator==(XRPEndpointStep<P> const& lhs, XRPEndpointStep<P> const& rhs);

    friend bool
    operator!=(XRPEndpointStep const& lhs, XRPEndpointStep const& rhs)
>>>>>>> release:src/ripple/app/paths/impl/XRPEndpointStep.cpp
    {
        return !(lhs == rhs);
    }

    bool
    equal(Step const& rhs) const override
    {
<<<<<<< HEAD:src/ripple/app/paths/impl/ZXCEndpointStep.cpp
        if (auto ds = dynamic_cast<ZXCEndpointStep const*> (&rhs))
=======
        if (auto ds = dynamic_cast<XRPEndpointStep const*>(&rhs))
>>>>>>> release:src/ripple/app/paths/impl/XRPEndpointStep.cpp
        {
            return *this == *ds;
        }
        return false;
    }
};

//------------------------------------------------------------------------------

// Flow is used in two different circumstances for transferring funds:
//  o Payments, and
//  o Offer crossing.
// The rules for handling funds in these two cases are almost, but not
// quite, the same.

// Payment ZXCEndpointStep class (not offer crossing).
class ZXCEndpointPaymentStep : public ZXCEndpointStep<ZXCEndpointPaymentStep>
{
public:
    using ZXCEndpointStep<ZXCEndpointPaymentStep>::ZXCEndpointStep;

<<<<<<< HEAD:src/ripple/app/paths/impl/ZXCEndpointStep.cpp
    ZXCAmount
    zxcLiquid (ReadView& sb) const
    {
        return zxcLiquidImpl (sb, 0);;
=======
    XRPAmount
    xrpLiquid(ReadView& sb) const
    {
        return xrpLiquidImpl(sb, 0);
        ;
>>>>>>> release:src/ripple/app/paths/impl/XRPEndpointStep.cpp
    }

    std::string
    logString() const override
    {
<<<<<<< HEAD:src/ripple/app/paths/impl/ZXCEndpointStep.cpp
        return logStringImpl ("ZXCEndpointPaymentStep");
    }
};

// Offer crossing ZXCEndpointStep class (not a payment).
class ZXCEndpointOfferCrossingStep :
    public ZXCEndpointStep<ZXCEndpointOfferCrossingStep>
=======
        return logStringImpl("XRPEndpointPaymentStep");
    }
};

// Offer crossing XRPEndpointStep class (not a payment).
class XRPEndpointOfferCrossingStep
    : public XRPEndpointStep<XRPEndpointOfferCrossingStep>
>>>>>>> release:src/ripple/app/paths/impl/XRPEndpointStep.cpp
{
private:
    // For historical reasons, offer crossing is allowed to dig further
    // into the ZXC reserve than an ordinary payment.  (I believe it's
    // because the trust line was created after the ZXC was removed.)
    // Return how much the reserve should be reduced.
    //
    // Note that reduced reserve only happens if the trust line does not
    // currently exist.
    static std::int32_t
    computeReserveReduction(StrandContext const& ctx, AccountID const& acc)
    {
        if (ctx.isFirst && !ctx.view.read(keylet::line(acc, ctx.strandDeliver)))
            return -1;
        return 0;
    }

public:
<<<<<<< HEAD:src/ripple/app/paths/impl/ZXCEndpointStep.cpp
    ZXCEndpointOfferCrossingStep (
        StrandContext const& ctx, AccountID const& acc)
    : ZXCEndpointStep<ZXCEndpointOfferCrossingStep> (ctx, acc)
    , reserveReduction_ (computeReserveReduction (ctx, acc))
    {
    }

    ZXCAmount
    zxcLiquid (ReadView& sb) const
    {
        return zxcLiquidImpl (sb, reserveReduction_);
=======
    XRPEndpointOfferCrossingStep(StrandContext const& ctx, AccountID const& acc)
        : XRPEndpointStep<XRPEndpointOfferCrossingStep>(ctx, acc)
        , reserveReduction_(computeReserveReduction(ctx, acc))
    {
    }

    XRPAmount
    xrpLiquid(ReadView& sb) const
    {
        return xrpLiquidImpl(sb, reserveReduction_);
>>>>>>> release:src/ripple/app/paths/impl/XRPEndpointStep.cpp
    }

    std::string
    logString() const override
    {
<<<<<<< HEAD:src/ripple/app/paths/impl/ZXCEndpointStep.cpp
        return logStringImpl ("ZXCEndpointOfferCrossingStep");
=======
        return logStringImpl("XRPEndpointOfferCrossingStep");
>>>>>>> release:src/ripple/app/paths/impl/XRPEndpointStep.cpp
    }

private:
    std::int32_t const reserveReduction_;
};

//------------------------------------------------------------------------------

template <class TDerived>
<<<<<<< HEAD:src/ripple/app/paths/impl/ZXCEndpointStep.cpp
inline bool operator==(ZXCEndpointStep<TDerived> const& lhs,
    ZXCEndpointStep<TDerived> const& rhs)
=======
inline bool
operator==(
    XRPEndpointStep<TDerived> const& lhs,
    XRPEndpointStep<TDerived> const& rhs)
>>>>>>> release:src/ripple/app/paths/impl/XRPEndpointStep.cpp
{
    return lhs.acc_ == rhs.acc_ && lhs.isLast_ == rhs.isLast_;
}

template <class TDerived>
<<<<<<< HEAD:src/ripple/app/paths/impl/ZXCEndpointStep.cpp
boost::optional<Quality>
ZXCEndpointStep<TDerived>::qualityUpperBound(
    ReadView const& v, bool& redeems) const
=======
std::pair<boost::optional<Quality>, DebtDirection>
XRPEndpointStep<TDerived>::qualityUpperBound(
    ReadView const& v,
    DebtDirection prevStepDir) const
>>>>>>> release:src/ripple/app/paths/impl/XRPEndpointStep.cpp
{
    return {
        Quality{STAmount::uRateOne},
        this->debtDirection(v, StrandDirection::forward)};
}

template <class TDerived>
<<<<<<< HEAD:src/ripple/app/paths/impl/ZXCEndpointStep.cpp
std::pair<ZXCAmount, ZXCAmount>
ZXCEndpointStep<TDerived>::revImp (
=======
std::pair<XRPAmount, XRPAmount>
XRPEndpointStep<TDerived>::revImp(
>>>>>>> release:src/ripple/app/paths/impl/XRPEndpointStep.cpp
    PaymentSandbox& sb,
    ApplyView& afView,
    boost::container::flat_set<uint256>& ofrsToRm,
    ZXCAmount const& out)
{
<<<<<<< HEAD:src/ripple/app/paths/impl/ZXCEndpointStep.cpp
    auto const balance = static_cast<TDerived const*>(this)->zxcLiquid (sb);
=======
    auto const balance = static_cast<TDerived const*>(this)->xrpLiquid(sb);
>>>>>>> release:src/ripple/app/paths/impl/XRPEndpointStep.cpp

    auto const result = isLast_ ? out : std::min(balance, out);

<<<<<<< HEAD:src/ripple/app/paths/impl/ZXCEndpointStep.cpp
    auto& sender = isLast_ ? zxcAccount() : acc_;
    auto& receiver = isLast_ ? acc_ : zxcAccount();
    auto ter   = accountSend (sb, sender, receiver, toSTAmount (result), j_);
=======
    auto& sender = isLast_ ? xrpAccount() : acc_;
    auto& receiver = isLast_ ? acc_ : xrpAccount();
    auto ter = accountSend(sb, sender, receiver, toSTAmount(result), j_);
>>>>>>> release:src/ripple/app/paths/impl/XRPEndpointStep.cpp
    if (ter != tesSUCCESS)
        return {ZXCAmount{beast::zero}, ZXCAmount{beast::zero}};

    cache_.emplace(result);
    return {result, result};
}

template <class TDerived>
<<<<<<< HEAD:src/ripple/app/paths/impl/ZXCEndpointStep.cpp
std::pair<ZXCAmount, ZXCAmount>
ZXCEndpointStep<TDerived>::fwdImp (
=======
std::pair<XRPAmount, XRPAmount>
XRPEndpointStep<TDerived>::fwdImp(
>>>>>>> release:src/ripple/app/paths/impl/XRPEndpointStep.cpp
    PaymentSandbox& sb,
    ApplyView& afView,
    boost::container::flat_set<uint256>& ofrsToRm,
    ZXCAmount const& in)
{
<<<<<<< HEAD:src/ripple/app/paths/impl/ZXCEndpointStep.cpp
    assert (cache_);
    auto const balance = static_cast<TDerived const*>(this)->zxcLiquid (sb);
=======
    assert(cache_);
    auto const balance = static_cast<TDerived const*>(this)->xrpLiquid(sb);
>>>>>>> release:src/ripple/app/paths/impl/XRPEndpointStep.cpp

    auto const result = isLast_ ? in : std::min(balance, in);

<<<<<<< HEAD:src/ripple/app/paths/impl/ZXCEndpointStep.cpp
    auto& sender = isLast_ ? zxcAccount() : acc_;
    auto& receiver = isLast_ ? acc_ : zxcAccount();
    auto ter   = accountSend (sb, sender, receiver, toSTAmount (result), j_);
=======
    auto& sender = isLast_ ? xrpAccount() : acc_;
    auto& receiver = isLast_ ? acc_ : xrpAccount();
    auto ter = accountSend(sb, sender, receiver, toSTAmount(result), j_);
>>>>>>> release:src/ripple/app/paths/impl/XRPEndpointStep.cpp
    if (ter != tesSUCCESS)
        return {ZXCAmount{beast::zero}, ZXCAmount{beast::zero}};

    cache_.emplace(result);
    return {result, result};
}

template <class TDerived>
std::pair<bool, EitherAmount>
<<<<<<< HEAD:src/ripple/app/paths/impl/ZXCEndpointStep.cpp
ZXCEndpointStep<TDerived>::validFwd (
=======
XRPEndpointStep<TDerived>::validFwd(
>>>>>>> release:src/ripple/app/paths/impl/XRPEndpointStep.cpp
    PaymentSandbox& sb,
    ApplyView& afView,
    EitherAmount const& in)
{
    if (!cache_)
    {
<<<<<<< HEAD:src/ripple/app/paths/impl/ZXCEndpointStep.cpp
        JLOG (j_.error()) << "Expected valid cache in validFwd";
        return {false, EitherAmount (ZXCAmount (beast::zero))};
=======
        JLOG(j_.error()) << "Expected valid cache in validFwd";
        return {false, EitherAmount(XRPAmount(beast::zero))};
>>>>>>> release:src/ripple/app/paths/impl/XRPEndpointStep.cpp
    }

    assert(in.native);

<<<<<<< HEAD:src/ripple/app/paths/impl/ZXCEndpointStep.cpp
    auto const& zxcIn = in.zxc;
    auto const balance = static_cast<TDerived const*>(this)->zxcLiquid (sb);
=======
    auto const& xrpIn = in.xrp;
    auto const balance = static_cast<TDerived const*>(this)->xrpLiquid(sb);
>>>>>>> release:src/ripple/app/paths/impl/XRPEndpointStep.cpp

    if (!isLast_ && balance < zxcIn)
    {
<<<<<<< HEAD:src/ripple/app/paths/impl/ZXCEndpointStep.cpp
        JLOG (j_.error()) << "ZXCEndpointStep: Strand re-execute check failed."
            << " Insufficient balance: " << to_string (balance)
            << " Requested: " << to_string (zxcIn);
        return {false, EitherAmount (balance)};
=======
        JLOG(j_.warn()) << "XRPEndpointStep: Strand re-execute check failed."
                        << " Insufficient balance: " << to_string(balance)
                        << " Requested: " << to_string(xrpIn);
        return {false, EitherAmount(balance)};
>>>>>>> release:src/ripple/app/paths/impl/XRPEndpointStep.cpp
    }

    if (zxcIn != *cache_)
    {
<<<<<<< HEAD:src/ripple/app/paths/impl/ZXCEndpointStep.cpp
        JLOG (j_.error()) << "ZXCEndpointStep: Strand re-execute check failed."
            << " ExpectedIn: " << to_string (*cache_)
            << " CachedIn: " << to_string (zxcIn);
=======
        JLOG(j_.warn()) << "XRPEndpointStep: Strand re-execute check failed."
                        << " ExpectedIn: " << to_string(*cache_)
                        << " CachedIn: " << to_string(xrpIn);
>>>>>>> release:src/ripple/app/paths/impl/XRPEndpointStep.cpp
    }
    return {true, in};
}

template <class TDerived>
TER
<<<<<<< HEAD:src/ripple/app/paths/impl/ZXCEndpointStep.cpp
ZXCEndpointStep<TDerived>::check (StrandContext const& ctx) const
{
    if (!acc_)
    {
        JLOG (j_.debug()) << "ZXCEndpointStep: specified bad account.";
=======
XRPEndpointStep<TDerived>::check(StrandContext const& ctx) const
{
    if (!acc_)
    {
        JLOG(j_.debug()) << "XRPEndpointStep: specified bad account.";
>>>>>>> release:src/ripple/app/paths/impl/XRPEndpointStep.cpp
        return temBAD_PATH;
    }

    auto sleAcc = ctx.view.read(keylet::account(acc_));
    if (!sleAcc)
    {
<<<<<<< HEAD:src/ripple/app/paths/impl/ZXCEndpointStep.cpp
        JLOG (j_.warn()) << "ZXCEndpointStep: can't send or receive ZXC from "
                             "non-existent account: "
                          << acc_;
=======
        JLOG(j_.warn()) << "XRPEndpointStep: can't send or receive XRP from "
                           "non-existent account: "
                        << acc_;
>>>>>>> release:src/ripple/app/paths/impl/XRPEndpointStep.cpp
        return terNO_ACCOUNT;
    }

    if (!ctx.isFirst && !ctx.isLast)
    {
        return temBAD_PATH;
    }

<<<<<<< HEAD:src/ripple/app/paths/impl/ZXCEndpointStep.cpp
    auto& src = isLast_ ? zxcAccount () : acc_;
    auto& dst = isLast_ ? acc_ : zxcAccount();
    auto ter = checkFreeze (ctx.view, src, dst, zxcCurrency ());
=======
    auto& src = isLast_ ? xrpAccount() : acc_;
    auto& dst = isLast_ ? acc_ : xrpAccount();
    auto ter = checkFreeze(ctx.view, src, dst, xrpCurrency());
>>>>>>> release:src/ripple/app/paths/impl/XRPEndpointStep.cpp
    if (ter != tesSUCCESS)
        return ter;

    if (ctx.view.rules().enabled(fix1781))
    {
        auto const issuesIndex = isLast_ ? 0 : 1;
        if (!ctx.seenDirectIssues[issuesIndex].insert(xrpIssue()).second)
        {
            JLOG(j_.debug())
                << "XRPEndpointStep: loop detected: Index: " << ctx.strandSize
                << ' ' << *this;
            return temBAD_PATH_LOOP;
        }
    }

    return tesSUCCESS;
}

//------------------------------------------------------------------------------

namespace test {
// Needed for testing
<<<<<<< HEAD:src/ripple/app/paths/impl/ZXCEndpointStep.cpp
bool zxcEndpointStepEqual (Step const& step, AccountID const& acc)
{
    if (auto xs =
        dynamic_cast<ZXCEndpointStep<ZXCEndpointPaymentStep> const*> (&step))
=======
bool
xrpEndpointStepEqual(Step const& step, AccountID const& acc)
{
    if (auto xs =
            dynamic_cast<XRPEndpointStep<XRPEndpointPaymentStep> const*>(&step))
>>>>>>> release:src/ripple/app/paths/impl/XRPEndpointStep.cpp
    {
        return xs->acc() == acc;
    }
    return false;
}
}  // namespace test

//------------------------------------------------------------------------------

std::pair<TER, std::unique_ptr<Step>>
<<<<<<< HEAD:src/ripple/app/paths/impl/ZXCEndpointStep.cpp
make_ZXCEndpointStep (
    StrandContext const& ctx,
    AccountID const& acc)
=======
make_XRPEndpointStep(StrandContext const& ctx, AccountID const& acc)
>>>>>>> release:src/ripple/app/paths/impl/XRPEndpointStep.cpp
{
    TER ter = tefINTERNAL;
    std::unique_ptr<Step> r;
    if (ctx.offerCrossing)
    {
        auto offerCrossingStep =
<<<<<<< HEAD:src/ripple/app/paths/impl/ZXCEndpointStep.cpp
            std::make_unique<ZXCEndpointOfferCrossingStep> (ctx, acc);
        ter = offerCrossingStep->check (ctx);
        r = std::move (offerCrossingStep);
=======
            std::make_unique<XRPEndpointOfferCrossingStep>(ctx, acc);
        ter = offerCrossingStep->check(ctx);
        r = std::move(offerCrossingStep);
>>>>>>> release:src/ripple/app/paths/impl/XRPEndpointStep.cpp
    }
    else  // payment
    {
<<<<<<< HEAD:src/ripple/app/paths/impl/ZXCEndpointStep.cpp
        auto paymentStep =
            std::make_unique<ZXCEndpointPaymentStep> (ctx, acc);
        ter = paymentStep->check (ctx);
        r = std::move (paymentStep);
=======
        auto paymentStep = std::make_unique<XRPEndpointPaymentStep>(ctx, acc);
        ter = paymentStep->check(ctx);
        r = std::move(paymentStep);
>>>>>>> release:src/ripple/app/paths/impl/XRPEndpointStep.cpp
    }
    if (ter != tesSUCCESS)
        return {ter, nullptr};

    return {tesSUCCESS, std::move(r)};
}

}  // namespace ripple
