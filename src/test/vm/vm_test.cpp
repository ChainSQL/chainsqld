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
#include <thread>

#include <ripple/beast/unit_test.h>
#include <ripple/basics/StringUtilities.h>
#include <boost/algorithm/string.hpp>

#include "FakeExtVM.h"

/*
* usage:
	--unittest="VM" --unittest-arg="code=0x... input=0x...;0x..."
	--unittest="VM" --unittest-arg="file=smartCodes "
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
		std::string args = arg();
		size_t code_npos = args.find("code=");
		size_t file_npos = args.find("file=");
		if (code_npos == 0) {
			init_env();
			createAndCall();
		}

		if (file_npos == 0) {
			muiltCreateCode();
		}

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

	void createAndCall() {
		try {
			bytes code;
			code.assign(code_.begin(), code_.end());
			evmc_address contractAddress = { {1,2,3,4} };
			{
				FakeExecutive execute(code);
				int64_t gas = 30000000;
				execute.create(contractAddress, gas);
			}

			// invoke functions
			{
				std::for_each(datas_.begin(), datas_.end(), [this, &contractAddress](const std::string& input) {
					bytesConstRef data((uint8_t*)input.c_str(), input.size());
					FakeExecutive execute(data, contractAddress);
					int64_t gas = 30000000;
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

	int readSmartCodes(const std::string& codesPath, std::string& codes) {
		int ret = 1;
		do {
			if (boost::filesystem::exists(codesPath) == false)
				break;

			std::ifstream is(codesPath.c_str(), std::ifstream::binary);
			if (is) {
				is.seekg(0, is.end);
				int len = is.tellg();
				is.seekg(0, is.beg);
				codes.resize(len);
				is.read(&codes[0], len);
				is.close();
			}

			ret = 0;
		} while (0);
			
		return ret;
	}

	void muiltCreateCode() {

		std::string args = arg();
		size_t path_npos = args.find("file=");

		std::string mess_codes;
		std::string path;
		if (path_npos != std::string::npos) {
			path = args.substr(path_npos + 5);
			std::cout << "read smart codes in " << path << std::endl;
			readSmartCodes(path, mess_codes);
		}
		
		std::vector<std::string> codes;
		boost::split(codes, mess_codes, boost::is_any_of(";"), boost::token_compress_on);
		std::vector<std::string> runCodes;
		for (size_t i = 0; i < codes.size(); i++) {
			std::string run_code;
			fromHex(codes[i], run_code);
			runCodes.push_back(run_code);
		}

		auto worker = [this](const evmc_address& address, const evmc_uint256be& codeHash, const bytes& code) {
			FakeExecutive execute(code);
			int64_t gas = 300000;
			execute.create(address, codeHash, gas);
		};

		size_t t1_start = 0;
		size_t t1_end = (runCodes.size() / 2) - 1;
		std::thread t1([this,&worker, &runCodes, t1_start, t1_end]() {
			
			uint8_t idx = 1;
			for(size_t i = t1_start; i < t1_end; i++) {
				bytes code;
				code.assign(runCodes[i].begin(), runCodes[i].end());
				evmc_address contractAddress = { {1,2,3,idx++} };
				evmc_uint256be codeHash = { {5,6,7,8,idx++} };
				worker(contractAddress, codeHash, code);

				std::this_thread::sleep_for(std::chrono::milliseconds(20));
			}
		});

		size_t t2_start = runCodes.size()/2;
		size_t t2_end = runCodes.size();
		std::thread t2([this, &worker, &runCodes, t2_start, t2_end]() {
			std::this_thread::sleep_for(std::chrono::milliseconds(20));
			uint8_t idx = 1;
			for(size_t i = t2_start; i < t2_end; i++) {
				bytes code;
				code.assign(runCodes[i].begin(), runCodes[i].end());
				evmc_address contractAddress = { {1,2,++idx,4} };
				evmc_uint256be codeHash = { {5,6,7,8,idx++,9} };
				worker(contractAddress, codeHash, code);
				std::this_thread::sleep_for(std::chrono::milliseconds(20));
			}
		});
		
		t1.join();
		t2.join();
	}

	std::string code_;
	std::vector<std::string> datas_;
};
BEAST_DEFINE_TESTSUITE_MANUAL(VM, evm, ripple);
}
