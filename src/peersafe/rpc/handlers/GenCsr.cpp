#include <BeastConfig.h>
#include <boost/algorithm/string.hpp>
#include <ripple/app/misc/NetworkOPs.h>
#include <ripple/json/json_value.h>
#include <ripple/net/RPCErr.h>
#include <ripple/protocol/ErrorCodes.h>
#include <ripple/protocol/JsonFields.h>
#include <ripple/rpc/Context.h>
#include <ripple/rpc/impl/TransactionSign.h>
#include <ripple/rpc/Role.h>
#include <peersafe/crypto/X509.h>


namespace ripple {

Json::Value doGenCsr(RPC::Context& context)
{

	Json::Value ret(context.params);

	if (ret[jss::tx_json].size() != 2)
	{
		std::string errMsg = "must follow 2 params,in format:\"seed\" \"country province city organization common\".";
		ret.removeMember(jss::tx_json);
		return RPC::make_error(rpcINVALID_PARAMS, errMsg);
	}

	//1. sSeed
	std::string sSeed = ret[jss::tx_json][uint32_t(0)].asString();
	//2.x509_subject
	std::string sX509_subject = ret[jss::tx_json][uint32_t(1)].asString();

	std::vector<std::string> vecX509Subject;
	boost::split(vecX509Subject, sX509_subject, boost::is_any_of(" "), boost::token_compress_on);
	
	if (vecX509Subject.size() != 5){

		std::string errMsg = "must in format: \"country province city organization common\".";
		ret.removeMember(jss::tx_json);
		return RPC::make_error(rpcINVALID_PARAMS, errMsg);
	}

	auto const seed = parseBase58<Seed>(sSeed);
	if (!seed) {
		return  rpcError(rpcBAD_SEED);
	}
		

	std::string country         = vecX509Subject[0];
	std::string province       = vecX509Subject[1];
	std::string city                = vecX509Subject[2];
	std::string organization = vecX509Subject[3];
	std::string common       = vecX509Subject[4];

	x509_subject sub = {
		country,
		province,
		city,
		organization,
		common
	};
	std::string strExcept;
	bool bOK = genCsr(*seed, sub, "x509Req.csr", strExcept);

	if (bOK) {
		ret[jss::status] = "success";
	}
	else {
		ret[jss::info] = "error";
	}

    return ret;
}
} // ripple
