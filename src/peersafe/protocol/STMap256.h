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

namespace ripple {

	class STMap256
		: public STBase
	{
	public:
		using value_type = std::map<uint256,uint256> const&;

		STMap256(){};

		explicit STMap256(SField const& n)
			: STBase(n)
		{ }

		explicit STMap256(std::map<uint256,uint256> const& map)
			: mValue(map)
		{ }

		STMap256(SField const& n, std::map<uint256, uint256> const& map)
			: STBase(n), mValue(map)
		{ }

		STMap256(SerialIter& sit, SField const& name)
        : STBase(name)
        {
            Blob data = sit.getVL();
            auto const count = data.size() / (256 / 8) / 2;

            Blob::iterator begin = data.begin();
            unsigned int uStart = 0;
            for (unsigned int i = 0; i != count; i++)
            {
                unsigned int uKeyEnd = uStart + (256 / 8);
                unsigned int uValueEnd = uStart + (256 / 8) * 2;
                // This next line could be optimized to construct a default
                // uint256 in the map and then copy into it
                mValue.insert(std::make_pair(uint256(Blob(begin + uStart, begin + uKeyEnd)),
                            uint256(Blob(begin + uKeyEnd, begin + uValueEnd))));
                uStart = uValueEnd;
            }
        }


		STBase*
			copy(std::size_t n, void* buf) const override
		{
			return emplace(n, buf, *this);
		}

		STBase*
			move(std::size_t n, void* buf) override
		{
			return emplace(n, buf, std::move(*this));
		}

		SerializedTypeID
			getSType() const override
		{
			return STI_MAP256;
		}

        void
        add(Serializer& s) const
        {
            assert(fName->isBinary());
            assert(fName->fieldType == STI_MAP256);
			Blob blob;
            for (auto iter = mValue.begin(); iter != mValue.end(); iter++)
            {
				blob.insert(blob.end(), iter->first.begin(), iter->first.end());
				blob.insert(blob.end(), iter->second.begin(), iter->second.end());
            }
			s.addVL(blob);
        }

        bool
        isEquivalent(const STBase& t) const
        {
            const STMap256* v = dynamic_cast<const STMap256*> (&t);
            return v && (mValue == v->mValue);
        }

        Json::Value
        getJson(int) const
        {
            Json::Value ret(Json::objectValue);

            for (auto iter = mValue.begin(); iter != mValue.end(); iter++)
            {
                ret[to_string(iter->first)] = to_string(iter->second);
            }

            return ret;
        }

		bool
			isDefault() const override
		{
			return mValue.empty();
		}

		STMap256&
			operator= (std::map<uint256,uint256> const& v)
		{
			mValue = v;
			return *this;
		}

		STMap256&
			operator= (std::map<uint256, uint256>&& v)
		{
			mValue = std::move(v);
			return *this;
		}

		void
			setValue(const STMap256& v)
		{
			mValue = v.mValue;
		}

		/** Retrieve a copy of the vector we contain */
		explicit
			operator std::map<uint256,uint256>() const
		{
			return mValue;
		}

		std::size_t
			size() const
		{
			return mValue.size();
		}

		bool
			empty() const
		{
			return mValue.empty();
		}

		uint256&
			operator[] (const uint256& key)
		{
			return mValue[key];
		}		

		uint256 & at(const uint256& key)
		{
			return mValue.at(key);
		}

		std::map<uint256,uint256> const&
			value() const
		{
			return mValue;
		}

		std::map<uint256, uint256>::iterator
			insert(std::map<uint256, uint256>::const_iterator pos, uint256 const& key,uint256 const& value)
		{
			return mValue.insert(pos, std::make_pair(key,value));
		}

		//std::vector<uint256>::iterator
		//	insert(std::vector<uint256>::const_iterator pos, uint256 const&& key, uint256&& value)
		//{
		//	return mValue.insert(pos, std::make_pair(std::move(key), std::move(value)));
		//}

		void
			insert(uint256 const& key,uint256 const& v)
		{
			mValue.insert(std::make_pair(key,v));
		}

		std::map<uint256,uint256>::iterator
			begin()
		{
			return mValue.begin();
		}

		std::map<uint256, uint256>::const_iterator
			begin() const
		{
			return mValue.begin();
		}

		std::map<uint256, uint256>::iterator
			end()
		{
			return mValue.end();
		}

		std::map<uint256, uint256>::const_iterator
			end() const
		{
			return mValue.end();
		}

		std::map<uint256, uint256>::iterator
			erase(std::map<uint256, uint256>::iterator position)
		{
			return mValue.erase(position);
		}

		size_t erase(const uint256& key)
		{
			return mValue.erase(key);
		}

		void
			clear() noexcept
		{
			return mValue.clear();
		}

	private:
		std::map<uint256,uint256> mValue;
	};

} // ripple

#endif
