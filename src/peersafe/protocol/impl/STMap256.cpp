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

#include <BeastConfig.h>
#include <ripple/basics/Log.h>
#include <ripple/basics/StringUtilities.h>
#include <ripple/protocol/JsonFields.h>
#include <peersafe/protocol/STMap256.h>

namespace ripple {

	//STMap256::STMap256(SerialIter& sit, SField const& name)
	//	: STBase(name)
	//{
	//	Blob data = sit.getVL();
	//	auto const count = data.size() / (256 / 8) / 2;

	//	Blob::iterator begin = data.begin();
	//	unsigned int uStart = 0;
	//	for (unsigned int i = 0; i != count; i++)
	//	{
	//		unsigned int uKeyEnd = uStart + (256 / 8);
	//		unsigned int uValueEnd = uStart + (256 / 8) * 2;
	//		// This next line could be optimized to construct a default
	//		// uint256 in the map and then copy into it
	//		mValue.insert(std::make_pair(uint256(Blob(begin + uStart, begin + uKeyEnd)),
	//			uint256(Blob(begin + uKeyEnd, begin + uValueEnd))));
	//		uStart = uValueEnd;
	//	}
	//}

	//void
	//	STMap256::add(Serializer& s) const
	//{
	//	assert(fName->isBinary());
	//	assert(fName->fieldType == STI_MAP256);
	//	for (auto iter = mValue.begin(); iter != mValue.end(); iter++)
	//	{
	//		s.add256(iter->first);
	//		s.add256(iter->second);
	//	}
	//}

	//bool
	//	STMap256::isEquivalent(const STBase& t) const
	//{
	//	const STMap256* v = dynamic_cast<const STMap256*> (&t);
	//	return v && (mValue == v->mValue);
	//}

	//Json::Value
	//	STMap256::getJson(int) const
	//{
	//	Json::Value ret(Json::objectValue);

	//	for (auto iter = mValue.begin(); iter != mValue.end(); iter++)
	//	{
	//		ret[to_string(iter->first)] = to_string(iter->second);
	//	}

	//	return ret;
	//}

} // ripple
