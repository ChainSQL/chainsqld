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


#include <ripple/basics/Log.h>
#include <ripple/basics/StringUtilities.h>
#include <peersafe/protocol/STMap256.h>

namespace ripple {

STMap256::STMap256()
{
    
}

STMap256::STMap256(SerialIter& sit, SField const& name) : STBase(name)
{
    Blob data = sit.getVL();
    if (data.size() == 256 / 8)
        mRootHash = uint256(data);
    else
    {
        auto const count = data.size() / (256 / 8) / 2;

        Blob::iterator begin = data.begin();
        unsigned int uStart = 0;
        for (unsigned int i = 0; i != count; i++)
        {
            unsigned int uKeyEnd = uStart + (256 / 8);
            unsigned int uValueEnd = uStart + (256 / 8) * 2;
            // This next line could be optimized to construct a default
            // uint256 in the map and then copy into it
            mValue.insert(std::make_pair(
                uint256(Blob(begin + uStart, begin + uKeyEnd)),
                uint256(Blob(begin + uKeyEnd, begin + uValueEnd))));
            uStart = uValueEnd;
        }
    }
}

void
STMap256::add(Serializer& s) const
{
    assert(fName->isBinary());
    assert(fName->fieldType == STI_MAP256);

    Blob blob;
    if (mRootHash)
    {
        blob.insert(blob.end(), mRootHash->begin(), mRootHash->end());
    }
    else
    {
        for (auto iter = mValue.begin(); iter != mValue.end(); iter++)
        {
            blob.insert(blob.end(), iter->first.begin(), iter->first.end());
            blob.insert(blob.end(), iter->second.begin(), iter->second.end());
        }
    }
    s.addVL(blob);
}

Json::Value
STMap256::getJson(JsonOptions /*options*/) const
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

uint256&
STMap256::operator[](const uint256& key)
{
    assert(!mRootHash);
    return mValue[key];
}

uint256&
STMap256::at(const uint256& key)
{
    assert(!mRootHash);
    return mValue.at(key);
}

uint256 const&
STMap256::at(const uint256& key) const
{
    assert(!mRootHash);
    return mValue.at(key);
}

size_t
STMap256::erase(const uint256& key)
{
    assert(!mRootHash);
    return mValue.erase(key);
}

bool
STMap256::has(const uint256& key) const
{
    assert(!mRootHash);
    return mValue.find(key) != mValue.end();
}

size_t
STMap256::size() const
{
    assert(!mRootHash);
    return mValue.size();
}

TER
STMap256::forEach(std::function<TER(uint256 const&, uint256 const&)> f) const
{
    assert(!mRootHash);
    for (auto const& [k, v] : mValue)
    {
        if (auto ter = f(k, v); ter != tesSUCCESS)
            return ter;
    }

    return tesSUCCESS;
}

void
STMap256::updateRoot(const uint256& rootHash)
{
    mRootHash = rootHash;
}

boost::optional<uint256> STMap256::rootHash()
{
    return mRootHash;
}

boost::optional<uint256>
STMap256::rootHash() const
{
    return mRootHash;
}

} // ripple
