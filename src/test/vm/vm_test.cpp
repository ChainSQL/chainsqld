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

#include <string>
#include <vector>

#include <ripple/beast/unit_test.h>
#include <ripple/basics/StringUtilities.h>
#include <boost/algorithm/string.hpp>

#include "FakeExtVM.h"

/*
* usage:
	--unittest="VM" --unittest-arg="code=0x... input=0x...;0x..."
*/

namespace ripple {

size_t fromHex(const std::string& hex, std::string& binary)
{
	std::string pure_hex;
	if (hex[0] == '0' && hex[1] == 'x') {
		pure_hex = hex.substr(2);
	}
	else {
		pure_hex = hex;
	}
	std::pair<Blob, bool> ret = strUnHex(pure_hex);
	if (ret.second == false)
		return 0;
	binary.assign(ret.first.begin(), ret.first.end());
	return binary.size();
}

class VM_test : public beast::unit_test::suite {
public:
	VM_test() {

	}

    void run() {
		init_env();
		//call();
		createAndCall();
		pass();
	}

private:
	void init_env() {
		std::string args = arg();
		size_t code_npos = args.find("code=");
		size_t input_npos = args.find("input=");

		std::string code;
		if (code_npos != std::string::npos) {
			if (input_npos != std::string::npos) {
				code = args.substr(code_npos + 5, input_npos - 6);
			}
			else {
				code = args.substr(code_npos + 5);
			}
			fromHex(code, code_);
		}

		if (input_npos != std::string::npos) {
			std::string data;
			data = args.substr(input_npos + 6);
			std::vector<std::string> datas;
			boost::split(datas, data, boost::is_any_of(";"), boost::token_compress_on);

			std::for_each(datas.begin(), datas.end(), [this](const std::string& data) {
				std::string binary;
				if (fromHex(data, binary) > 0)
					datas_.push_back(binary);
			});
			
		}
	}

	void call() {
		try {
			bytes code;
			code.assign(code_.begin(), code_.end());
			std::for_each(datas_.begin(), datas_.end(), [this, &code](const std::string& input) {
				bytesConstRef data((uint8_t*)input.c_str(), input.size());
				FakeExecutive execute(data, code);
				evmc_address contractAddress = { { 1,2,3,4 } };
				int64_t gas = 300000;
				execute.call(contractAddress, gas);
			});

		}
		catch (const std::exception& e) {
			std::cout << e.what() << std::endl;
		}
		catch (...) {
			std::cout << "unkown exception." << std::endl;
		}

	}

	void createAndCall() {
		try {
			bytes code;
			code.assign(code_.begin(), code_.end());
			evmc_address contractAddress = { {1,2,3,4} };
			{
				FakeExecutive execute(code);
				int64_t gas = 300000;
				execute.create(contractAddress, gas);
			}

			// invoke functions
			{
				std::for_each(datas_.begin(), datas_.end(), [this, &contractAddress](const std::string& input) {
					bytesConstRef data((uint8_t*)input.c_str(), input.size());
					FakeExecutive execute(data, contractAddress);
					int64_t gas = 300000;
					execute.call(contractAddress, gas);
				});
				
			}
		}
		catch (const std::exception& e) {
			std::cout << "exception: " << e.what() << std::endl;
		}
		catch (...) {
			std::cout << "unkown exception." << std::endl;
		}

	}

	std::string code_;
	std::vector<std::string> datas_;
};
BEAST_DEFINE_TESTSUITE_MANUAL(VM, evm, ripple);
}
