#include <peersafe/app/consensus/ViewChangeManager.h>
#include <ripple/basics/Log.h>

namespace ripple {

ViewChangeManager::ViewChangeManager(beast::Journal const& j):
	j_(j)
{
}

bool ViewChangeManager::recvViewChange(ViewChange const& change)
{
	uint64_t toView = change.toView();
	if (viewChangeReq_.find(toView) != viewChangeReq_.end())
	{
        /**
         * Maybe pre round viewchange doesn't be deleted(new consensus round hasn't begin), 
         * so delete old viewchange here first. otherwise, emplace will failed.
         */
        if (viewChangeReq_[toView].find(change.nodePublic()) != viewChangeReq_[toView].end())
        {
            auto oldViewChange = viewChangeReq_[toView].find(change.nodePublic())->second;
            JLOG(j_.info()) << "peerViewChange toView=" << toView
                << ", pubKey=" << toBase58(TOKEN_NODE_PUBLIC, change.nodePublic()) << " exist, prevSeq=" << oldViewChange.prevSeq()
                << ", and this viewChange preSeq=" << change.prevSeq();
            if (oldViewChange.prevSeq() < change.prevSeq())
            {
                viewChangeReq_[toView].erase(change.nodePublic());
            }
        }

		auto ret = viewChangeReq_[toView].emplace(change.nodePublic(), change);
		if (ret.second)
		{
			JLOG(j_.info()) << "peerViewChange viewChangeReq saved,toView=" <<
				change.toView() << ",size=" << viewChangeReq_[toView].size();
		}
		return ret.second;
	}
	else
	{
		std::map<PublicKey, ViewChange> mapChange;
		mapChange.insert(std::make_pair(change.nodePublic(), change));
		viewChangeReq_.insert(std::make_pair(toView, mapChange));
		return true;
	}
}

bool ViewChangeManager::checkChange(VIEWTYPE const& toView, VIEWTYPE const& curView, RCLCxLedger::ID const& curPrevHash, std::size_t quorum)
{
	if (viewChangeReq_.find(toView) == viewChangeReq_.end())
		return false;

	if (viewChangeReq_[toView].size() >= quorum)
	{
		int count = 0; 
		for (auto item : viewChangeReq_[toView])
		{
			if (item.second.prevHash() == curPrevHash)
			{
				count++;
			}
		}
		if (count >= quorum && toView > curView)
		{
			return true;
		}
	}
	return false;
}

std::tuple<bool, uint32_t, uint256>
ViewChangeManager::shouldTriggerViewChange(VIEWTYPE const& toView, RCLCxLedger const& prevLedger, std::size_t quorum)
{
	if (viewChangeReq_[toView].size() >= quorum)
	{
		auto& mapChange = viewChangeReq_[toView];
		std::map<int, int> mapSeqCount;
		uint32_t prevSeq = 0;
		uint256 prevHash = beast::zero;
		//Check if the prevSeq is consistent between view_change messages.
		for (auto iter = mapChange.begin(); iter != mapChange.end(); iter++)
		{
			int prevSeqTmp = iter->second.prevSeq();
			if (mapSeqCount.find(prevSeqTmp) != mapSeqCount.end())
			{
				mapSeqCount[prevSeqTmp]++;
			}
			else
			{
				mapSeqCount[prevSeqTmp] = 1;
			}

			if (mapSeqCount[prevSeqTmp] >= quorum)
			{
				prevSeq = prevSeqTmp;
				prevHash = iter->second.prevHash();
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

void ViewChangeManager::onViewChanged(VIEWTYPE const& newView)
{
	auto iter = viewChangeReq_.begin();
	while (iter != viewChangeReq_.end())
	{
		if (iter->first < newView)
		{
			iter = viewChangeReq_.erase(iter);
		}
		else
		{
			iter++;
		}
	}
}

void ViewChangeManager::onNewRound(RCLCxLedger const& prevLedger)
{
	for (auto it = viewChangeReq_.begin(); it != viewChangeReq_.end();)
	{
		for (auto cache_entry = it->second.begin(); cache_entry != it->second.end();)
		{
			/// delete expired cache
			if (cache_entry->second.prevSeq() < prevLedger.seq())
				cache_entry = it->second.erase(cache_entry);
			/// in case of faked block hash
			else if (cache_entry->second.prevSeq() == prevLedger.seq() &&
				cache_entry->second.prevHash() != prevLedger.id())
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

void ViewChangeManager::clearCache()
{
	viewChangeReq_.clear();
}

}