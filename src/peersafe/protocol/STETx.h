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

#ifndef RIPPLE_PROTOCOL_STETX_H_INCLUDED
#define RIPPLE_PROTOCOL_STETX_H_INCLUDED

#include <ripple/protocol/STTx.h>
#include <eth/tools/RLP.h>
#include <eth/vm/Common.h>
#include <peersafe/protocol/ContractDefines.h>


using namespace eth;

namespace ripple {

class TransactionSkeleton;

/// Named-boolean type to encode whether a signature be included in the
/// serialization process.
enum IncludeSignature {
    WithoutSignature = 0,  ///< Do not include a signature.
    WithSignature = 1,     ///< Do include a signature.
};

extern const KnownFormats<TxType>::Item* getTxFormat(TxType type);

class STETx final : public STTx
{
public:
    static char const*
    getCountedObjectName()
    {
        return "STTx";
    }

public:
    STETx() = delete;
    STETx&
    operator=(STETx const& other) = delete;

    STETx(STETx const& other) = default;


    explicit STETx(Slice const& sit, std::uint32_t lastLedgerSeq = 0) noexcept(false);
    
    explicit STETx(TransactionSkeleton const& ts, std::string secret, std ::uint64_t chainID,
        std::uint32_t lastLedgerSeq = 0) noexcept(false);
    
    /** Check the signature.
        @return `true` if valid signature. If invalid, the error message string.
    */
    virtual std::pair<bool, std::string>
    checkSign(RequireFullyCanonicalSig requireCanonicalSig) const override;


    virtual void
    add(Serializer& s) const override
    {
        s.add8(0);
        s.addRaw(m_rlpData);
    }

    Blob const&
    getRlpData() const;

    virtual std::string
    getTxBinary() const override;

private:
    struct RlpDecoded
    {
        u256 nonce;
        u256 gasPrice;
        u256 gas;
        h160 receiveAddress;
        u256 value;
        eth::bytes data;
        u256 v;
        h256 r;
        h256 s;
        RlpDecoded()
            : nonce(Invalid256)
            , gasPrice(Invalid256)
            , gas(Invalid256)
            , receiveAddress(0)
            , value(0)
        {
        }
        RlpDecoded(TransactionSkeleton const& ts);
    };

    std::pair<bool, AccountID>
    getSender(RlpDecoded const& decoded);

    void
    makeWithDecoded(RlpDecoded const& decoded,std::uint32_t lastLedgerSeq);

    // Serializes this transaction to an RLPStream.
    /// @throws TransactionIsUnsigned if including signature was requested but
    /// it was not initialized
    void
    streamRLP(RLPStream& _s, 
        RlpDecoded const& _data,
        IncludeSignature _sig = WithSignature,
        bool _forEip155hash = false) const;

    uint256
    sha3(RlpDecoded const& _decoded, IncludeSignature _sig);

private:
    Blob m_rlpData;
    ContractOpType m_type;
};

}

#endif
