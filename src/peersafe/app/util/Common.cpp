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


#include <cctype>
#include <chrono>
#include <peersafe/app/util/Common.h>
#include <peersafe/protocol/STETx.h>

namespace ripple {


uint64_t
utcTime()
{
	auto tp = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::system_clock::now());
	auto tmp = std::chrono::duration_cast<std::chrono::milliseconds>(tp.time_since_epoch());

	return tmp.count();
}

bool
isHexID(std::string const& txid)
{
    if (txid.size() != 64)
        return false;

    auto const ret =
        std::find_if(txid.begin(), txid.end(), [](std::string::value_type c) {
            return !std::isxdigit(static_cast<unsigned char>(c));
        });

    return (ret == txid.end());
}

std::shared_ptr<STTx const>
makeSTTx(Slice sit)
{
    if (*sit.begin() == 0)
    {
        sit.remove_prefix(1);
        return std::make_shared<STETx const>(sit);
    }
    else
        return std::make_shared<STTx const>(sit);
}

std::string
ethAddrChecksum(std::string addr)
{
    std::string addrTemp = (addr.substr(0,2) == "0x")? addr.substr(2) : addr;
    if(addrTemp.length() != 40)
        return std::string("");
    
    transform(addrTemp.begin(),addrTemp.end(),addrTemp.begin(),::tolower);
    
    Blob addrSha3;
    addrSha3.resize(32);
    eth::sha3((const uint8_t*)addrTemp.data(), addrTemp.size(), addrSha3.data());
    std::string addrSha3Str = strHex(addrSha3.begin(), addrSha3.end());
    std::string ret;
    
    for(int i=0; i< addrTemp.length(); i++)
    {
        if(charUnHex(addrSha3Str[i]) >= 8)
        {
            ret += toupper(addrTemp[i]);
        }
        else
        {
            ret += addrTemp[i];
        }
    }
    
    return ret;
}

Json::Value
formatEthError(int code, std::string const& msg)
{
    Json::Value jvResult;
    Json::Value jvError;
    jvError["code"] = code;
    jvError["message"] = msg;
    jvError["data"] = {};
    jvResult["error"] = jvError;

    return jvResult;
}

Json::Value
formatEthError(int code, error_code_i rpcCode)
{
    return formatEthError(code, RPC::get_error_info(rpcCode).message.c_str());
}

}
