//------------------------------------------------------------------------------
/*
This file is part of rippled: https://github.com/ripple/rippled
Copyright (c) 2012, 2013 Ripple Labs Inc.

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

#include <string>

#include <ripple/beast/unit_test.h>
#include <ripple/basics/StringUtilities.h>
#include <test/jtx/Env.h>
#include <ripple/app/tx/impl/ApplyContext.h>
#include <peersafe/app/misc/Executive.h>

/*
* usage:
-uExecutive
*/

namespace ripple {
	namespace test {
		std::string strHex(const std::string &strBin, bool bIsUpper = false)
		{
			std::string strHex;
			strHex.resize(strBin.size() * 2);
			for (size_t i = 0; i < strBin.size(); i++)
			{
				uint8_t cTemp = strBin[i];
				for (size_t j = 0; j < 2; j++)
				{
					uint8_t cCur = (cTemp & 0x0f);
					if (cCur < 10)
					{
						cCur += '0';
					}
					else
					{
						cCur += ((bIsUpper ? 'A' : 'a') - 10);
					}
					strHex[2 * i + 1 - j] = cCur;
					cTemp >>= 4;
				}
			}

			return strHex;
		}

		using namespace test::jtx;
		class Executive_test : public beast::unit_test::suite {

			void run() {
				initEnv();
				testContractCallContract();
			}

			void initEnv()
			{
				pEnv = std::make_shared<Env>(*this);
				auto const alice = Account("alice");
				auto const bob = Account("bob");
				pEnv->fund(ZXC(10000), alice, bob);

				
			}

			void testContractCallContract() {
				/*
				pragma solidity ^0.4.18;
					contract Deployed {
						uint public a = 1;

						function setA(uint _a) public  {
							a = _a;
						}

						function a() returns (uint){
							return a;
						}
					}

					contract Existing  {

						Deployed dc;

						function Existing(address _t) public {
							dc = Deployed(_t);
						}

						function getA() public view returns (uint result) {
							return dc.a();
						}

						function setA(uint _val) public returns (uint result) {
							dc.setA(_val);
							return _val;
						}

					}
				*/

				//Deployed
				std::string codeDeployed = "6080604052600160005534801561001557600080fd5b5060df806100246000396000f3006080604052600436106049576000357c0100000000000000000000000000000000000000000000000000000000900463ffffffff1680630dbe671f14604e578063ee919d50146076575b600080fd5b348015605957600080fd5b50606060a0565b6040518082815260200191505060405180910390f35b348015608157600080fd5b50609e6004803603810190808035906020019092919050505060a9565b005b60008054905090565b80600081905550505600a165627a7a723058200946467e7d8dd67ceb26ca7afe7d5c275e9053ac7c40bc5275e9969a90a20fcd0029";
				std::string codeExisting = "608060405234801561001057600080fd5b506040516020806102e583398101806040528101908080519060200190929190505050806000806101000a81548173ffffffffffffffffffffffffffffffffffffffff021916908373ffffffffffffffffffffffffffffffffffffffff16021790555050610262806100836000396000f30060806040526004361061004c576000357c0100000000000000000000000000000000000000000000000000000000900463ffffffff168063d46300fd14610051578063ee919d501461007c575b600080fd5b34801561005d57600080fd5b506100666100bd565b6040518082815260200191505060405180910390f35b34801561008857600080fd5b506100a760048036038101908080359060200190929190505050610184565b6040518082815260200191505060405180910390f35b60008060009054906101000a900473ffffffffffffffffffffffffffffffffffffffff1673ffffffffffffffffffffffffffffffffffffffff16630dbe671f6040518163ffffffff167c0100000000000000000000000000000000000000000000000000000000028152600401602060405180830381600087803b15801561014457600080fd5b505af1158015610158573d6000803e3d6000fd5b505050506040513d602081101561016e57600080fd5b8101908080519060200190929190505050905090565b60008060009054906101000a900473ffffffffffffffffffffffffffffffffffffffff1673ffffffffffffffffffffffffffffffffffffffff1663ee919d50836040518263ffffffff167c010000000000000000000000000000000000000000000000000000000002815260040180828152602001915050600060405180830381600087803b15801561021657600080fd5b505af115801561022a573d6000803e3d6000fd5b505050508190509190505600a165627a7a723058202f60237e34cc6e98c160d63379b017083534075b0563980e062c2f7f24e398220029000000000000000000000000";
				std::string callSetA = "ee919d50000000000000000000000000000000000000000000000000000000000000007b";
				std::string callGetA = "d46300fd";
				auto blobDeployed = strUnHex(codeDeployed);
				auto res = executeCreate(alice, *blobDeployed);
				if (!res.second)
				{
					return;
				}
				//create existing from deployed
				std::string addressStr(res.first.begin(), res.first.end());
				std::string addressHex = strHex(addressStr, false);
				codeExisting += addressHex;
				auto blobExisting = strUnHex(codeDeployed);
				res = executeCreate(alice, *blobExisting);
				if (!res.second)
				{
					return;
				}

				std::string addressExist(res.first.begin(), res.first.end());

			}

			std::pair<AccountID, bool> executeCreate(Account sender, Blob data)
			{
				auto& ctx = getApplyContext();
				SleOps ops(ctx);
				auto pInfo = std::make_shared<EnvInfoImpl>(1, 210000,1000,0,0, ctx.app.getPreContractFace());
				Executive e(ops, *pInfo, 1);
				uint256 value = uint256(10000000);
				uint256 gasPrice = uint256(10);
				int64_t gas = 3000000;

				if (!e.create(sender, value, gasPrice, gas, &data, sender))
				{
					e.go();
					if (e.getException() != tesSUCCESS)
					{
						eth::bytes out = e.takeOutput().toBytes();
						std::cout << "exception:" << std::string(out.begin(), out.end()) << std::endl;
						return std::make_pair(beast::zero, false);
					}
					else
					{
						std::cout << "contract address " << to_string(e.newAddress()) << std::endl;
						return std::make_pair(e.newAddress(), true);;
					}
				}
				return std::make_pair(beast::zero, false);
			}

			bool executeCall(Account sender, AccountID &contract, Blob data)
			{
				auto& ctx = getApplyContext();
				SleOps ops(ctx);
				auto pInfo = std::make_shared<EnvInfoImpl>(pCtx_->view().info().seq, 210000,1000,0,0, ctx.app.getPreContractFace());
				Executive e(ops, *pInfo, 1);
				uint256 value = uint256(0);
				uint256 gasPrice = uint256(10);
				int64_t gas = 3000000;
				//(AccountID const& _receiveAddress, AccountID const& _senderAddress,
				//uint256 const& _value, uint256 const& _gasPrice, BlobRef _data, int64_t const& _gas)
				if (!e.call(contract, sender, value, gasPrice, &data, gas))
				{
					e.go();
					if (e.getException() != tesSUCCESS)
					{
						eth::bytes out = e.takeOutput().toBytes();
						std::cout << "exception:" << std::string(out.begin(), out.end()) << std::endl;
						return false;
					}
					else
					{
						std::cout << "contract address " << to_string(e.newAddress()) << std::endl;
						return true;
					}
				}
				return false;
			}

			ApplyContext& getApplyContext() {
				STTx tx(ttPAYMENT,
					[](auto& obj)
				{
				});
				auto &env = *pEnv;
				auto openview = const_cast<OpenView&>(*env.current());
				pCtx_ = std::make_shared<ApplyContext>(env.app(), openview, tx,
					tesSUCCESS, 10, tapNO_CHECK_SIGN, env.journal);
				return *pCtx_;
			}
		private:
			std::shared_ptr<Env> pEnv;
			std::shared_ptr<ApplyContext> pCtx_;
			Account alice;
			Account bob;
		};

		BEAST_DEFINE_TESTSUITE_MANUAL(Executive, evm, ripple);
	}
}