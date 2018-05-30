#include <peersafe/app/misc/SleOps.h>
#include <ripple/protocol/digest.h>

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
		return pSle->getFieldU32(sfNonce);
	}

	void SleOps::setNonce(evmc_address const& addr, uint32 const& _newNonce)
	{
		SLE::pointer pSle = getSle(addr);
		pSle->setFieldU32(sfNonce, _newNonce);
	}

	bool SleOps::addressHasCode(evmc_address const& addr)
	{
		SLE::pointer pSle = getSle(addr);
		if (pSle->isFieldPresent(sfContractCode))
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

	void SleOps::transferBalance(evmc_address const& _from, evmc_address const& _to, uint256 const& _value)
	{
		subBalance(_from, _value); 
		addBalance(_to, _value);
	}

	void SleOps::addBalance(evmc_address const& addr, uint256 const& amount)
	{
		SLE::pointer pSle = getSle(addr);
		auto balance = pSle->getFieldAmount(sfBalance);

		//STAmount sAmount = STAmount.
		//pSle->setFieldAmount(balance + _amount);
		//(*pSle)[sfBalance] = (*pSle)[sfBalance] + reqDelta;
	}

	void SleOps::subBalance(evmc_address const& _addr, uint256 const& _value)
	{

	}

	void SleOps::clearStorage(evmc_address const& _contract)
	{
		SLE::pointer pSle = getSle(_contract);
		pSle->makeFieldAbsent(sfContractCode);
	}
}