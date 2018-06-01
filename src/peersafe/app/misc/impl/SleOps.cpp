#include <peersafe/app/misc/SleOps.h>
#include <ripple/protocol/digest.h>
#include <ripple/protocol/TxFormats.h>
#include <ripple/app/tx/apply.h>

namespace ripple {
    //just raw function for zxc, all paras should be tranformed in extvmFace modules.

    SleOps::SleOps(ApplyContext& ctx)
        :ctx_(ctx)
        , contractCacheCode_("contractCode", 100, 300, stopwatch(), ctx.app.journal("TaggedCache"))
    {        
    }

    SLE::pointer SleOps::getSle(evmc_address const & addr) const
    {        
        ApplyView& view = ctx_.view();
        
        AccountID accountID = fromEvmC(addr);
        auto const k = keylet::account(accountID);
        return view.peek(k);
    }

	void SleOps::incNonce(evmc_address const& addr)
	{
		SLE::pointer pSle = getSle(addr);
		uint32 nonce = pSle->getFieldU32(sfNonce);
		pSle->setFieldU32(sfNonce,++nonce);
	}

	uint32 SleOps::getNonce(evmc_address const& addr)
	{
		SLE::pointer pSle = getSle(addr);
		if (pSle)
			return pSle->getFieldU32(sfNonce);
		else
			return 0;
	}

	void SleOps::setNonce(evmc_address const& addr, uint32 const& _newNonce)
	{
		SLE::pointer pSle = getSle(addr);
		pSle->setFieldU32(sfNonce, _newNonce);
	}

	bool SleOps::addressHasCode(evmc_address const& addr)
	{
		SLE::pointer pSle = getSle(addr);
		if (pSle && pSle->isFieldPresent(sfContractCode))
			return true;
		return false;
	}

	void SleOps::setCode(evmc_address const& addr, bytes&& code)
	{
		SLE::pointer pSle = getSle(addr);
		pSle->setFieldVL(sfContractCode,code);
	}

	bytes const& SleOps::code(evmc_address const& addr) 	
    {
        Blob *pBlobCode = contractCacheCode_.fetch(fromEvmC(addr)).get();
        if (nullptr == pBlobCode)
        {         
            SLE::pointer pSle = getSle(addr);
            Blob blobCode = pSle->getFieldVL(sfContractCode);

            auto p = std::make_shared<ripple::Blob>(blobCode);
            contractCacheCode_.canonicalize(fromEvmC(addr), p);

            pBlobCode = p.get();
        }        
        return *pBlobCode;
	}

	uint256 SleOps::codeHash(evmc_address const& addr)
	{
        bytes const& code = SleOps::code(addr);
		return sha512Half(code);
	}

	void SleOps::transferBalance(evmc_address const& _from, evmc_address const& _to, evmc_uint256be const& _value)
	{
		int64_t value = fromUint256(_value);
		subBalance(_from, value);
		addBalance(_to, value);
	}

	TER SleOps::activateContract(evmc_address const& _from, evmc_address const& _to, evmc_uint256be const& _value)
	{
		int64_t value = fromUint256(_value);
		STTx paymentTx(ttPAYMENT,
			[&_from,&_to,&value](auto& obj)
		{
			obj.setAccountID(sfAccount, fromEvmC(_from));
			obj.setAccountID(sfDestination, fromEvmC(_to));
			obj.setFieldAmount(sfAmount, ZXCAmount(value));
		});
		ApplyFlags flags = tapNO_CHECK_SIGN;
		auto ret = ripple::apply(ctx_.app, ctx_.view().openView(), paymentTx, ctx_.view().flags(), ctx_.app.journal("Executive"));
		return ret.first;
	}


	void SleOps::addBalance(evmc_address const& addr, int64_t const& amount)
	{
		SLE::pointer pSle = getSle(addr);
		auto balance = pSle->getFieldAmount(sfBalance).zxc().drops();
		int64_t finalBanance = balance + amount;
		
		pSle->setFieldAmount(sfBalance,ZXCAmount(finalBanance));
	}

	void SleOps::subBalance(evmc_address const& addr, int64_t const& amount)
	{
		SLE::pointer pSle = getSle(addr);
		auto balance = pSle->getFieldAmount(sfBalance).zxc().drops();
		int64_t finalBanance = balance - amount;

		pSle->setFieldAmount(sfBalance, ZXCAmount(finalBanance));
	}

	int64_t SleOps::balance(evmc_address address)
	{
		SLE::pointer pSle = getSle(address);
		return pSle->getFieldAmount(sfBalance).zxc().drops();
	}

	void SleOps::clearStorage(evmc_address const& _contract)
	{
		SLE::pointer pSle = getSle(_contract);
		pSle->makeFieldAbsent(sfContractCode);
	}

	evmc_address SleOps::calcNewAddress(evmc_address sender, int nonce)
	{
		bytes data(sender.bytes, sender.bytes + 20);
		data.push_back(nonce);

		ripesha_hasher rsh;
		rsh(data.data(), data.size());
		auto const d = static_cast<
			ripesha_hasher::result_type>(rsh);
		AccountID id;
		static_assert(sizeof(d) == id.size(), "");
		std::memcpy(id.data(), d.data(), d.size());
		return toEvmC(id);
	}
}