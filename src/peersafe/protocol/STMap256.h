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

#ifndef RIPPLE_PROTOCOL_STMAP256_H_INCLUDED
#define RIPPLE_PROTOCOL_STMAP256_H_INCLUDED

#include <ripple/protocol/STBitString.h>
#include <ripple/protocol/STInteger.h>
#include <ripple/protocol/STBase.h>
#include <ripple/shamap/SHAMap.h>
#include <ripple/protocol/jss.h>


namespace ripple {

class STMap256
	: public STBase
{
public:

	STMap256();

	explicit STMap256(SField const& n)
		: STBase(n)
	{ }

	STMap256(SerialIter& sit, SField const& name);

	STBase* copy(std::size_t n, void* buf) const override
    {
        return emplace(n, buf, *this);
    }

	STBase* move(std::size_t n, void* buf) override
    {
        return emplace(n, buf, std::move(*this));
    }

	SerializedTypeID getSType() const override
    {
        return STI_MAP256;
    }

    bool isEquivalent(const STBase& t) const override
    {
        const STMap256* v = dynamic_cast<const STMap256*>(&t);
        return v && (mValue == v->mValue) && mRootHash == v->mRootHash;
    }

    bool isDefault() const override
    {
        return mValue.empty() && !mRootHash;
    }

    void
    add(Serializer& s) const override
    {
        assert(fName->isBinary());
        assert(fName->fieldType == STI_MAP256);

        Blob blob;
        if (mRootHash)
        {
            blob.insert(blob.end(),mRootHash->begin(),mRootHash->end());
        }
        else
        {
            for (auto iter = mValue.begin(); iter != mValue.end(); iter++)
            {
                blob.insert(
                    blob.end(), iter->first.begin(), iter->first.end());
                blob.insert(
                    blob.end(), iter->second.begin(), iter->second.end());
            }
        }
        s.addVL(blob);
    }

    void
    setValue(const STMap256& v)
    {
        mValue = v.mValue;
        mRootHash = v.mRootHash;
    }

    Json::Value getJson(JsonOptions /*options*/) const override
    {
        Json::Value ret(Json::objectValue);

        if (mRootHash)
            ret[jss::hash] = to_string(*mRootHash);
        else
        {
            for (auto iter = mValue.begin(); iter != mValue.end(); iter++)
            {
                ret[to_string(iter->first)] = to_string(iter->second);
            }
        }

        return ret;
    }

	uint256& operator[](const uint256& key);	

	uint256& at(const uint256& key);

	size_t erase(const uint256& key);

    bool has(const uint256& key);

    void updateRoot(const uint256& rootHash);

    boost::optional<uint256> rootHash();

    boost::optional<uint256> rootHash() const;

private:
	std::map<uint256,uint256>	mValue;
    boost::optional<uint256>    mRootHash;
};

} // ripple

#endif
