
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


//----------------------------------------------------------------------------------------

class ViewChange {

public:
    enum GenReason {
        TIMEOUT     = 1,
        EMPTYBLOCK  = 2,
    };

    ViewChange(
        GenReason reason,
		std::uint32_t const prevSeq,
		uint256 const& prevHash,
		PublicKey const nodePublic,
		std::uint64_t const& toView)
        : reason_(reason)
        , prevSeq_(prevSeq)
		, prevHash_(prevHash)
		, nodePublic_(nodePublic)
		, toView_(toView)
    {
        signingHash_ = sha512Half(reason_, prevSeq_, prevHash_, toView_);
    }

	ViewChange(
        GenReason reason,
		std::uint32_t const prevSeq,
		uint256 const& prevHash,
		PublicKey const nodePublic,
		std::uint64_t const& toView,
		Slice signature)
        : reason_(reason)
        , prevSeq_(prevSeq)
		, prevHash_(prevHash)
		, nodePublic_(nodePublic)
		, toView_(toView)
		, signature_(signature)
	{
        signingHash_ = sha512Half(reason_, prevSeq_, prevHash_, toView_);
    }

    GenReason const& genReason() const
    {
        return reason_;
    }

	std::uint32_t const& prevSeq() const
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
        return signingHash_;
	}

    void setSignatrue(const Buffer& sign)
    {
        signature_ = sign;
    }

	bool checkSign() const
	{
		return verifyDigest(nodePublic(), signingHash_, signature_, false);
	}

private:
    GenReason       reason_;
	std::uint32_t   prevSeq_;
	uint256         prevHash_;
	PublicKey       nodePublic_;
	std::uint64_t   toView_;

    uint256         signingHash_;
	Buffer          signature_;
};

//----------------------------------------------------------------------------------------

uint256 viewChangeUniqueId(
    ViewChange::GenReason reason,
	std::uint32_t const prevSeq,
	uint256 const& prevHash,
	PublicKey const nodePublic,
	std::uint64_t const& toView);

//----------------------------------------------------------------------------------------

class CommitteeViewChange
{
private:
    ViewChange::GenReason               mReason;
    uint64                              mView;
    LedgerIndex                         mPreSeq;
    LedgerHash                          mPreHash;
    std::map<PublicKey const, Blob>     mSignatures;

public:
    CommitteeViewChange(protocol::TMCommitteeViewChange const& m);

    inline ViewChange::GenReason genReason() const
    {
        return mReason;
    }

    inline uint64 view() const
    {
        return mView;
    }

    inline LedgerIndex preSeq() const
    {
        return mPreSeq;
    }

    inline LedgerHash const& preHash() const
    {
        return mPreHash;
    }

    inline uint256 suppressionID() const
    {
        return sha512Half(
            mReason,
            mView,
            mPreSeq,
            mPreHash);
    }

    bool checkValidity(std::unique_ptr<ValidatorList> const& list);
};

}

#endif