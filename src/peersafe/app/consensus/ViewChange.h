
#ifndef PEERSAFE_APP_CONSENSUS_VIEWCHANGE_H
#define PEERSAFE_APP_CONSENSUS_VIEWCHANGE_H

#include <cstdint>
#include "ripple.pb.h"
#include <ripple/basics/base_uint.h>
#include <ripple/protocol/PublicKey.h>
#include <ripple/protocol/Serializer.h>
#include <ripple/protocol/digest.h>
#include <ripple/basics/Buffer.h>
#include <ripple/app/misc/ValidatorList.h>
#include <ripple/protocol/Protocol.h>
#include <ripple/protocol/RippleLedgerHash.h>

namespace ripple {
class ViewChange {
public:
	ViewChange(
		std::uint32_t const prevSeq,
		uint256 const& prevHash,
		PublicKey const nodePublic,
		std::uint64_t const& toView
	):
		prevSeq_(prevSeq),
		prevHash_(prevHash),
		nodePublic_(nodePublic),
		toView_(toView)
	{

	}

	ViewChange(
		std::uint32_t const prevSeq,
		uint256 const& prevHash,
		PublicKey const nodePublic,
		std::uint64_t const& toView,
		Slice signature
	) :
		prevSeq_(prevSeq),
		prevHash_(prevHash),
		nodePublic_(nodePublic),
		toView_(toView),
		signature_(signature)
	{

	}

	std::uint32_t const& prevSeq()const
	{
		return prevSeq_;
	}

	uint256 const& prevHash() const
	{
		return prevHash_;
	}

	PublicKey const& nodePublic() const
	{
		return nodePublic_;
	}

    inline Buffer const& signature() const
    {
        return signature_;
    }

	std::uint64_t const& toView() const
	{
		return toView_;
	}

	uint256 signingHash() const
	{
		return sha512Half(
			prevSeq_,
			prevHash_,
			nodePublic_,
			toView_
		);
	}
	bool checkSign()const
	{
		return verifyDigest(
			nodePublic(), signingHash(), signature_, false);
	}
private:
	std::uint32_t  prevSeq_;
	uint256 prevHash_;
	PublicKey  nodePublic_;
	std::uint64_t toView_;
	Buffer signature_;
};

uint256
viewChangeUniqueId(
	std::uint32_t const prevSeq,
	uint256 const& prevHash,
	PublicKey const nodePublic,
	std::uint64_t const& toView
);

class CommitteeViewChange
{
private:
    uint64                              mView;
    LedgerIndex                         mPreSeq;
    LedgerHash                          mPreHash;
    std::map<PublicKey const, Slice>    mSignatures;

public:
    CommitteeViewChange(protocol::TMCommitteeViewChange const& m);


    inline LedgerIndex preSeq()
    {
        return mPreSeq;
    }

    inline LedgerHash preHash()
    {
        return mPreHash;
    }

    inline uint256 suppressionID()
    {
        return sha512Half(
            mView,
            mPreSeq,
            mPreHash);
    }

    bool checkValidity(std::unique_ptr<ValidatorList> const& list);
};

}

#endif