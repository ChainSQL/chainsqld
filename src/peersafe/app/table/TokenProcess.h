#pragma once

#ifndef TOKEN_PROCESS_H_INCLUDE
#define TOKEN_PROCESS_H_INCLUDE

#include <ripple/basics/Blob.h>
#include <ripple/protocol/SecretKey.h>

namespace ripple {

	class TokenProcess
	{
	private:
		Blob passBlob;
		void* sm4Handle;
		int secretkeyType;
	public:
		bool isValidate;
	public:
		TokenProcess();
		~TokenProcess();
		bool setSymmertryKey(const Blob& cipherBlob, const SecretKey& secret_key);
		Blob symmertryDecrypt(Blob rawEncrept);
	};

}

#endif