
#ifndef RIPPLE_SHAMAP_LEAFNODEHASHCACHE_H_INCLUDED
#define RIPPLE_SHAMAP_LEAFNODEHASHCACHE_H_INCLUDED

#include <ripple/basics/base_uint.h>
#include <set>
#include <string>
#include <mutex>

namespace ripple {

namespace detail {
class LeafNodeHashCache
{
    std::set<uint256> mSet;
    std::recursive_mutex mutable m_mutex;

public:
    void
    insert(uint256 hash)
    {
        std::lock_guard lock(m_mutex);
        mSet.insert(hash);
    }

    bool
    exist(uint256 hash)
    {
        std::lock_guard lock(m_mutex);
        return mSet.find(hash) != mSet.end();
    }

    size_t
    size()
    {
        std::lock_guard lock(m_mutex);
        return mSet.size();
    }

    void 
    clear()
    {
        std::lock_guard lock(m_mutex);
        mSet.clear();
    }
};
}
}  // namespace ripple

#endif