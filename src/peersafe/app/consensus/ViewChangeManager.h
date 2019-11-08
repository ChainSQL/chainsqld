
#ifndef PEERSAFE_APP_CONSENSUS_VIEWCHANGE_MANAGER_H
#define PEERSAFE_APP_CONSENSUS_VIEWCHANGE_MANAGER_H

#include <map>
#include <cstdint>
#include <ripple/basics/base_uint.h>
#include <ripple/protocol/PublicKey.h>
#include <peersafe/app/consensus/ViewChange.h>
#include <ripple/beast/utility/Journal.h>
#include <ripple/app/consensus/RCLCxLedger.h>

namespace ripple {
	class ViewChangeManager {
		using VIEWTYPE = std::uint64_t;
	public:
		ViewChangeManager(beast::Journal const& j);
		/** Receive a view change message.
			Return:
				- true if the first time receiving this msg.
				- false if msg duplicate.
		*/
		bool recvViewChange(ViewChange const& change);

		/** Check if we can trigger view-change.
			Return:
				- true if condition for view-change met.
				- false if condition not met.
		*/
		bool checkChange(VIEWTYPE const& toView, VIEWTYPE const& curView, RCLCxLedger::ID const& curPrevHash ,std::size_t quorum);

		std::tuple<bool,uint32_t,uint256> 
			shouldTriggerViewChange(VIEWTYPE const& toView, RCLCxLedger const& prevLedger,std::size_t quorum);

		// Erase some old view-change cache when view_change happen.
		void onViewChanged(VIEWTYPE const& newView);

		//Erase invalid ViewChange object from the cache on new round started.
		void onNewRound(RCLCxLedger const& ledger);

		void clearCache();
	private:
		std::map<VIEWTYPE, std::map<PublicKey, ViewChange>> viewChangeReq_;
		// Journal for debugging
		beast::Journal j_;
	};
}
#endif