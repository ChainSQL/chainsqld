#include <peersafe/app/misc/SleOps.h>
#include <ripple/protocol/digest.h>
#include <ripple/protocol/TxFormats.h>
#include <ripple/app/tx/apply.h>
#include <ripple/ledger/ApplyViewImpl.h>
#include <ripple/app/misc/NetworkOPs.h>
#include <peersafe/app/tx/DirectApply.h>


namespace ripple {
    //just raw function for zxc, all paras should be tranformed in extvmFace modules.

    SleOps::SleOps(ApplyContext& ctx)
        :ctx_(ctx)
        , contractCacheCode_("contractCode", 100, 300, stopwatch(), ctx.app.journal("TaggedCache"))
    {        
    }

    SLE::pointer SleOps::getSle(AccountID const & addr) const
    {        
        ApplyView& view = ctx_.view();
        auto const k = keylet::account(addr);
        return view.peek(k);
    }

	void SleOps::incSequence(AccountID const& addr)
	{
		SLE::pointer pSle = getSle(addr);
		if (pSle)
		{
			uint32 sequence = pSle->getFieldU32(sfSequence);
			{
				pSle->setFieldU32(sfSequence, ++sequence);
				ctx_.view().update(pSle);
			}
		}		
	}

	uint32 SleOps::getSequence(AccountID const& addr)
	{
		SLE::pointer pSle = getSle(addr);
		if (pSle)
			return pSle->getFieldU32(sfSequence);
		else
			return 0;
	}

	bool SleOps::addressHasCode(AccountID const& addr)
	{
		SLE::pointer pSle = getSle(addr);
		if (pSle && pSle->isFieldPresent(sfContractCode))
			return true;
		return false;
	}

	void SleOps::setCode(AccountID const& addr, bytes&& code)
	{
		SLE::pointer pSle = getSle(addr);
		if(pSle)
			pSle->setFieldVL(sfContractCode,code);
	}

	bytes const& SleOps::code(AccountID const& addr) 	
    {
        Blob *pBlobCode = contractCacheCode_.fetch(addr).get();
        if (nullptr == pBlobCode)
        {         
            SLE::pointer pSle = getSle(addr);
			Blob blobCode;
			if (pSle)
				blobCode = pSle->getFieldVL(sfContractCode);
	
            auto p = std::make_shared<ripple::Blob>(blobCode);
            contractCacheCode_.canonicalize(addr, p);

            pBlobCode = p.get();
        }        
        return *pBlobCode;
	}

	uint256 SleOps::codeHash(AccountID const& addr)
	{
        bytes const& code = SleOps::code(addr);
		return sha512Half(code);
	}

	TER SleOps::transferBalance(AccountID const& _from, AccountID const& _to, uint256 const& _value)
	{
		if (_value == uint256(0))
			return tesSUCCESS;

		int64_t value = fromUint256(_value);
		auto ret = subBalance(_from, value);
		if(ret == tesSUCCESS)
			addBalance(_to, value);
		return ret;
	}

	TER SleOps::doPayment(AccountID const& _from, AccountID const& _to, uint256 const& _value)
	{
		int64_t value = fromUint256(_value);
		STTx paymentTx(ttPAYMENT,
			[&_from,&_to,&value](auto& obj)
		{
			obj.setAccountID(sfAccount, _from);
			obj.setAccountID(sfDestination, _to);
			obj.setFieldAmount(sfAmount, ZXCAmount(value));
		});
		auto ret = applyDirect(ctx_.app, ctx_.view(), paymentTx, ctx_.app.journal("Executive"));
		return ret;
	}

	bool SleOps::createContractAccount(AccountID const& _from, AccountID const& _to, uint256 const& _value)
	{
		// Open a ledger for editing.
		auto const k = keylet::account(_to);
		SLE::pointer sleDst = ctx_.view().peek(k);

		if (!sleDst)
		{
			// Create the account.
			sleDst = std::make_shared<SLE>(k);
			sleDst->setAccountID(sfAccount, _to);
			sleDst->setFieldU32(sfSequence, 1);
			ctx_.view().insert(sleDst);
		}

		if (_value != uint256(0))
		{
			transferBalance(_from, _to, _value);
		}

		return true;
	}

	void SleOps::addBalance(AccountID const& addr, int64_t const& amount)
	{
		SLE::pointer pSle = getSle(addr);
		if (pSle)
		{
			auto balance = pSle->getFieldAmount(sfBalance).zxc().drops();
			int64_t finalBanance = balance + amount;
			if (amount > 0)
			{
				pSle->setFieldAmount(sfBalance, ZXCAmount(finalBanance));
				ctx_.view().update(pSle);
			}			
		}
	}

	TER SleOps::subBalance(AccountID const& addr, int64_t const& amount)
	{
		SLE::pointer pSle = getSle(addr);
		if (pSle)
		{
			// This is the total reserve in drops.
			auto const uOwnerCount = pSle->getFieldU32(sfOwnerCount);
			auto const reserve = ctx_.view().fees().accountReserve(uOwnerCount);

			auto balance = pSle->getFieldAmount(sfBalance).zxc().drops();
			int64_t finalBanance = balance - amount;
			//no reserve demand for contract
			if (finalBanance >= reserve || (pSle->isFieldPresent(sfContractCode) && finalBanance >= 0))
			{
				pSle->setFieldAmount(sfBalance, ZXCAmount(finalBanance));
				ctx_.view().update(pSle);
			}
			else
				return tecUNFUNDED_PAYMENT;
		}
		return tesSUCCESS;
	}

	int64_t SleOps::balance(AccountID const& address)
	{
		SLE::pointer pSle = getSle(address);
		if (pSle)
			return pSle->getFieldAmount(sfBalance).zxc().drops();
		else
			return 0;
	}

	void SleOps::clearStorage(AccountID const& _contract)
	{
		SLE::pointer pSle = getSle(_contract);
		if (pSle)
		{
			pSle->makeFieldAbsent(sfContractCode);
			ctx_.view().update(pSle);
		}			
	}

	AccountID SleOps::calcNewAddress(AccountID sender, int nonce)
	{
		bytes data(sender.begin(), sender.end());
		data.push_back(nonce);

		ripesha_hasher rsh;
		rsh(data.data(), data.size());
		auto const d = static_cast<
			ripesha_hasher::result_type>(rsh);
		AccountID id;
		static_assert(sizeof(d) == id.size(), "");
		std::memcpy(id.data(), d.data(), d.size());
		return id;
	}

    void SleOps::PubContractEvents(const AccountID& contractID, uint256 const * aTopic, int iTopicNum, const Blob& byValue)
    {
        ctx_.app.getOPs().PubContractEvents(contractID, aTopic, iTopicNum, byValue);
    }

    void SleOps::kill(AccountID addr)
    {
		SLE::pointer pSle = getSle(addr);
		if (pSle)
			ctx_.view().erase(pSle);
    }
}