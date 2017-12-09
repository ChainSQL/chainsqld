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
#include <ripple/protocol/STTx.h>
#include <ripple/protocol/HashPrefix.h>
#include <ripple/protocol/JsonFields.h>
#include <ripple/protocol/PublicKey.h>
#include <ripple/protocol/Protocol.h>
#include <ripple/protocol/Sign.h>
#include <ripple/protocol/STAccount.h>
#include <ripple/protocol/STArray.h>
#include <ripple/protocol/TxFlags.h>
#include <ripple/protocol/types.h>
#include <ripple/protocol/STParsedJSON.h>
#include <ripple/basics/contract.h>
#include <ripple/basics/Log.h>
#include <ripple/basics/StringUtilities.h>
#include <ripple/json/to_string.h>
#include <boost/format.hpp>
#include <array>
#include <memory>
#include <type_traits>
#include <utility>
#include <ripple/json/json_reader.h>
#include <peersafe/protocol/TableDefines.h>

namespace ripple {

static
auto getTxFormat (TxType type)
{
    auto format = TxFormats::getInstance().findByType (type);

    if (format == nullptr)
    {
        Throw<std::runtime_error> (
            "Invalid transaction type " +
            std::to_string (
                static_cast<std::underlying_type_t<TxType>>(type)));
    }

    return format;
}

STTx::STTx (STObject&& object)
    : STObject (std::move (object))
{
    tx_type_ = static_cast <TxType> (getFieldU16 (sfTransactionType));

    if (!setType (getTxFormat (tx_type_)->elements))
        Throw<std::runtime_error> ("transaction not valid");

    tid_ = getHash(HashPrefix::transactionID);
}

std::pair<std::shared_ptr<STTx>, std::string> STTx::parseSTTx(Json::Value& obj, AccountID accountID)
{
	std::string err_message;
	int transactionType = 0;

	int type = obj["OpType"].asInt();

	if (isTableListSetOpType((TableOpType)type)) {
		transactionType = ttTABLELISTSET;
	}
	else if (isSqlStatementOpType((TableOpType)type)) {
		transactionType = ttSQLSTATEMENT;
	}
	else
	{
		transactionType = ttSQLSTATEMENT;
	}

	obj[jss::TransactionType] = transactionType;
	obj[jss::Account] = to_string(accountID);
	obj[jss::Fee] = 0;
	obj[jss::Sequence] = 0;

	std::shared_ptr<STTx> stpTrans;
	STParsedJSONObject parsed(std::string(jss::tx_json), obj);
	if (parsed.object == boost::none)
	{
		err_message = parsed.error[jss::error_message].asString();
		return std::make_pair(std::move(stpTrans), err_message);
	}

	try
	{
		// If we're generating a multi-signature the SigningPubKey must be
		// empty, otherwise it must be the master account's public key.
		parsed.object->setFieldVL(sfSigningPubKey, Slice(nullptr, 0));

		stpTrans = std::make_shared<STTx>(
			std::move(parsed.object.get()));
	}
	catch (std::exception&)
	{
		err_message = "transaction not valid,please check";
	}
	return std::make_pair(std::move(stpTrans), err_message);
}

STTx::STTx (SerialIter& sit)
    : STObject (sfTransaction)
{
    int length = sit.getBytesLeft ();

    if ((length < Protocol::txMinSizeBytes) || (length > Protocol::txMaxSizeBytes))
        Throw<std::runtime_error> ("Transaction length invalid");

    set (sit);
    tx_type_ = static_cast<TxType> (getFieldU16 (sfTransactionType));

    if (!setType (getTxFormat (tx_type_)->elements))
        Throw<std::runtime_error> ("transaction not valid"); 

    tid_ = getHash(HashPrefix::transactionID);
}

STTx::STTx (
        TxType type,
        std::function<void(STObject&)> assembler)
    : STObject (sfTransaction)
{
    auto format = getTxFormat (type);

    set (format->elements);
    setFieldU16 (sfTransactionType, format->getType ());

    assembler (*this);

    tx_type_ = static_cast<TxType>(getFieldU16 (sfTransactionType));

    if (tx_type_ != type)
        LogicError ("Transaction type was mutated during assembly");

    tid_ = getHash(HashPrefix::transactionID);
}

bool STTx::isCrossChainUpload() const
{
	if (isFieldPresent(sfOriginalAddress) ||
		isFieldPresent(sfTxnLgrSeq) ||
		isFieldPresent(sfCurTxHash) ||
		isFieldPresent(sfFutureTxHash)
		)
		return true;
	return false;
}

std::string STTx::buildRaw(std::string sOperationRule) const
{
	std::string sRaw;
	if (!isFieldPresent(sfRaw))
	{
		return sRaw;
	}
	ripple::Blob raw;
	raw = getFieldVL(sfRaw);
	sRaw = std::string(raw.begin(), raw.end());
	TableOpType optype = (TableOpType)getFieldU16(sfOpType);
	if (!isSqlStatementOpType(optype))
		return sRaw;
	if (sOperationRule.empty())
	{
		return sRaw;
	}
	using MapRule = std::map<std::string, Json::Value>;

	Json::Value finalRaw;
	Json::Value raw_json;
	Json::Reader().parse(sRaw, raw_json);
	switch (optype)
	{
	case R_INSERT:
	{
		MapRule mapRule;
		if (!sOperationRule.empty())
		{
			Json::Value jsonRule;
			if (Json::Reader().parse(sOperationRule, jsonRule))
			{
				Json::Value& condition = jsonRule[jss::Condition];
				std::vector<std::string> members = condition.getMemberNames();
				// retrieve members in object
				for (size_t i = 0; i < members.size(); i++) {
					std::string field_name = members[i];
					mapRule[field_name] = condition[field_name];
				}
			}
		}
		for (Json::UInt idx = 0; idx < raw_json.size(); idx++) {
			auto& v = raw_json[idx];
			std::vector<std::string> members = v.getMemberNames();
			// deal with operation rule,fill those field not filled
			for (auto iter = mapRule.begin(); iter != mapRule.end(); iter++)
			{
				if (std::find(members.begin(), members.end(), iter->first) == members.end())
				{
					Json::Value fieldValue = iter->second;
					if (fieldValue.asString() == "$account")
						fieldValue = to_string(getAccountID(sfAccount));
					else if (iter->second == "$tx_hash")
						fieldValue = to_string(getTransactionID());
					v[iter->first] = fieldValue;
				}
			}
		}
		finalRaw = raw_json;
		break;
	}
	case R_UPDATE:
	{
		Json::Value conditions;
		// parse record
		for (Json::UInt idx = 0; idx < raw_json.size(); idx++) {
			auto& v = raw_json[idx];
			if (idx != 0) {
				conditions.append(v);
			}
		}
		if (conditions.isArray() && conditions.size() > 0)
		{
			Json::Value newRaw;
			buildRaw(conditions, sOperationRule);
		}
		else {
			Json::Value jsonRule;
			Json::Reader().parse(sOperationRule, jsonRule);
			conditions = jsonRule;
		}

		finalRaw.append(raw_json[(Json::UInt)0]);
		for (Json::UInt idx = 0; idx < conditions.size(); idx++)
		{
			finalRaw.append(conditions[idx]);
		}
		break;
	}
	case R_DELETE:
	{
		Json::Value newRaw;
		buildRaw(raw_json, sOperationRule);
		finalRaw = raw_json;
		break;
	}
	default:
		break;
	}
	return finalRaw.toStyledString();
}

void STTx::buildRaw(Json::Value& condition, std::string& rule) const
{
	Json::Value finalRaw;
	Json::Value finalObj(Json::objectValue);
	Json::Value jsonRule;
	StringReplace(rule, "$account", to_string(getAccountID(sfAccount)));
	Json::Reader().parse(rule, jsonRule);
	if (!jsonRule.isMember(jss::Condition))
		return;

	Json::Value rawObj;
	if (condition.size() == 1)
	{
		rawObj = condition[0u];
	}
	else if(condition.size() > 1)
	{
		Json::Value jsonOr;
		jsonOr["$or"] = condition;
		rawObj= jsonOr;
	}

	Json::Value ruleCondition = jsonRule[jss::Condition];
	Json::Value finalRule;
	if (ruleCondition.isArray())
	{
		if (ruleCondition.size() > 1)
		{
			finalRule["$or"] = ruleCondition;
		}
	}		
	else
		finalRule = ruleCondition;

	if (rawObj.size() != 0)
	{
		Json::Value finalCondition(Json::arrayValue);
		finalCondition.append(rawObj);
		finalCondition.append(finalRule);
		finalObj["$and"] = finalCondition;
	}
	else
	{
		finalObj = finalRule;
	}
	finalRaw.append(finalObj);

	std::swap(finalRaw, condition);
}

std::vector<STTx> STTx::getTxs(STTx& tx,std::string sTableNameInDB)
{
	std::vector<STTx> vec;
	if (tx.getTxnType() == ttSQLTRANSACTION)
	{
		Blob txs_blob = tx.getFieldVL(sfStatements);
		std::string txs_str;

		ripple::AccountID accountID = tx.getAccountID(sfAccount);

		txs_str.assign(txs_blob.begin(), txs_blob.end());
		Json::Value objs;
		Json::Reader().parse(txs_str, objs);

		for (auto obj : objs)
		{
			//int type = obj["OpType"].asInt();
			//if (type == T_ASSERT) continue;
			auto tx_pair = parseSTTx(obj, accountID);
			auto tx = *tx_pair.first;
			getOneTx(vec,tx,sTableNameInDB);
		}
	}
	else
	{
		getOneTx(vec, tx, sTableNameInDB);
	}

	return vec;
}

void STTx::getOneTx(std::vector<STTx>& vec, STTx& tx, std::string sTableNameInDB)
{
	if (sTableNameInDB == "")
	{
		vec.push_back(std::move(tx));
	}
	else
	{
		//check name
		ripple::STArray tables = tx.getFieldArray(sfTables);
		if (tables.size() > 0 && tables[0].isFieldPresent(sfNameInDB))
		{
			if (to_string(tables[0].getFieldH160(sfNameInDB)) == sTableNameInDB)
			{
				vec.push_back(std::move(tx));
			}
		}
	}
}

std::string
STTx::getFullText () const
{
    std::string ret = "\"";
    ret += to_string (getTransactionID ());
    ret += "\" = {";
    ret += STObject::getFullText ();
    ret += "}";
    return ret;
}

boost::container::flat_set<AccountID>
STTx::getMentionedAccounts () const
{
    boost::container::flat_set<AccountID> list;

    for (auto const& it : *this)
    {
        if (auto sa = dynamic_cast<STAccount const*> (&it))
        {
            assert(! sa->isDefault());
            if (! sa->isDefault())
                list.insert(sa->value());
        }
        else if (auto sa = dynamic_cast<STAmount const*> (&it))
        {
            auto const& issuer = sa->getIssuer ();
            if (! isZXC (issuer))
                list.insert(issuer);
        }
    }

    return list;
}

static Blob getSigningData (STTx const& that)
{
    Serializer s;
    s.add32 (HashPrefix::txSign);
    that.addWithoutSigningFields (s);
    return s.getData();
}

uint256
STTx::getSigningHash () const
{
    return STObject::getSigningHash (HashPrefix::txSign);
}

Blob STTx::getSignature () const
{
    try
    {
        return getFieldVL (sfTxnSignature);
    }
    catch (std::exception const&)
    {
        return Blob ();
    }
}

void STTx::sign (
    PublicKey const& publicKey,
    SecretKey const& secretKey)
{
    auto const data = getSigningData (*this);

    auto const sig = ripple::sign (
        publicKey,
        secretKey,
        makeSlice(data));

    setFieldVL (sfTxnSignature, sig);
    tid_ = getHash(HashPrefix::transactionID);
}

std::pair<bool, std::string> STTx::checkSign(bool allowMultiSign) const
{
    std::pair<bool, std::string> ret {false, ""};
    try
    {
        if (allowMultiSign)
        {
            // Determine whether we're single- or multi-signing by looking
            // at the SigningPubKey.  It it's empty we must be
            // multi-signing.  Otherwise we're single-signing.
            Blob const& signingPubKey = getFieldVL (sfSigningPubKey);
            ret = signingPubKey.empty () ?
                checkMultiSign () : checkSingleSign ();
        }
        else
        {
            ret = checkSingleSign ();
        }
    }
    catch (std::exception const&)
    {
        ret = {false, "Internal signature check failure."};
    }
    return ret;
}

Json::Value STTx::getJson (int) const
{
    Json::Value ret = STObject::getJson (0);

    ret[jss::hash] = to_string (getTransactionID ());
    return ret;
}

Json::Value STTx::getJson (int options, bool binary) const
{
    if (binary)
    {
        Json::Value ret;
        Serializer s = STObject::getSerializer ();
        ret[jss::tx] = strHex (s.peekData ());
        ret[jss::hash] = to_string (getTransactionID ());
        return ret;
    }
    return getJson(options);
}

std::string const&
STTx::getMetaSQLInsertReplaceHeader ()
{
    static std::string const sql = "INSERT OR REPLACE INTO Transactions "
        "(TransID, TransType, FromAcct, FromSeq, LedgerSeq, Status, RawTxn, TxnMeta)"
        " VALUES ";

    return sql;
}

std::string STTx::getMetaSQL (std::uint32_t inLedger,
                                               std::string const& escapedMetaData) const
{
    Serializer s;
    add (s);
    return getMetaSQL (s, inLedger, TXN_SQL_VALIDATED, escapedMetaData);
}

// VFALCO This could be a free function elsewhere
std::string
STTx::getMetaSQL (Serializer rawTxn,
    std::uint32_t inLedger, char status, std::string const& escapedMetaData) const
{
    static boost::format bfTrans ("('%s', '%s', '%s', '%d', '%d', '%c', %s, %s)");
    std::string rTxn = sqlEscape (rawTxn.peekData ());

    auto format = TxFormats::getInstance().findByType (tx_type_);
    assert (format != nullptr);

    return str (boost::format (bfTrans)
                % to_string (getTransactionID ()) % format->getName ()
                % toBase58(getAccountID(sfAccount))
                % getSequence () % inLedger % status % rTxn % escapedMetaData);
}

std::pair<bool, std::string> STTx::checkSingleSign () const
{
    // We don't allow both a non-empty sfSigningPubKey and an sfSigners.
    // That would allow the transaction to be signed two ways.  So if both
    // fields are present the signature is invalid.
    if (isFieldPresent (sfSigners))
        return {false, "Cannot both single- and multi-sign."};

    bool validSig = false;
    try
    {
        bool const fullyCanonical = (getFlags() & tfFullyCanonicalSig);
        auto const spk = getFieldVL (sfSigningPubKey);

        if (publicKeyType (makeSlice(spk)))
        {
            Blob const signature = getFieldVL (sfTxnSignature);
            Blob const data = getSigningData (*this);

            validSig = verify (
                PublicKey (makeSlice(spk)),
                makeSlice(data),
                makeSlice(signature),
                fullyCanonical);
        }
    }
    catch (std::exception const&)
    {
        // Assume it was a signature failure.
        validSig = false;
    }
    if (validSig == false)
        return {false, "Invalid signature."};

    return {true, ""};
}

std::pair<bool, std::string> STTx::checkMultiSign () const
{
    // Make sure the MultiSigners are present.  Otherwise they are not
    // attempting multi-signing and we just have a bad SigningPubKey.
    if (!isFieldPresent (sfSigners))
        return {false, "Empty SigningPubKey."};

    // We don't allow both an sfSigners and an sfTxnSignature.  Both fields
    // being present would indicate that the transaction is signed both ways.
    if (isFieldPresent (sfTxnSignature))
        return {false, "Cannot both single- and multi-sign."};

    STArray const& signers {getFieldArray (sfSigners)};

    // There are well known bounds that the number of signers must be within.
    if (signers.size() < minMultiSigners || signers.size() > maxMultiSigners)
        return {false, "Invalid Signers array size."};

    // We can ease the computational load inside the loop a bit by
    // pre-constructing part of the data that we hash.  Fill a Serializer
    // with the stuff that stays constant from signature to signature.
    Serializer const dataStart {startMultiSigningData (*this)};

    // We also use the sfAccount field inside the loop.  Get it once.
    auto const txnAccountID = getAccountID (sfAccount);

    // Determine whether signatures must be full canonical.
    bool const fullyCanonical = (getFlags() & tfFullyCanonicalSig);

    // Signers must be in sorted order by AccountID.
    AccountID lastAccountID (beast::zero);

    for (auto const& signer : signers)
    {
        auto const accountID = signer.getAccountID (sfAccount);

        // The account owner may not multisign for themselves.
        if (accountID == txnAccountID)
            return {false, "Invalid multisigner."};

        // No duplicate signers allowed.
        if (lastAccountID == accountID)
            return {false, "Duplicate Signers not allowed."};

        // Accounts must be in order by account ID.  No duplicates allowed.
        if (lastAccountID > accountID)
            return {false, "Unsorted Signers array."};

        // The next signature must be greater than this one.
        lastAccountID = accountID;

        // Verify the signature.
        bool validSig = false;
        try
        {
            Serializer s = dataStart;
            finishMultiSigningData (accountID, s);

            auto spk = signer.getFieldVL (sfSigningPubKey);

            if (publicKeyType (makeSlice(spk)))
            {
                Blob const signature =
                    signer.getFieldVL (sfTxnSignature);

                validSig = verify (
                    PublicKey (makeSlice(spk)),
                    s.slice(),
                    makeSlice(signature),
                    fullyCanonical);
            }
        }
        catch (std::exception const&)
        {
            // We assume any problem lies with the signature.
            validSig = false;
        }
        if (!validSig)
            return {false, std::string("Invalid signature on account ") +
                toBase58(accountID)  + "."};
    }

    // All signatures verified.
    return {true, ""};
}

//------------------------------------------------------------------------------

static
bool
isMemoOkay (STObject const& st, std::string& reason)
{
    if (!st.isFieldPresent (sfMemos))
        return true;

    auto const& memos = st.getFieldArray (sfMemos);

    // The number 2048 is a preallocation hint, not a hard limit
    // to avoid allocate/copy/free's
    Serializer s (2048);
    memos.add (s);

    // FIXME move the memo limit into a config tunable
    if (s.getDataLength () > 1024)
    {
        reason = "The memo exceeds the maximum allowed size.";
        return false;
    }

    for (auto const& memo : memos)
    {
        auto memoObj = dynamic_cast <STObject const*> (&memo);

        if (!memoObj || (memoObj->getFName() != sfMemo))
        {
            reason = "A memo array may contain only Memo objects.";
            return false;
        }

        for (auto const& memoElement : *memoObj)
        {
            auto const& name = memoElement.getFName();

            if (name != sfMemoType &&
                name != sfMemoData &&
                name != sfMemoFormat)
            {
                reason = "A memo may contain only MemoType, MemoData or "
                         "MemoFormat fields.";
                return false;
            }

            // The raw data is stored as hex-octets, which we want to decode.
            auto data = strUnHex (memoElement.getText ());

            if (!data.second)
            {
                reason = "The MemoType, MemoData and MemoFormat fields may "
                         "only contain hex-encoded data.";
                return false;
            }

            if (name == sfMemoData)
                continue;

            // The only allowed characters for MemoType and MemoFormat are the
            // characters allowed in URLs per RFC 3986: alphanumerics and the
            // following symbols: -._~:/?#[]@!$&'()*+,;=%
            static std::array<char, 256> const allowedSymbols = []
            {
                std::array<char, 256> a;
                a.fill(0);

                std::string symbols (
                    "0123456789"
                    "-._~:/?#[]@!$&'()*+,;=%"
                    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                    "abcdefghijklmnopqrstuvwxyz");

                for(char c : symbols)
                    a[c] = 1;
                return a;
            }();

            for (auto c : data.first)
            {
                if (!allowedSymbols[c])
                {
                    reason = "The MemoType and MemoFormat fields may only "
                             "contain characters that are allowed in URLs "
                             "under RFC 3986.";
                    return false;
                }
            }
        }
    }

    return true;
}

// Ensure all account fields are 160-bits
static
bool
isAccountFieldOkay (STObject const& st)
{
    for (int i = 0; i < st.getCount(); ++i)
    {
        auto t = dynamic_cast<STAccount const*>(st.peekAtPIndex (i));
        if (t && t->isDefault ())
            return false;
    }

    return true;
}

bool passesLocalChecks (STObject const& st, std::string& reason)
{
    if (!isMemoOkay (st, reason))
        return false;

    if (!isAccountFieldOkay (st))
    {
        reason = "An account field is invalid.";
        return false;
    }

    return true;
}

std::shared_ptr<STTx const>
sterilize (STTx const& stx)
{
    Serializer s;
    stx.add(s);
    SerialIter sit(s.slice());
    return std::make_shared<STTx const>(std::ref(sit));
}

} // ripple
