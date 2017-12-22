//------------------------------------------------------------------------------
/*
 This file is part of chainsqld: https://github.com/chainsql/chainsqld
 Copyright (c) 2016-2018 Peersafe Technology Co., Ltd.
 
	chainsqld is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.
 
	chainsqld is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.
	You should have received a copy of the GNU General Public License
	along with cpp-ethereum.  If not, see <http://www.gnu.org/licenses/>.
 */
//==============================================================================

#include <BeastConfig.h>
#include <ripple/app/ledger/LedgerMaster.h>
#include <ripple/app/ledger/TransactionMaster.h>
#include <ripple/protocol/JsonFields.h>
#include <ripple/protocol/digest.h>
#include <ripple/protocol/RippleAddress.h>
#include <ripple/json/json_reader.h>
#include <ripple/rpc/impl/RPCHelpers.h>
#include <ripple/app/misc/Transaction.h>
#include <peersafe/protocol/TableDefines.h>
#include <peersafe/rpc/impl/TxPrepareBase.h>
#include <peersafe/rpc/impl/TableAssistant.h>
#include <peersafe/rpc/impl/TableUtils.h>

namespace ripple {
TxPrepareBase::TxPrepareBase(Application& app, const std::string& secret, const std::string& publickey, Json::Value& tx_json, getCheckHashFunc func, bool ws):
	app_(app),
	secret_(secret),
    public_(publickey),
	tx_json_(tx_json),
	getCheckHashFunc_(func),
	ws_(ws)
{
	m_bConfidential = false;
	sTableName_ = "";
	u160NameInDB_ = beast::zero;
}

TxPrepareBase::~TxPrepareBase()
{

}

bool TxPrepareBase::isConfidential()
{
	if (ws_)
	{
		int opType = tx_json_[jss::OpType].asInt();
		if (opType == T_CREATE)
		{
			if (tx_json_.isMember(jss::Token))
				return true;
		}
		else 
		{		
			if (checkConfidential(ownerID_, sTableName_))
				return true;
		}
	}

	return m_bConfidential;
}

Json::Value& TxPrepareBase::getTxJson()
{
	return tx_json_;
}

Json::Value TxPrepareBase::prepare()
{
	auto ret = prepareBase();
	if (ret.isMember("error_message") || ws_)
		return ret;
	return prepareVL(getTxJson());
}

Json::Value TxPrepareBase::prepareVL(Json::Value& json)
{
	Json::Value jsonRet;
	if (!json.isObject())
	{
		jsonRet[jss::error] = "value type is not object.";
		return jsonRet;
	}

	for (auto const& fieldName : json.getMemberNames())
	{
		Json::Value& value = json[fieldName];

		auto const& field = SField::getField(fieldName);

		switch (field.fieldType)
		{
		case STI_ARRAY:		
			try
			{
				if (!value.isArray())
				{
					jsonRet[jss::error] = "key : " + fieldName + ", value type is not array.";
					std::move(jsonRet);
				}

				for (auto &valueItem : value)
				{
					if (!valueItem.isObject())  continue;

					jsonRet = prepareVL(valueItem);
					if (jsonRet.isMember(jss::error))	std::move(jsonRet);
				}
				
			}
			catch (std::exception const& e) 
			{
				jsonRet[jss::error] = e.what();
				std::move(jsonRet);
			}
			break;
		case STI_OBJECT:
			try
			{
				jsonRet = prepareVL(value);
				if (jsonRet.isMember(jss::error))	return jsonRet;				
			}
			catch (std::exception const& e)
			{
				jsonRet[jss::error] = e.what();
				std::move(jsonRet);
			}
			break;
		case STI_VL:
			try
			{
				std::string strValue;
				if (value.isArray() || value.isObject())
					strValue = value.toStyledString();
				else
					strValue = value.asString();
				json[fieldName] = strHex(strValue);
			}
			catch (std::exception const& e)
			{
				jsonRet[jss::error] = e.what();
				std::move(jsonRet);
			}
			break;
		default:
			break;
		}
	}
	
	return jsonRet;
}

Json::Value TxPrepareBase::prepareGetRaw()
{
	Json::Value ret(Json::objectValue);
	auto& tx_json = getTxJson();

	if (tx_json["OpType"] != T_RECREATE)   return ret;

	table_BaseInfo baseinfo = app_.getLedgerMaster().getTableBaseInfo(app_.getLedgerMaster().getValidLedgerIndex(), ownerID_, sTableName_);
	auto txn = app_.getMasterTransaction().fetch(baseinfo.createdTxnHash, true);
	if (!txn)
	{
		return generateError("can not find create tx in local disk,please change node or try later", ws_);;
	}
	auto stTx = txn->getSTransaction();
	auto vecTxs = STTx::getTxs(const_cast<STTx&>(*stTx.get()), to_string(baseinfo.nameInDB));
	for (auto& tx : vecTxs)
	{
		auto optype = tx.getFieldU16(sfOpType);
		if (optype == T_CREATE || optype == T_RECREATE)
		{
			auto blob = stTx->getFieldVL(sfRaw);
			std::string str(blob.begin(), blob.end());
			tx_json[jss::Raw] = strHex(str);
		}
	}

	return ret;
}

Json::Value TxPrepareBase::prepareBase()
{
    //check the future hash
    auto ret = prepareFutureHash(tx_json_, app_, ws_);
    if (ret.isMember("error_message"))
        return ret;

	//actually, this fun get base info: account ,stableName ,nameInDB
	ret = prepareDBName();
	if (ret.isMember("error_message"))
		return ret;
    
	//prepare raw for recreate operation
    ret = prepareGetRaw();
    if (ret.isMember("error_message"))
        return ret;	

	if (!ws_)
	{
		ret = prepareRawEncode();
		if (ret.isMember("error_message"))
			return ret;
	}

	ret = prepareStrictMode();
	if (ret.isMember("error_message"))
		return ret;

	if(app_.getTableSync().IsPressSwitchOn())
		preparePressData();

	return ret;
}

void TxPrepareBase::preparePressData()
{
	std::chrono::time_point<std::chrono::system_clock, std::chrono::seconds> tp = std::chrono::time_point_cast<std::chrono::seconds>(std::chrono::system_clock::now());
	auto tmp = std::chrono::duration_cast<std::chrono::seconds>(tp.time_since_epoch());
	auto& tx_json = getTxJson();
	tx_json[jss::Flags] = (uint32)tmp.count();
}

uint256 TxPrepareBase::getCheckHashOld(const std::string& sAccount, const std::string& sTableName)
{
	return beast::zero;
}

Blob TxPrepareBase::getPassblobExtra(const std::string& sAccount, const std::string& sTableName)
{
	Blob b;
	return b;
}
void TxPrepareBase::updateInfo(const std::string& sAccount, const std::string& sTableName, const std::string& sNameInDB)
{
	sTableName_    = sTableName;
	u160NameInDB_  = from_hex_text<uint160>(sNameInDB);
	auto pOwner = ripple::parseBase58<AccountID>(sAccount);
	if(boost::none != pOwner)
		ownerID_   = *pOwner;
}

std::string TxPrepareBase::getNameInDB(const std::string& sAccount, const std::string& sTableName)
{
    if (u160NameInDB_ == beast::zero)
        return "";
	return to_string(u160NameInDB_);
}

Json::Value TxPrepareBase::prepareStrictMode()
{
	Json::Value ret(Json::objectValue);
	auto& tx_json = getTxJson();

	if (!isStrictModeOpType((TableOpType)tx_json[jss::OpType].asInt()))  return ret;
	if (!tx_json.isMember(jss::Raw))		                             return ret;
	if (!tx_json_.isMember(jss::StrictMode) || !tx_json_[jss::StrictMode].asBool())
		return ret;
	//parse raw
	std::string sRaw;
	if (tx_json[jss::Raw].isArray())				
		sRaw = tx_json[jss::Raw].toStyledString();
	else         				                    
		sRaw = tx_json[jss::Raw].asString();
	if (ws_)
	{
		auto rawPair = strUnHex(sRaw);
		if (!rawPair.second)
			return generateError("Raw should be hexed", ws_);
		sRaw = strCopy(rawPair.first);
	}

	uint256 checkHashNew;
	uint256 checkHash;
	if (T_CREATE != tx_json[jss::OpType].asInt())
	{
		auto retPair = getCheckHash(to_string(ownerID_), sTableName_);
		if (retPair.first.isZero())
			return generateError("Please make sure table exist or to be created in this transaction", ws_);
		checkHash = retPair.first;
	}

	ret = prepareCheckHash(sRaw, checkHash, checkHashNew);
	if (ret.isMember("error_message"))
		return ret;
    if(isStrictModeOpType((TableOpType)(tx_json[jss::OpType].asInt())))
        updateCheckHash(to_string(ownerID_), sTableName_, checkHashNew);
	return ret;
}

std::pair<uint256, Json::Value> TxPrepareBase::getCheckHash(const std::string& sAccountId, const std::string& sTableName)
{	
	Json::Value jvRet;
	uint256 checkHash = beast::zero;

	checkHash = getCheckHashOld(sAccountId, sTableName);
	if (checkHash == beast::zero)
	{		
		checkHash = getCheckHashFunc_(u160NameInDB_);

		if (checkHash.isZero())
		{
			auto accountID = ripple::parseBase58<AccountID>(sAccountId);
			auto ret = app_.getLedgerMaster().getLatestTxCheckHash(*accountID, sTableName);
			checkHash = ret.first;
			if (checkHash.isZero())
				return std::make_pair(checkHash, generateError("GetCheckHash failed,checkHash is empty.", ws_));
		}
	}
	return std::make_pair(checkHash, jvRet);
}

Json::Value TxPrepareBase::prepareDBName()
{
	auto& tx_json = getTxJson();
	std::string accountId;

	if (tx_json.isMember(jss::Owner) && tx_json[jss::Owner].asString().size() != 0)
		accountId = tx_json[jss::Owner].asString();
	else if (tx_json.isMember(jss::Account) && tx_json[jss::Account].asString().size() != 0)
		accountId = tx_json[jss::Account].asString();
	else
		return generateError("Account is missing,please checkout!", ws_);

	// fill NameInDB
	Json::Value& tables_json(tx_json[jss::Tables]);
	for (auto& json : tables_json)
	{
		if (json[jss::Table][jss::TableName].asString().size() == 0)
		{
			return generateError("TableName is missing,please checkout!", ws_);
		}

        std::string sNameInDB = "";
        std::string sTableName = json[jss::Table][jss::TableName].asString();
		if (json[jss::Table][jss::NameInDB].asString().size() == 0)
		{			
            if (ws_)
            {
                auto tablePair = strUnHex(sTableName);
                if (tablePair.second)
                    sTableName = strCopy(tablePair.first);
            }
			sNameInDB = getNameInDB(accountId, sTableName);
			if (sNameInDB.size() > 0)
				json[jss::Table][jss::NameInDB] = sNameInDB;
			else
			{				
				Json::Value ret = app_.getTableAssistant().getDBName(accountId, sTableName);
				if (ret["status"].asString() == "error")
				{
					return ret;
				}

				sNameInDB = ret["nameInDB"].asString();
				if (sNameInDB.size() > 0)
					json[jss::Table][jss::NameInDB] =sNameInDB;
				updateNameInDB(accountId, sTableName, sNameInDB);
			}            
		}
        else
        {
            sNameInDB = json[jss::Table][jss::NameInDB].asString();
            updateNameInDB(accountId, sTableName, sNameInDB);            
        }
        updateInfo(accountId, sTableName, sNameInDB);
	}
	Json::Value ret(Json::objectValue);
	return ret;
}

Json::Value TxPrepareBase::prepareRawEncode()
{
	int opType = tx_json_[jss::OpType].asInt();
	if (opType == T_CREATE || opType == T_ASSERT || opType == R_INSERT || opType == R_UPDATE || opType == T_GRANT)
	{
        auto rawStr = tx_json_[jss::Raw].toStyledString();
        if (rawStr.size() == 0)
		{
			return generateError("Raw is missing,please checkout!", ws_);
		}
        else
        {
            if (!tx_json_[jss::Raw].isArray() || tx_json_[jss::Raw].size() == 0)
                return generateError("[] in Raw is empty, please checkout!", ws_);
        }
	}

	if (opType == T_CREATE)
	{
		if (tx_json_.isMember(jss::Confidential))
		{
            if (tx_json_[jss::Confidential].asBool())
            {
                auto ret = prepareForCreate();
                if (ret.isMember("error_message"))
                {
                    return ret;
                }
                
                m_bConfidential = true;
            }
            tx_json_.removeMember(jss::Confidential);
		}
	}
	else if (opType == T_ASSIGN || opType == T_GRANT)
	{		
		if (checkConfidential(ownerID_, sTableName_))
		{
			auto ret = prepareForAssign();
			if (ret.isMember("error_message"))
			{
				return ret;
			}
			m_bConfidential = true;
		}
		tx_json_.removeMember("PublicKey");
	}
	else if (opType == R_INSERT ||
		opType == R_DELETE ||
		opType == R_UPDATE ||
		opType == T_ASSERT)
	{		
		if (checkConfidential(ownerID_, sTableName_))
		{
			auto ret = prepareForOperating();
			if (ret.isMember("error_message"))
			{
				return ret;
			}
			m_bConfidential = true;
		}
	}
	Json::Value ret(Json::objectValue);
	return ret;
}

Json::Value TxPrepareBase::parseTableName()
{
	Json::Value json(Json::stringValue);
	if (tx_json_[jss::Tables].size() == 0)
		return generateError("Tables is missing", ws_);
	auto sTableName = tx_json_[jss::Tables][0u][jss::Table][jss::TableName].asString();
	if(sTableName.empty())
		return generateError("TableName is empty", ws_);
	json = sTableName;
	return json;
}

Json::Value TxPrepareBase::prepareCheckHash(const std::string& sRaw,const uint256& checkHash,uint256& checkHashNew)
{
    int opType = tx_json_[jss::OpType].asInt();
    if (opType == T_CREATE)
        checkHashNew = sha512Half(makeSlice(sRaw));
    else
        checkHashNew = sha512Half(makeSlice(sRaw), checkHash);

	if (tx_json_.isMember(jss::StrictMode))
	{
        if (tx_json_[jss::StrictMode].asBool())
            tx_json_[jss::TxCheckHash] = to_string(checkHashNew);            

        tx_json_.removeMember(jss::StrictMode);		
	}
	
	Json::Value ret(Json::objectValue);
	return ret;
}

std::pair<Blob, Json::Value> TxPrepareBase::getPassBlob(AccountID& ownerId, AccountID& userId,
	boost::optional<SecretKey> secret_key)
{
	auto ret = getPassblobExtra(to_string(ownerId), sTableName_);
	if (ret.size() > 0)
		return std::make_pair(ret, "");
	return getPassBlobBase(ownerId,userId,secret_key);
}

std::pair<Blob, Json::Value> TxPrepareBase::getPassBlobBase(AccountID& ownerId, AccountID& userId,
	boost::optional<SecretKey> secret_key)
{
	Json::Value jvResult;
	Blob passBlob;
	//decrypt passBlob
	std::pair<Blob, Json::Value> result;

	bool bRet = false;
	std::string sError;
	std::tie(bRet, passBlob, sError) = app_.getLedgerMaster().getUserToken(userId, ownerId, sTableName_);

	if (!bRet)
	{
        jvResult = generateError(sError, ws_);
		result = std::make_pair(passBlob, jvResult);
		return result;
	}		

	//table not encrypted
	if (passBlob.size() == 0)
	{
		return std::make_pair(passBlob, jvResult);
	}
	//passBlob = RippleAddress::decryptPassword(passBlob, *secret_key);
    passBlob = ripple::decrypt(passBlob, *secret_key);
	if (passBlob.size() == 0)
	{
		jvResult = generateError("Decrypt password failed!", ws_);
		return std::pair<Blob, Json::Value>(passBlob, jvResult);
	}
	return std::pair<Blob, Json::Value>(passBlob, jvResult);
}

Json::Value TxPrepareBase::prepareForCreate()
{
	Json::Value ret;
	//get public key
	//auto oPublic_key = RippleAddress::getPublicKey(secret_);
    
    PublicKey public_key;
    if (!public_.empty())
    {
        std::string publicKeyDe58 = decodeBase58Token(public_, TOKEN_ACCOUNT_PUBLIC);
        if (publicKeyDe58.empty() || publicKeyDe58.size() != 65)
        {
            return generateError("Parse publicKey failed, please checkout!", ws_);
        }
        PublicKey tempPubKey(Slice(publicKeyDe58.c_str(), publicKeyDe58.size()));
        public_key = tempPubKey;
    }
    else
    {
        //boost::optional<PublicKey> oPublic_key;
        auto oPublic_key = ripple::getPublicKey(secret_);
        if (!oPublic_key)
        {
            return generateError("Secret error,please checkout!", ws_);
        }
        else
        {
            public_key = *oPublic_key;
        }
    }

    std::string raw = tx_json_[jss::Raw].toStyledString();
    Blob raw_blob = strCopy(raw);
    Blob rawCipher;
    HardEncrypt* hEObj = HardEncryptObj::getInstance();
    Blob plainBlob;
    // get random password
    Blob passBlob = RippleAddress::getRandomPassword();
    if (nullptr != hEObj)
    {
        //unsigned char sessionKey[512] = { 0 };
        //unsigned long sessionKeyLen = 512;
        const int plainPaddingMaxLen = 16;
        unsigned char* pCipherData = new unsigned char[raw_blob.size()+ plainPaddingMaxLen];
        unsigned long cipherDataLen = raw_blob.size()+ plainPaddingMaxLen;
        //hEObj->SM4GenerateSessionKey(sessionKey, &sessionKeyLen);
        //hEObj->SM4SymEncrypt(sessionKey, sessionKeyLen, raw_blob.data(), raw_blob.size(), cipherData, &cipherDataLen);
        hEObj->SM4SymEncrypt(passBlob.data(), passBlob.size(), raw_blob.data(), raw_blob.size(), pCipherData, &cipherDataLen);
        //passBlob = Blob(sessionKey, sessionKey + sessionKeyLen);
        rawCipher = Blob(pCipherData, pCipherData + cipherDataLen);
        delete [] pCipherData;
        /*hEObj->SM4SymDecrypt(sessionKey, sessionKeyLen, cipherData, cipherDataLen, decipherData, &decipherDataLen);
        plainBlob = Blob(decipherData, decipherData + decipherDataLen);
        if (raw_blob == plainBlob)
            DebugPrint("good");
        else DebugPrint("wrong");*/
    }
    else
    {
        //get password cipher
        rawCipher = RippleAddress::encryptAES(passBlob, raw_blob);
    }
    tx_json_[jss::Token] = strCopy(ripple::encrypt(passBlob, public_key));
	if (rawCipher.size() > 0)
	{
		tx_json_[jss::Raw] = strCopy(rawCipher);
	}
	else
	{
		return generateError("encrypt raw failed,please checkout!", ws_);
	}
	
	updatePassblob(to_string(ownerID_), sTableName_, passBlob);

	return ret;
}

Json::Value TxPrepareBase::prepareForAssign()
{
	Json::Value ret;

    PublicKey public_key;
    SecretKey secret_key;
	std::string sPublic_key = tx_json_["PublicKey"].asString();
    if (nullptr == HardEncryptObj::getInstance())
    {
        auto oPublicKey = parseBase58<PublicKey>(TOKEN_ACCOUNT_PUBLIC, sPublic_key);
        if (!oPublicKey)
        {
            return generateError("Parse publickey failed,please checkout!", ws_);
        }
        if (tx_json_["User"].asString() != toBase58(calcAccountID(*oPublicKey)))
            return generateError("PublicKey is not compatible with User!", ws_);
        public_key = *oPublicKey;

        //boost::optional<SecretKey> secret_key = RippleAddress::getSecretKey(secret_);
        boost::optional<SecretKey> oSecret_key = ripple::getSecretKey(secret_);
        if (!oSecret_key)
        {
            return generateError("Secret is missing,please checkout!", ws_);
        }
        else
        {
            secret_key = *oSecret_key;
        }
    }
    else
    {
        std::string publicKeyDe58 = decodeBase58Token(sPublic_key, TOKEN_ACCOUNT_PUBLIC);
        if (publicKeyDe58.empty())
        {
            return generateError("Parse publickey failed,please checkout!", ws_);
        }
        PublicKey tempPubKey(Slice(publicKeyDe58.c_str(), publicKeyDe58.size()));

        if (tx_json_["User"].asString() != toBase58(calcAccountID(tempPubKey)))
        {
            return generateError("PublicKey is not compatible with User!", ws_);
        }
        public_key = tempPubKey;

        std::string privateKeyStrDe58 = decodeBase58Token(secret_, TOKEN_ACCOUNT_SECRET);
        if (privateKeyStrDe58.empty() || privateKeyStrDe58.size() != 32)
        {
            return generateError("Parse secret key error,please checkout!", ws_);
        }
        SecretKey tempSecKey(Slice(privateKeyStrDe58.c_str(), strlen(privateKeyStrDe58.c_str())));
        secret_key = tempSecKey;
    }
	std::pair<Blob, Json::Value> result = getPassBlob(ownerID_, ownerID_, secret_key);
	if (result.second.isMember("error_message"))
	{
		return result.second;
	}
	
	//get password cipher
	if(result.first.size() > 0)
        tx_json_[jss::Token] = strCopy(ripple::encrypt(result.first, public_key));
		//tx_json_[jss::Token] = strCopy(RippleAddress::getPasswordCipher(result.first, *oPublicKey));

	return ret;
}

Json::Value TxPrepareBase::prepareForOperating()
{
	Json::Value ret;
    HardEncrypt* hEObj = HardEncryptObj::getInstance();
    SecretKey secret_key;
    if (nullptr == hEObj)
    {
        //boost::optional<SecretKey> secret_key = RippleAddress::getSecretKey(secret_);
        boost::optional<SecretKey> oSecret_key = ripple::getSecretKey(secret_);
        if (!oSecret_key)
        {
            auto jvResult = generateError("Secret is missing,please checkout!", ws_);
            return jvResult;
        }
        secret_key = *oSecret_key;
    }
    else
    {
        std::string privateKeyStrDe58 = decodeBase58Token(secret_, TOKEN_ACCOUNT_SECRET);
        if (privateKeyStrDe58.empty() || privateKeyStrDe58.size() != 32)
        {
            return generateError("Parse secret key error,please checkout!", ws_);
        }
        SecretKey tempSecKey(Slice(privateKeyStrDe58.c_str(), strlen(privateKeyStrDe58.c_str())));
        secret_key = tempSecKey;
    }

	auto userAccountId = ripple::parseBase58<AccountID>(tx_json_["Account"].asString());
	if (!userAccountId)
	{
		return generateError("Account is missing,please checkout!", ws_);
	}

	std::pair<Blob, Json::Value> result = getPassBlob(ownerID_, *userAccountId, secret_key);
	if (result.second.isMember("error"))
	{
		return result.second;
	}
    // r_delete raw can be empty
    if (!tx_json_.isMember(jss::Raw) || tx_json_[jss::Raw].isNull())
        return ret;
    std::string raw = tx_json_[jss::Raw].toStyledString();
    Blob raw_blob = strCopy(raw);
    Blob rawCipher;
    
    Blob passBlob = result.first;
    if (nullptr == hEObj)
    {
        rawCipher = RippleAddress::encryptAES(passBlob, raw_blob);
    }
    else
    {
        const int plainPaddingMaxLen = 16;
        unsigned char* pCipherData = new unsigned char[raw_blob.size()+ plainPaddingMaxLen];
        unsigned long cipherDataLen = raw_blob.size()+ plainPaddingMaxLen;
        hEObj->SM4SymEncrypt(passBlob.data(), passBlob.size(), raw_blob.data(), raw_blob.size(), pCipherData, &cipherDataLen);
        rawCipher = Blob(pCipherData, pCipherData + cipherDataLen);
        delete [] pCipherData;
    }
	
	if (rawCipher.size() > 0)
		tx_json_[jss::Raw] = strCopy(rawCipher);
	else
		return generateError("encrypt raw failed,please checkout!", ws_);

	auto str = tx_json_[jss::Raw].asString();
	return ret;
}
bool TxPrepareBase::checkConfidential(const AccountID& owner, const std::string& tableName)
{
	return checkConfidentialBase(owner, tableName);
}
bool TxPrepareBase::checkConfidentialBase(const AccountID& owner, const std::string& tableName)
{
	auto ledger = app_.getLedgerMaster().getValidatedLedger();
	if (ledger == NULL)  return false;

	auto id = keylet::table(owner);
	auto const tablesle = ledger->read(id);
	if (tablesle == nullptr)
		return false;
	auto aTableEntries = tablesle->getFieldArray(sfTableEntries);

	STEntry const *pEntry = getTableEntry(aTableEntries, tableName);
	if (pEntry != NULL)
	{
		if (pEntry->isFieldPresent(sfUsers))
		{
			auto& users = pEntry->getFieldArray(sfUsers);
			if (users.size() > 0 && users[0].isFieldPresent(sfToken))
			{
				return true;
			}
		}
	}
	return false;
}

Json::Value TxPrepareBase::prepareFutureHash(const Json::Value& tx_json, Application& app,bool bWs)
{
    Json::Value jsonRet(Json::objectValue);
    
    AccountID accountID;
    std::string sCurHash;
    
    if (tx_json.isMember(jss::Account) && tx_json[jss::Account].asString().size() != 0)
    {
		auto pAccount = ripple::parseBase58<AccountID>(tx_json[jss::Account].asString());
		if(pAccount)
			accountID = *pAccount;
		else
			return generateError("Parse Account failed.", bWs);
    }
    else
        return generateError("Account is missing,please checkout!", bWs);

    if (tx_json.isMember(jss::OriginalAddress) || tx_json.isMember(jss::TxnLgrSeq) ||
        tx_json.isMember(jss::CurTxHash) || tx_json.isMember(jss::FutureTxHash))
    {
        if (!tx_json.isMember(jss::OriginalAddress) || tx_json[jss::OriginalAddress].asString().size() == 0)
        {
            return generateError("no OriginalAddress field.", bWs);
        }
        else if (tx_json[jss::OriginalAddress].asString().size() <= 0)
        {
            return generateError("OriginalAddress field is empty.", bWs);
        }

        if (!tx_json.isMember(jss::TxnLgrSeq) || tx_json[jss::TxnLgrSeq].asString().size() == 0)
        {
            return generateError("no TxnLgrSeq field.", bWs);
        }
        else if (tx_json[jss::TxnLgrSeq].asString().size() <= 0)
        {
            return generateError("TxnLgrSeq field is empty.", bWs);
        }

        if (!tx_json.isMember(jss::CurTxHash) || tx_json[jss::CurTxHash].asString().size() == 0)
        {
            return generateError("no CurTxHash field.", bWs);
        }
        else if (tx_json[jss::CurTxHash].asString().size() <= 0)
        {
            return generateError("CurTxHash field is empty.", bWs);
        }
        else
        {
            sCurHash = tx_json[jss::CurTxHash].asString();
        }

        if (!tx_json.isMember(jss::FutureTxHash) || tx_json[jss::FutureTxHash].asString().size() == 0)
        {
            return generateError("no FutureTxHash field.", bWs);
        }
        else if (tx_json[jss::FutureTxHash].asString().size() <= 0)
        {
            return generateError("FutureTxHash field is empty.", bWs);
        }

        bool bRet;
        uint256 hashFuture;
        std::string sError;
        std::tie(bRet, hashFuture, sError) = app.getLedgerMaster().getUserFutureHash(accountID);

        if (!bRet)
        {
            return generateError(sError, bWs);
        }
        else
        {
            if (hashFuture.isNonZero() && hashFuture != from_hex_text<uint256>(sCurHash))
            {
                Json::Value ret = generateError("current hash is not the expected one", bWs);
				ret[jss::FutureTxHash] = to_string(hashFuture);
				return ret;
            }
        }        
    }
    return jsonRet;
}

}
