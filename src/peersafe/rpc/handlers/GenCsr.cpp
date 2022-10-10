
#include <boost/algorithm/string.hpp>
#include <ripple/app/misc/NetworkOPs.h>
#include <ripple/json/json_value.h>
#include <ripple/net/RPCErr.h>
#include <ripple/protocol/ErrorCodes.h>
#include <ripple/protocol/jss.h>
#include <ripple/rpc/Context.h>
#include <ripple/rpc/impl/TransactionSign.h>
#include <ripple/rpc/Role.h>
#include <peersafe/crypto/X509.h>


namespace ripple {

Json::Value doGenCsr(RPC::JsonContext& context)
{

	Json::Value ret(context.params);
    
    auto params = context.params;
#ifdef HARD_GM
    KeyType keyType = KeyType::gmalg;
#else
    KeyType keyType = CommonKey::chainAlgTypeG;
#endif

    if (params.isMember (jss::key_type))
    {
        if (!params[jss::key_type].isString())
        {
            return RPC::expected_field_error(jss::key_type, "string");
        }

        auto oKeyType = keyTypeFromString(params[jss::key_type].asString());

        if (!oKeyType || *(oKeyType) == KeyType::invalid)
            return rpcError(rpcINVALID_PARAMS);
        keyType = *(oKeyType);
    }

	//x509_subject
    std::string sX509_subject = params[jss::x509_subjects].asString();

	std::vector<std::string> vecX509Subject;
	boost::split(vecX509Subject, sX509_subject, boost::is_any_of(" "), boost::token_compress_on);
	
	if (vecX509Subject.size() != 5){

		std::string errMsg = "must in format: \"country province city organization common\".";
		ret.removeMember(jss::tx_json);
		return RPC::make_error(rpcINVALID_PARAMS, errMsg);
	}

	std::string country      = vecX509Subject[0];
	std::string province     = vecX509Subject[1];
	std::string city         = vecX509Subject[2];
	std::string organization = vecX509Subject[3];
	std::string common       = vecX509Subject[4];

	x509_subject sub = {
		country,
		province,
		city,
		organization,
		common
	};
    
    boost::optional<Seed> seed;
    std::string strExcept;
    std::string seedStr = params[jss::seed].asString();
    bool bOK = genCsr(keyType, seedStr, sub, "x509Req.csr", strExcept);

	if (bOK) {
		ret[jss::status] = "success";
	}
	else {
		ret[jss::info] = "error";
	}

    return ret;
}
} // ripple
