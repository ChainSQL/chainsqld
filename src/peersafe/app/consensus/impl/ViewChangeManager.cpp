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

bool ViewChangeManager::checkChange(VIEWTYPE const& toView, VIEWTYPE const& curView, std::size_t quorum)
{
	if (viewChangeReq_.find(toView) == viewChangeReq_.end())
		return false;

	if (viewChangeReq_[toView].size() >= quorum)
	{
		if (toView > curView)
		{
			return true;
		}
	}
	return false;
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

}