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
#include <ripple/net/RPCErr.h>
#include <ripple/app/misc/Transaction.h>
#include <peersafe/protocol/TableDefines.h>
#include <peersafe/rpc/impl/TxPrepareBase.h>
#include <peersafe/rpc/impl/TableAssistant.h>
#include <peersafe/rpc/TableUtils.h>
#include <peersafe/app/table/TableSync.h>

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
	if (ret.isMember(jss::error) || ws_)
		return ret;
	return prepareVL(getTxJson());
}

Json::Value TxPrepareBase::prepareVL(Json::Value& json)
{
	Json::Value jsonRet;
	if (!json.isObject())
	{
		return RPC::make_error(rpcINVALID_PARAMS, "value type is not object.");
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
					std::string errMsg = "key : " + fieldName + ", value type is not array.";
					jsonRet = RPC::make_error(rpcINVALID_PARAMS, errMsg);
					std::move(jsonRet);
				}

				for (auto &valueItem : value)
				{
					if (!valueItem.isObject())  continue;

					jsonRet = prepareVL(valueItem);
					if (jsonRet.isMember(jss::error)) std::move(jsonRet);
				}
				
			}
			catch (std::exception const& e) 
			{
				jsonRet = RPC::make_error(rpcGENERAL, e.what());
				std::move(jsonRet);
			}
			break;
		case STI_OBJECT:
			try
			{
				jsonRet = prepareVL(value);
				if (jsonRet.isMember(jss::error))
					return jsonRet;
			}
			catch (std::exception const& e)
			{
				jsonRet = RPC::make_error(rpcGENERAL, e.what());
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
				jsonRet = RPC::make_error(rpcGENERAL, e.what());
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
		std::string errMsg = "can not find create tx in local disk,please change node or try later";
		return RPC::make_error(rpcGENERAL, errMsg);
	}
	auto stTx = txn->getSTransaction();
	auto vecTxs = app_.getMasterTransaction().getTxs(const_cast<STTx&>(*stTx.get()), to_string(baseinfo.nameInDB));
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
	auto ret = checkBaseInfo(tx_json_, app_, ws_);
	if (ret.isMember(jss::error))
		return ret;

    //check the future hash
    ret = prepareFutureHash(tx_json_, app_, ws_);
    if (ret.isMember(jss::error))
        return ret;

	//actually, this fun get base info: account ,stableName ,nameInDB
	ret = prepareDBName();
	if (ret.isMember(jss::error))
		return ret;
    
	//prepare raw for recreate operation
    ret = prepareGetRaw();
    if (ret.isMember(jss::error))
        return ret;	

	if (!ws_)
	{
		ret = prepareRawEncode();
		if (ret.isMember(jss::error))
			return ret;
	}

	ret = prepareStrictMode();
	if (ret.isMember(jss::error))
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
		{
			return RPC::make_error(rpcRAW_INVALID, "Raw should be hexed");
		}
		sRaw = strCopy(rawPair.first);
	}

	uint256 checkHashNew;
	uint256 checkHash;
	if (T_CREATE != tx_json[jss::OpType].asInt())
	{
		auto retPair = getCheckHash(to_string(ownerID_), sTableName_);
		if (retPair.first.isZero())
			return RPC::make_error(rpcTAB_NOT_EXIST, "Please make sure table exist or to be created in this transaction");
		checkHash = retPair.first;
	}

	ret = prepareCheckHash(sRaw, checkHash, checkHashNew);
	if (ret.isMember(jss::error))
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
			{
				Json::Value ret = RPC::make_error(rpcGENERAL, "GetCheckHash failed,checkHash is empty.");
				return std::make_pair(checkHash, ret);
			}
				
		}
	}
	return std::make_pair(checkHash, jvRet);
}

Json::Value TxPrepareBase::prepareDBName()
{
	Json::Value ret(Json::objectValue);
	auto& tx_json = getTxJson();

	std::string accountId;

	if (tx_json.isMember(jss::Owner) && tx_json[jss::Owner].asString().size() != 0)
		accountId = tx_json[jss::Owner].asString();
	else if (tx_json.isMember(jss::Account) && tx_json[jss::Account].asString().size() != 0)
		accountId = tx_json[jss::Account].asString();
	else
		return RPC::missing_field_error(jss::Account);

	// fill NameInDB
	Json::Value& tables_json(tx_json[jss::Tables]);
	for (auto& json : tables_json)
	{
		if (json[jss::Table][jss::TableName].asString().size() == 0)
		{
			return RPC::missing_field_error(jss::TableName);
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

			// get nameInDB from validated ledger
			auto aAccountId = ripple::parseBase58<AccountID>(accountId);
			auto nameInDBInLedger = app_.getLedgerMaster().getNameInDB(app_.getLedgerMaster().getValidLedgerIndex(), *aAccountId, sTableName);

			sNameInDB = getNameInDB(accountId, sTableName);
			if (sNameInDB.size() > 0)
				json[jss::Table][jss::NameInDB] = sNameInDB;
			else if (nameInDBInLedger != beast::zero)
			{
				sNameInDB = to_string(nameInDBInLedger);
				json[jss::Table][jss::NameInDB] = sNameInDB;
				updateNameInDB(accountId, sTableName, sNameInDB);
			}				
			else if(tx_json_[jss::OpType].asInt() == T_CREATE || tx_json_[jss::OpType].asInt() == T_REPORT)
			{				
				// if create table,generate one, else error
				Json::Value ret = app_.getTableAssistant().getDBName(accountId, sTableName);
				if (ret.isMember(jss::error))
				{
					return ret;
				}

				sNameInDB = ret["nameInDB"].asString();
				if (sNameInDB.size() > 0)
					json[jss::Table][jss::NameInDB] = sNameInDB;
				updateNameInDB(accountId, sTableName, sNameInDB);
			}
			else
			{
				std::string errMsg = "Please make sure table exist before this operation!";
				return RPC::make_error(rpcTAB_NOT_EXIST, errMsg);
			}
		}
        else
        {
            sNameInDB = json[jss::Table][jss::NameInDB].asString();
            updateNameInDB(accountId, sTableName, sNameInDB);            
        }
        updateInfo(accountId, sTableName, sNameInDB);
	}
	
	return ret;
}

Json::Value TxPrepareBase::prepareRawEncode()
{
	Json::Value ret(Json::objectValue);
	int opType = tx_json_[jss::OpType].asInt();
	if (opType == T_CREATE || opType == T_ASSERT || opType == R_INSERT || opType == R_UPDATE || opType == T_GRANT)
	{
        auto rawStr = tx_json_[jss::Raw].toStyledString();
        if (rawStr.size() == 0)
		{
			return RPC::missing_field_error(jss::Raw);
		}
        else
        {
			if (!tx_json_[jss::Raw].isArray() || tx_json_[jss::Raw].size() == 0)
			{
				return RPC::make_error(rpcINVALID_PARAMS, "[] in Raw is empty, please checkout!");
			}
        }
	}

	if (opType == T_CREATE)
	{
		if (tx_json_.isMember(jss::Confidential))
		{
            if (tx_json_[jss::Confidential].asBool())
            {
                auto ret = prepareForCreate();
                if (ret.isMember(jss::error))
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
			if (ret.isMember(jss::error))
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
			if (ret.isMember(jss::error))
			{
				return ret;
			}
			m_bConfidential = true;
		}
	}

	return ret;
}

Json::Value TxPrepareBase::parseTableName()
{
	Json::Value json(Json::stringValue);
	if (tx_json_[jss::Tables].size() == 0)
		return RPC::missing_field_error(jss::Tables);
	auto sTableName = tx_json_[jss::Tables][0u][jss::Table][jss::TableName].asString();
	if (sTableName.empty())
		return RPC::make_error(rpcINVALID_PARAMS, "TableName is empty");
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
	error_code_i errCode;
	std::tie(bRet, passBlob, errCode) = app_.getLedgerMaster().getUserToken(userId, ownerId, sTableName_);

	if (!bRet)
	{

		RPC::inject_error(errCode, jvResult);
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
		RPC::inject_error(rpcGENERAL, "Decrypt password failed!", jvResult);
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
			return RPC::make_error(rpcINVALID_PARAMS, "Parse publicKey failed, please checkout!");
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
			return RPC::make_error(rpcINVALID_PARAMS, "Secret error,please checkout!");
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
		return RPC::make_error(rpcGENERAL, "encrypt raw failed,please checkout!");
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
			return RPC::make_error(rpcINVALID_PARAMS, "Parse publicKey failed, please checkout!");
        }
		if (tx_json_["User"].asString() != toBase58(calcAccountID(*oPublicKey)))
		{
			return rpcError(rpcACT_NOT_MATCH_PUBKEY);
		}

        public_key = *oPublicKey;

        //boost::optional<SecretKey> secret_key = RippleAddress::getSecretKey(secret_);
        boost::optional<SecretKey> oSecret_key = ripple::getSecretKey(secret_);
        if (!oSecret_key)
        {
			return RPC::missing_field_error(jss::secret);
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
			return RPC::make_error(rpcINVALID_PARAMS, "Parse publicKey failed, please checkout!");
        }
        PublicKey tempPubKey(Slice(publicKeyDe58.c_str(), publicKeyDe58.size()));

        if (tx_json_["User"].asString() != toBase58(calcAccountID(tempPubKey)))
        {
			return rpcError(rpcACT_NOT_MATCH_PUBKEY);
        }
        public_key = tempPubKey;

        std::string privateKeyStrDe58 = decodeBase58Token(secret_, TOKEN_ACCOUNT_SECRET);
        if (privateKeyStrDe58.empty() || privateKeyStrDe58.size() != 32)
        {
			return RPC::make_error(rpcINVALID_PARAMS, "Parse secret key error,please checkout!");
        }
        SecretKey tempSecKey(Slice(privateKeyStrDe58.c_str(), strlen(privateKeyStrDe58.c_str())));
        secret_key = tempSecKey;
    }
	std::pair<Blob, Json::Value> result = getPassBlob(ownerID_, ownerID_, secret_key);
	if (result.second.isMember(jss::error))
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
			return RPC::missing_field_error(jss::secret);
        }
        secret_key = *oSecret_key;
    }
    else
    {
        std::string privateKeyStrDe58 = decodeBase58Token(secret_, TOKEN_ACCOUNT_SECRET);
        if (privateKeyStrDe58.empty() || privateKeyStrDe58.size() != 32)
        {
			return RPC::make_error(rpcINVALID_PARAMS, "Parse secret key error,please checkout!");
        }
        SecretKey tempSecKey(Slice(privateKeyStrDe58.c_str(), strlen(privateKeyStrDe58.c_str())));
        secret_key = tempSecKey;
    }

	auto userAccountId = ripple::parseBase58<AccountID>(tx_json_["Account"].asString());
	if (!userAccountId)
	{
		return RPC::missing_field_error(jss::Account);
	}

	std::pair<Blob, Json::Value> result = getPassBlob(ownerID_, *userAccountId, secret_key);
	if (result.second.isMember(jss::error))
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
	{
		return RPC::make_error(rpcGENERAL, "encrypt raw failed,please checkout!");
	}

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

	STEntry *pEntry = getTableEntry(aTableEntries, tableName);
	return pEntry ? pEntry->isConfidential():false;
}

Json::Value TxPrepareBase::checkBaseInfo(const Json::Value& tx_json, Application& app, bool bWs)
{
	Json::Value jsonRet(Json::objectValue);
	AccountID accountID;
	if (tx_json.isMember(jss::Account) && tx_json[jss::Account].asString().size() != 0)
	{
		AccountID accountID;
		std::string accountStr = tx_json[jss::Account].asString();
		auto jvAccepted = RPC::accountFromString(accountID, accountStr, true);
		if (jvAccepted)
		{
			return jvAccepted;
		}
	}
	else
		return RPC::missing_field_error(jss::Account);

	if (tx_json.isMember(jss::Owner) && tx_json[jss::Owner].asString().size() != 0)
	{
		AccountID ownerID;
		std::string ownerStr = tx_json[jss::Account].asString();
		auto jvAccepted = RPC::accountFromString(ownerID, ownerStr, true);
		if (jvAccepted)
		{
			return jvAccepted;
		}
	}

	return jsonRet;
}

Json::Value TxPrepareBase::prepareFutureHash(const Json::Value& tx_json, Application& app,bool bWs)
{
    Json::Value jsonRet(Json::objectValue);
    
	int opType = tx_json[jss::OpType].asInt();
	if (opType == T_REPORT)
		return jsonRet;

	AccountID accountID = *ripple::parseBase58<AccountID>(tx_json[jss::Account].asString());
    std::string sCurHash;

    if (tx_json.isMember(jss::OriginalAddress) || tx_json.isMember(jss::TxnLgrSeq) ||
        tx_json.isMember(jss::CurTxHash) || tx_json.isMember(jss::FutureTxHash))
    {
        if (!tx_json.isMember(jss::OriginalAddress))
        {
			return RPC::missing_field_error(jss::OriginalAddress);
        }
        else if (tx_json[jss::OriginalAddress].asString().size() <= 0)
        {
			return RPC::invalid_field_error(jss::OriginalAddress);
        }

        if (!tx_json.isMember(jss::TxnLgrSeq))
        {
			return RPC::missing_field_error(jss::TxnLgrSeq);
        }
        else if (tx_json[jss::TxnLgrSeq].asString().size() <= 0)
        {
			return RPC::invalid_field_error(jss::TxnLgrSeq);
        }

        if (!tx_json.isMember(jss::CurTxHash))
        {
			return RPC::missing_field_error(jss::CurTxHash);
        }
        else if (tx_json[jss::CurTxHash].asString().size() <= 0)
        {
			return RPC::invalid_field_error(jss::CurTxHash);
        }
        else
        {
            sCurHash = tx_json[jss::CurTxHash].asString();
        }

        if (!tx_json.isMember(jss::FutureTxHash))
        {
			return RPC::missing_field_error(jss::FutureTxHash);
        }
        else if (tx_json[jss::FutureTxHash].asString().size() <= 0)
        {
			return RPC::invalid_field_error(jss::FutureTxHash);
        }

        bool bRet;
        uint256 hashFuture;
        error_code_i errCode;
        std::tie(bRet, hashFuture, errCode) = app.getLedgerMaster().getUserFutureHash(accountID);

        if (!bRet)
        {
            return rpcError(errCode);
        }
        else
        {
            if (hashFuture.isNonZero() && hashFuture != from_hex_text<uint256>(sCurHash))
            {
				std::string errMsg = "current hash is not the expected one";
				jsonRet[jss::FutureTxHash] = to_string(hashFuture);
				return RPC::make_error(rpcGENERAL, errMsg);
            }
        }        
    }
    return jsonRet;
}

}
