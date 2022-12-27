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

#ifndef RIPPLE_PROTOCOL_SYSTEMPARAMETERS_H_INCLUDED
#define RIPPLE_PROTOCOL_SYSTEMPARAMETERS_H_INCLUDED

#include <ripple/basics/ZXCAmount.h>
#include <ripple/basics/chrono.h>
#include <chrono>
#include <cstdint>
#include <string>

namespace ripple {

// Various protocol and system specific constant globals.

/* The name of the system. */
static inline std::string const&
systemName()
{
    static std::string const name = "chainsql";
    return name;
}

/** Configure the native currency. */

/** Number of drops in the genesis account. */
constexpr ZXCAmount INITIAL_ZXC{100'000'000'000 * DROPS_PER_ZXC};

/** Returns true if the amount does not exceed the initial ZXC in existence. */
inline bool
isLegalAmount(ZXCAmount const& amount)
{
    return amount <= INITIAL_ZXC;
}

/* The currency code for the native currency. */
static inline std::string const&
systemCurrencyCode()
{
    static std::string const code = "ZXC";
    return code;
}

/** The ZXC ledger network's earliest allowed sequence */
static std::uint32_t constexpr 
//ZXC_LEDGER_EARLIEST_SEQ {32570};
ZXC_LEDGER_EARLIEST_SEQ{ 1 };

/** The minimum amount of support an amendment should have.

    @note This value is used by legacy code and will become obsolete
          once the fixAmendmentMajorityCalc amendment activates.
*/
constexpr std::ratio<204, 256> preFixAmendmentMajorityCalcThreshold;

constexpr std::ratio<80, 100> postFixAmendmentMajorityCalcThreshold;

/** The minimum amount of time an amendment must hold a majority */
constexpr std::chrono::seconds const defaultAmendmentMajorityTime = std::chrono::hours{12};

}  // namespace ripple

/** Default peer port (IANA registered) */
inline std::uint16_t constexpr DEFAULT_PEER_PORT{2459};

#endif
