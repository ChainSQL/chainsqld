//------------------------------------------------------------------------------
/*
    This file is part of rippled: https://github.com/ripple/rippled
    Copyright (c) 2012-2017 Ripple Labs Inc.

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


#include <peersafe/consensus/pop/ViewChangeManager.h>


namespace ripple {

std::size_t
ViewChangeManager::viewCount(VIEWTYPE toView)
{
    if (viewChangeReq_.find(toView) != viewChangeReq_.end())
    {
        return viewChangeReq_[toView].size();
    }

    return 0;
}

void
ViewChangeManager::onNewRound(RCLCxLedger const& prevLedger)
{
    for (auto it = viewChangeReq_.begin(); it != viewChangeReq_.end();)
    {
        for (auto cache_entry = it->second.begin();
             cache_entry != it->second.end();)
        {
            /// delete expired cache
            if (cache_entry->second->prevSeq() < prevLedger.seq())
                cache_entry = it->second.erase(cache_entry);
            /// in case of faked block hash
            else if (
                cache_entry->second->prevSeq() == prevLedger.seq() &&
                cache_entry->second->prevHash() != prevLedger.id())
                cache_entry = it->second.erase(cache_entry);
            else
                cache_entry++;
        }
        if (it->second.size() == 0)
            it = viewChangeReq_.erase(it);
        else
            it++;
    }
}

bool
ViewChangeManager::recvViewChange(ViewChange const& v)
{
    uint64_t toView = v->toView();
    if (viewChangeReq_.find(toView) != viewChangeReq_.end())
    {
        ///**
        // * Maybe pre round viewchange doesn't be deleted(new consensus round
        // * hasn't begin), so delete old viewchange here first. otherwise,
        // * emplace will failed.
        // */
        /*
        if (viewChangeReq_[toView].find(v->nodePublic()) !=
            viewChangeReq_[toView].end())
        {
            auto oldViewChange =
                viewChangeReq_[toView].find(v->nodePublic())->second;
            JLOG(j_.info())
                << "ViewChangeManager: ViewChange toView=" << toView
                << ", pubKey="
                << toBase58(TokenType::NodePublic, v->nodePublic())
                << " exist. old prevSeq=" << oldViewChange->prevSeq()
                << ", new preSeq=" << v->prevSeq();
            if (oldViewChange->prevSeq() < v->prevSeq())
            {
                viewChangeReq_[toView].erase(v->nodePublic());
            }
        }

        auto result = viewChangeReq_[toView].emplace(v->nodePublic(), v);
        return result.second;
        */
        viewChangeReq_[toView][v->nodePublic()] = v;
        return true;
    }
    else
    {
        std::map<PublicKey, ViewChange> mapChange;
        mapChange.insert(std::make_pair(v->nodePublic(), v));
        viewChangeReq_.insert(std::make_pair(toView, mapChange));
        return true;
    }
}

bool
ViewChangeManager::haveConsensus(
    VIEWTYPE const& toView,
    VIEWTYPE const& curView,
    RCLCxLedger::ID const& preHash,
    std::size_t quorum)
{
    if (viewChangeReq_.find(toView) == viewChangeReq_.end())
        return false;

    if (viewChangeReq_[toView].size() >= quorum)
    {
        int count = 0;
        for (auto item : viewChangeReq_[toView])
        {
            if (item.second->prevHash() == preHash)
            {
                count++;
            }
            else
            {
                JLOG(j_.info())
                    << "Hash not match,prevHash=" << preHash << ","
                    << "item.prevHash=" << item.second->prevHash()
                    << ",item.public_key=" << toBase58(TokenType::NodePublic,item.first);
            }
        }
        if (count >= quorum && toView > curView)
        {
            return true;
        }
    }

    return false;
}

void
ViewChangeManager::onViewChanged(VIEWTYPE const& newView, std::uint32_t preSeq)
{
    for (auto it = viewChangeReq_.begin(); it != viewChangeReq_.end();)
    {
        if (it->first <= newView)
        {
            it = viewChangeReq_.erase(it);
        }
        else
        {
            for (auto it1 = it->second.begin(); it1 != it->second.end();)
            {
                if (it1->second->prevSeq() < preSeq)
                    it1 = it->second.erase(it1);
                else
                    it1++;
            }

            if (it->second.empty())
                it = viewChangeReq_.erase(it);
            else
                it++;
        }
    }
}

std::tuple<bool, uint32_t, uint256>
ViewChangeManager::shouldTriggerViewChange(
    VIEWTYPE const& toView,
    RCLCxLedger const& prevLedger,
    std::size_t quorum)
{
    if (viewChangeReq_[toView].size() >= quorum)
    {
        auto& mapChange = viewChangeReq_[toView];
        std::map<RCLCxLedger::ID, int> mapSeqCount;
        uint32_t prevSeq = 0;
        uint256 prevHash = beast::zero;
        // Check if the prevSeq is consistent between view_change messages.
        for (auto iter = mapChange.begin(); iter != mapChange.end(); iter++)
        {
            auto const& prevHashTmp = iter->second->prevHash();
            if (mapSeqCount.find(prevHashTmp) != mapSeqCount.end())
            {
                mapSeqCount[prevHashTmp]++;
            }
            else
            {
                mapSeqCount[prevHashTmp] = 1;
            }

            if (mapSeqCount[prevHashTmp] >= quorum)
            {
                prevSeq = iter->second->prevSeq();
                prevHash = prevHashTmp;
                break;
            }
        }

        if (prevSeq > 0)
        {
            return std::make_tuple(true, prevSeq, prevHash);
        }
    }
    return std::make_tuple(false, 0, beast::zero);
}

void
ViewChangeManager::clearCache()
{
    viewChangeReq_.clear();
}

//  {
//      "views" : [
//          {
//              "toView" : 100,
//              "count" : 2
//              "view" : [
//                  {
//                      "public_key" : "xxx",
//                      "PreviousSeq" : 1,
//                      "PreviousHash" : "xxxx"
//                  },
//                  {
//                      "public_key" : "xxx",
//                      "PreviousSeq" : 1,
//                      "PreviousHash" : "xxxx"
//                  }
//              ]
//          }, 
//          {
//              "toView" : 101,
//              "count" : 2
//              "view" : [
//                  {
//                      "public_key" : "xxx",
//                      "PreviousSeq" : 1,
//                      "PreviousHash" : "xxxx"
//                  },
//                  {
//                      "public_key" : "xxx",
//                      "PreviousSeq" : 1,
//                      "PreviousHash" : "xxxx"
//                  }
//              ]
//          }
//      ]
//  }
Json::Value
ViewChangeManager::getJson() const
{
    Json::Value ret(Json::objectValue);
    Json::Value& views = (ret["views"] = Json::arrayValue);

    for (auto const& [toView, mapPK2View] : viewChangeReq_)
    {
        Json::Value elem(Json::objectValue);
        elem["toView"] = (Json::UInt)toView;
        elem[jss::count] = (Json::UInt)mapPK2View.size();

        Json::Value& view = (elem[jss::view] = Json::arrayValue);

        for (auto const& [_, v] : mapPK2View)
        {
            (void)_;
            Json::Value const& e = v->getJson(false);
            view.append(e);
        }
        views.append(elem);
    }

    return ret;
}

}  // namespace ripple