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

#ifndef RIPPLE_BASICS_ZXCAMOUNT_H_INCLUDED
#define RIPPLE_BASICS_ZXCAMOUNT_H_INCLUDED

#include <ripple/basics/contract.h>
#include <ripple/basics/safe_cast.h>
#include <ripple/beast/cxx17/type_traits.h>
#include <ripple/beast/utility/Zero.h>
#include <ripple/json/json_value.h>

#include <boost/multiprecision/cpp_int.hpp>
#include <boost/operators.hpp>
#include <boost/optional.hpp>

#include <cstdint>
#include <string>

namespace ripple {

namespace feeunit {

/** "drops" are the smallest divisible amount of ZXC. This is what most
    of the code uses. */
struct dropTag;

}  // namespace feeunit

class ZXCAmount : private boost::totally_ordered<ZXCAmount>,
                  private boost::additive<ZXCAmount>,
                  private boost::equality_comparable<ZXCAmount, std::int64_t>,
                  private boost::additive<ZXCAmount, std::int64_t>
{
public:
    using unit_type = feeunit::dropTag;
    using value_type = std::int64_t;

private:
    value_type drops_;

public:
    ZXCAmount() = default;
    constexpr ZXCAmount(ZXCAmount const& other) = default;
    constexpr ZXCAmount&
    operator=(ZXCAmount const& other) = default;

    constexpr ZXCAmount(beast::Zero) : drops_(0)
    {
    }

    constexpr ZXCAmount& operator=(beast::Zero)
    {
        drops_ = 0;
        return *this;
    }

    constexpr explicit ZXCAmount(value_type drops) : drops_(drops)
    {
    }

    ZXCAmount&
    operator=(value_type drops)
    {
        drops_ = drops;
        return *this;
    }

    constexpr ZXCAmount
    operator*(value_type const& rhs) const
    {
        return ZXCAmount{drops_ * rhs};
    }

    friend constexpr ZXCAmount
    operator*(value_type lhs, ZXCAmount const& rhs)
    {
        // multiplication is commutative
        return rhs * lhs;
    }

    ZXCAmount&
    operator+=(ZXCAmount const& other)
    {
        drops_ += other.drops();
        return *this;
    }

    ZXCAmount&
    operator-=(ZXCAmount const& other)
    {
        drops_ -= other.drops();
        return *this;
    }

    ZXCAmount&
    operator+=(value_type const& rhs)
    {
        drops_ += rhs;
        return *this;
    }

    ZXCAmount&
    operator-=(value_type const& rhs)
    {
        drops_ -= rhs;
        return *this;
    }

    ZXCAmount&
    operator*=(value_type const& rhs)
    {
        drops_ *= rhs;
        return *this;
    }

    ZXCAmount
    operator-() const
    {
        return ZXCAmount{-drops_};
    }

    bool
    operator==(ZXCAmount const& other) const
    {
        return drops_ == other.drops_;
    }

    bool
    operator==(value_type other) const
    {
        return drops_ == other;
    }

    bool
    operator<(ZXCAmount const& other) const
    {
        return drops_ < other.drops_;
    }

    /** Returns true if the amount is not zero */
    explicit constexpr operator bool() const noexcept
    {
        return drops_ != 0;
    }

    /** Return the sign of the amount */
    constexpr int
    signum() const noexcept
    {
        return (drops_ < 0) ? -1 : (drops_ ? 1 : 0);
    }

    /** Returns the number of drops */
    constexpr value_type
    drops() const
    {
        return drops_;
    }

    constexpr double
    decimalZXC() const;

    template <class Dest>
    boost::optional<Dest>
    dropsAs() const
    {
        if ((drops_ > std::numeric_limits<Dest>::max()) ||
            (!std::numeric_limits<Dest>::is_signed && drops_ < 0) ||
            (std::numeric_limits<Dest>::is_signed &&
             drops_ < std::numeric_limits<Dest>::lowest()))
        {
            return boost::none;
        }
        return static_cast<Dest>(drops_);
    }

    template <class Dest>
    Dest
    dropsAs(Dest defaultValue) const
    {
        return dropsAs<Dest>().value_or(defaultValue);
    }

    template <class Dest>
    Dest
    dropsAs(ZXCAmount defaultValue) const
    {
        return dropsAs<Dest>().value_or(defaultValue.drops());
    }

    Json::Value
    jsonClipped() const
    {
        static_assert(
            std::is_signed_v<value_type> && std::is_integral_v<value_type>,
            "Expected ZXCAmount to be a signed integral type");

        constexpr auto min = std::numeric_limits<Json::Int>::min();
        constexpr auto max = std::numeric_limits<Json::Int>::max();

        if (drops_ < min)
            return min;
        if (drops_ > max)
            return max;
        return static_cast<Json::Int>(drops_);
    }

    /** Returns the underlying value. Code SHOULD NOT call this
        function unless the type has been abstracted away,
        e.g. in a templated function.
    */
    constexpr value_type
    value() const
    {
        return drops_;
    }

    friend std::istream&
    operator>>(std::istream& s, ZXCAmount& val)
    {
        s >> val.drops_;
        return s;
    }
};

/** Number of drops per 1 ZXC */
constexpr ZXCAmount DROPS_PER_ZXC{1'000'000};

constexpr double
ZXCAmount::decimalZXC() const
{
    return static_cast<double>(drops_) / DROPS_PER_ZXC.drops();
}

// Output ZXCAmount as just the drops value.
template <class Char, class Traits>
std::basic_ostream<Char, Traits>&
operator<<(std::basic_ostream<Char, Traits>& os, const ZXCAmount& q)
{
    return os << q.drops();
}

inline std::string
to_string(ZXCAmount const& amount)
{
    return std::to_string(amount.drops());
}

inline ZXCAmount
mulRatio(
    ZXCAmount const& amt,
    std::uint32_t num,
    std::uint32_t den,
    bool roundUp)
{
    using namespace boost::multiprecision;

    if (!den)
        Throw<std::runtime_error>("division by zero");

    int128_t const amt128(amt.drops());
    auto const neg = amt.drops() < 0;
    auto const m = amt128 * num;
    auto r = m / den;
    if (m % den)
    {
        if (!neg && roundUp)
            r += 1;
        if (neg && !roundUp)
            r -= 1;
    }
    if (r > std::numeric_limits<ZXCAmount::value_type>::max())
        Throw<std::overflow_error>("ZXC mulRatio overflow");
    return ZXCAmount(r.convert_to<ZXCAmount::value_type>());
}

}  // namespace ripple

#endif  // RIPPLE_BASICS_ZXCAMOUNT_H_INCLUDED
