#pragma once

#ifndef COMMONKEY_H_INCLUDE
#define COMMONKEY_H_INCLUDE

//#include <gmencrypt/hardencrypt/HardEncryptObj.h>
#include <peersafe/gmencrypt/hardencrypt/HardEncrypt.h>

namespace ripple {
	class CommonKey {
	public:
		//enum keyType { gmInCard, gmOutCard, comKey };
		int keyTypeInt;
		int encrytCardIndex;

	public:
		CommonKey() { keyTypeInt = HardEncrypt::comKey; encrytCardIndex = 0; };
		CommonKey(int keyType_, int index_):keyTypeInt(keyType_), encrytCardIndex(index_){ };
	};
}

#endif