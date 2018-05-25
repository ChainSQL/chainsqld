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

#include <ripple/beast/unit_test.h>
#include <peersafe/vm/Executive.h>

#include "FakeExtVM.h"

namespace ripple {

int fromHexChar(char _i)
{
	if (_i >= '0' && _i <= '9')
		return _i - '0';
	if (_i >= 'a' && _i <= 'f')
		return _i - 'a' + 10;
	if (_i >= 'A' && _i <= 'F')
		return _i - 'A' + 10;
	return -1;
}

size_t fromHex(const std::string& hex, std::string& binary)
{
	//size_t offset = 0;
	size_t size = hex.size();
	size_t s = (size >= 2 && hex[0] == '0' && hex[1] == 'x') ? 2 : 0;

	if (size % 2)
	{
		int h = fromHexChar(hex[s++]);
		if (h != -1) {
			//ret.push_back(h);
			//binary[offset++] = h;
			binary.push_back(h);
		}
		else
			return 0;
	}
	for (size_t i = s; i < size; i += 2)
	{
		int h = fromHexChar(hex[i]);
		int l = fromHexChar(hex[i + 1]);
		if (h != -1 && l != -1) {
			//binary[offset++] = (uint8_t)(h * 16 + l);
			binary.push_back(h * 16 + l);
		} 
		else
			return 0;
	}
	return binary.size();
}

class VM_test : public beast::unit_test::suite {
public:
	VM_test() {

	}

	void run() {
		init_env();
		call();
		pass();
	}

private:
	void init_env() {
		std::string args = arg();
		size_t code_npos = args.find("code=");
		size_t input_npos = args.find("input=");

		if (code_npos != std::string::npos) {
			if (input_npos != std::string::npos) {
				code_ = args.substr(code_npos + 5, input_npos - 6);
			}
			else {
				code_ = args.substr(code_npos + 5);
			}
		}

		if (input_npos != std::string::npos) {
			data_ = args.substr(input_npos + 6);
		}
	}

	void call() {
		std::string code_bytes;
		fromHex(code_, code_bytes);

		std::string input_bytes;
		if(data_.size())
			fromHex(data_, input_bytes);
		
		bytes code;
		code.assign(code_bytes.begin(), code_bytes.end());
		if(input_bytes.empty()) {
			FakeExecutive execute(code);
			execute.create();
		}
		else {
			bytesConstRef data((uint8_t*)input_bytes.c_str(), input_bytes.size());
			FakeExecutive execute(data, code);
			execute.call();
		}

	}

	std::string code_;
	std::string data_;
};
BEAST_DEFINE_TESTSUITE_MANUAL(VM, evm, ripple);
}