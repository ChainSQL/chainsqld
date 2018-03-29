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

#ifndef RIPPLE_BASICS_TAGGEDCACHEEXT_H_INCLUDED
#define RIPPLE_BASICS_TAGGEDCACHEEXT_H_INCLUDED

#include <ripple/basics/TaggedCache.h>

/** Map/cache combination.
This class extends ripple TaggedCache, to achieve the following target:
1.Given an extra target_age_max and a function , when sweep() called,if
the target_age reach and target_age_max have not reached, we will sweep the
target as normal if the target has meet the demand we supply, or the target 
will still stay in the cache until target_age_max reach;
*/
namespace ripple {
template <
	class Key,
	class T,
	class Hash = hardened_hash <>,
	class KeyEqual = std::equal_to <Key>,
	//class Allocator = std::allocator <std::pair <Key const, Entry>>,
	class Mutex = std::recursive_mutex
>
class TaggedCacheExt : public TaggedCache<Key,T>
{
public:
	using mutex_type = Mutex;
	// VFALCO DEPRECATED The caller can just use std::unique_lock <type>
	using ScopedLockType = std::unique_lock <mutex_type>;
	using lock_guard = std::lock_guard <mutex_type>;
	using key_type = Key;
	using mapped_type = T;
	// VFALCO TODO Use std::shared_ptr, std::weak_ptr
	using weak_mapped_ptr = std::weak_ptr <mapped_type>;
	using mapped_ptr = std::shared_ptr <mapped_type>;
	using clock_type = beast::abstract_clock <std::chrono::steady_clock>;
public:
	TaggedCacheExt(std::string const& name, int size,
		clock_type::rep expiration_seconds,clock_type::rep expiration_seconds_max, clock_type& clock, std::function<bool(std::shared_ptr<T>)> func, beast::Journal journal) :
		TaggedCache<Key,T>(name, size, expiration_seconds, clock, journal),
		m_target_age_max(std::chrono::seconds(expiration_seconds_max)),
		m_judge_func(func)
	{
	}
protected:
	virtual bool needContinue(clock_type::time_point const now, typename TaggedCache<Key, T>::Entry& entry)
	{
		clock_type::time_point when_expire_max;
		if (this->m_target_size == 0 ||
			(static_cast<int> (this->m_cache.size()) <= this->m_target_size))
		{
			when_expire_max = now - this->m_target_age_max;
		}
		else
		{
			when_expire_max = now - clock_type::duration(
				m_target_age_max.count() * this->m_target_size / this->m_cache.size());

			clock_type::duration const minimumAge(
				std::chrono::seconds(1));
			if (when_expire_max > (now - minimumAge))
				when_expire_max = now - minimumAge;
		}

		if (entry.last_access > when_expire_max)
		{
			if (!this->m_judge_func(entry.ptr))
			{
				return true;
			}
		}
		return false;
	}

private:
	clock_type::duration m_target_age_max;
	std::function<bool(std::shared_ptr<T>)> m_judge_func;
};
}

#endif
