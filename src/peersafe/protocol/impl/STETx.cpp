//
//  STETx.cpp
//  chainsqld
//
//  Created by luleigreat on 2022/8/9.
//

#include <ripple/protocol/HashPrefix.h>
#include <ripple/protocol/Protocol.h>
#include <ripple/protocol/jss.h>
#include <ripple/protocol/PublicKey.h>
#include <peersafe/protocol/STETx.h>
#include <eth/vm/utils/keccak.h>
#include <peersafe/basics/TypeTransform.h>
#include <utility>
#include <ripple/protocol/STParsedJSON.h>
#include <ripple/basics/StringUtilities.h>
#include <eth/api/utils/TransactionSkeleton.h>



namespace ripple {

STETx::RlpDecoded::RlpDecoded(TransactionSkeleton const& ts): 
    nonce(ts.nonce)
    , gasPrice(ts.gasPrice)
    , gas(ts.gas)
    , receiveAddress(ts.to)
    , value(ts.value)
    , data(ts.data)
{
}

STETx::STETx(Slice const& sit, std::uint32_t lastLedgerSeq) noexcept(false)
    : m_rlpData(sit.begin(), sit.end())
{
    int length = sit.size();
    if ((length < txMinSizeBytes) || (length > txMaxSizeBytes))
        Throw<std::runtime_error>("Transaction length invalid");

    RlpDecoded decoded;
    //Decode RLP and set fields
    RLP const rlp(bytesConstRef(sit.data(),sit.size()));
    decoded.nonce = rlp[0].toInt<u256>();
    decoded.gasPrice = rlp[1].toInt<u256>();
    decoded.gas = rlp[2].toInt<u256>();

    if (!rlp[3].isData())
        Throw<std::runtime_error>("recipient RLP must be a byte array");
    decoded.receiveAddress =
        rlp[3].isEmpty() ? h160() : rlp[3].toHash<h160>(RLP::VeryStrict);

    decoded.value = rlp[4].toInt<u256>();

    if (!rlp[5].isData())
        Throw<std::runtime_error>("transaction data RLP must be a byte array");

    decoded.data = rlp[5].toBytes();

    decoded.v = rlp[6].toInt<u256>();
    decoded.r = rlp[7].toInt<u256>();
    decoded.s = rlp[8].toInt<u256>();

    m_type = rlp[3].isEmpty() ? ContractCreation : MessageCall;

    makeWithDecoded(decoded, lastLedgerSeq);
}


STETx::STETx(
    TransactionSkeleton const& ts,
    std::string secret,
    std::uint64_t chainID,
    std::uint32_t lastLedgerSeq) noexcept(false)
{
    
    RlpDecoded decoded(ts);
   
    int const vOffset = chainID * 2 + 35;
    int recovery = 1;
    //set default value for v
    decoded.v = u256(recovery + vOffset);

    m_type = ts.creation ? ContractCreation : MessageCall;
    //sign calculate rsv
    auto priData = *strUnHex(secret.substr(2));
    SecretKey sk(Slice(priData.data(),priData.size()));
    //used for sign
    auto digest = sha3(decoded, WithoutSignature);
    auto sig = signEthDigest(sk, digest);
    SignatureStruct sigStruct = *(SignatureStruct const*)&sig;
    if (sigStruct.isValid())
    {
        decoded.r = u256("0x" + to_string(sigStruct.r));
        decoded.s = u256("0x" + to_string(sigStruct.s));
        decoded.v = u256(recovery + vOffset);
    }
    
    RLPStream s;
    streamRLP(s, decoded, WithSignature, true);
    m_rlpData = std::move(s.out());
    
    makeWithDecoded(decoded, lastLedgerSeq);
}

void 
STETx::makeWithDecoded(RlpDecoded const& decoded, std::uint32_t lastLedgerSeq)
{
    setFName(sfTransaction);
    tx_type_ = ttETH_TX;

    // Check signature and get sender.
    auto sender = getSender(decoded);
    if (!sender.first)
        Throw<std::runtime_error>("check signature failed when get sender");

    uint64_t drops = uint64_t(decoded.value / u256(1e+12));
    if (decoded.value > 0 && drops == 0)
        Throw<std::runtime_error>("value too small to divide by 1e+12.");

    uint160 receive = fromH160(decoded.receiveAddress);
    AccountID receiveAddress;
    memcpy(receiveAddress.data(), receive.data(), receive.size());

    // Tx-Hash
    tid_ = sha3(decoded, WithSignature);
    //auto sTid = to_string(tid_);

    // Set fields
    set(getTxFormat(tx_type_)->getSOTemplate());
    setFieldU16(sfTransactionType, ttETH_TX);
    setAccountID(sfAccount, sender.second);
    setFieldU32(sfSequence, (uint32_t)decoded.nonce);
    setFieldU32(sfGas, (uint32_t)decoded.gas);
    setFieldU16(sfContractOpType, m_type);
    setFieldVL(sfSigningPubKey, Blob{});
    setFieldAmount(sfFee, ZXCAmount(10));
    if (!decoded.data.empty())
        setFieldVL(sfContractData, decoded.data);
    if (drops > 0)
        setFieldAmount(sfContractValue, ZXCAmount(drops));
    if (receiveAddress != beast::zero)
        setAccountID(sfContractAddress, receiveAddress);
    if (lastLedgerSeq > 0)
        setFieldU32(sfLastLedgerSequence, lastLedgerSeq);

    //auto str = getJson().toStyledString();

    pTxs_ = std::make_shared<std::vector<STTx>>();
    paJsonLog_ = std::make_shared<Json::Value>();
}

void STETx::streamRLP(
    RLPStream& _s,
    RlpDecoded const& _decoded,
    IncludeSignature _sig,
    bool _forEip155hash) const
{
    _s.appendList((_sig || _forEip155hash ? 3 : 0) + 6);
    _s << _decoded.nonce << _decoded.gasPrice << _decoded.gas;
    if (m_type == MessageCall)
        _s << _decoded.receiveAddress;
    else
        _s << "";
    _s << _decoded.value << _decoded.data;

    if (_sig)
    {
        _s << _decoded.v;
        _s << _decoded.r << _decoded.s;
    }
    else if (_forEip155hash)
    {
        auto chainId = ((uint64_t)_decoded.v - 35) / 2;
        _s << chainId << 0 << 0;
    }
}

uint256
STETx::sha3(RlpDecoded const& _decoded, IncludeSignature _sig)
{
    uint256 ret;
    if (_sig == WithSignature)
    {
        eth::sha3(m_rlpData.data(), m_rlpData.size(), ret.data());
    }
    else
    {
        RLPStream s;
        bool isReplayProtected = true;
        streamRLP(
            s, _decoded, _sig, isReplayProtected && _sig == WithoutSignature);

        eth::sha3(s.out().data(), s.out().size(), ret.data());
    }

    return ret;
}

std::pair<bool,AccountID>
STETx::getSender(RlpDecoded const& decoded)
{

    uint256 r = fromU256(decoded.r);
    uint256 s = fromU256(decoded.s);
    uint64_t v = uint64_t(decoded.v);

    AccountID from;
    boost::optional<uint64_t> chainId;
    if (v > 36)
    {
        chainId = (v - 35) / 2;
        if (chainId > std::numeric_limits<uint64_t>::max())
            Throw<std::runtime_error>("Invalid signature");
    }
    else if (v != 27 && v != 28)
        Throw<std::runtime_error>("Invalid signature");

    auto const recoveryID = chainId.has_value()
        ? byte(v - (uint64_t{*chainId} * 2 + 35))
        : byte(v - 27);

    ripple::SignatureStruct sig(r, s, recoveryID-1);
    if (sig.isValid())
    {
        try
        {
            Blob rec = ripple::recover(sig, sha3(decoded,WithoutSignature));
            if (rec.size() != 0)
            {
                uint256 shaData;
                eth::sha3(rec.data(), rec.size(), shaData.data());
                memcpy(from.data(), shaData.begin() + 12, 20);
                return std::make_pair(true, from);
            }
        }
        catch (...)
        {
        }
    }
    return std::make_pair(false, from);
}

std::pair<bool, std::string>
STETx::checkSign(RequireFullyCanonicalSig requireCanonicalSig) const
{
    std::pair<bool, std::string> ret{false, ""};
    try
    {
        if (isFieldPresent(sfAccount) && getAccountID(sfAccount) != beast::zero)
            return {true, ""};
    }
    catch (std::exception const&)
    {
        ret = {false, "Internal signature check failure."};
    }
    return ret;
}

Blob const&
STETx::getRlpData() const
{
    return m_rlpData;
}

std::string
STETx::getTxBinary() const
{
    return strHex(m_rlpData);
}

}  // namespace ripple
