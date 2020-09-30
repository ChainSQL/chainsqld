//------------------------------------------------------------------------------
/*
    This file is part of rippled: https://github.com/ripple/rippled
    Copyright (c) 2012-2016 Ripple Labs Inc->

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

#include <vector>
#include <set>
#include <map>
#include <thread>
#include <cstdlib>
#include <ctime>
#include <cmath>

#include <BeastConfig.h>
#include <peersafe/consensus/hotstuff/Hotstuff.h>

#include <ripple/protocol/SecretKey.h>
#include <ripple/protocol/PublicKey.h>
#include <ripple/beast/unit_test.h>
#include <ripple/basics/StringUtilities.h>

#include <test/app/SuitLogs.h>
#include <test/jtx/Env.h>

namespace ripple {
namespace test {

std::string generate_random_string(size_t size) {
	static const char alphanum[] =
		"0123456789"
		"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
		"abcdefghijklmnopqrstuvwxyz";
	std::string s;
	s.resize(size);
	for (size_t i = 0; i < size; ++i) {
		s[i] = alphanum[rand() % (sizeof(alphanum) - 1)];
	}

	return s;
}

class Replica 
	: public ripple::hotstuff::CommandManager
	, public ripple::hotstuff::ProposerElection
	, public ripple::hotstuff::ValidatorVerifier
	, public ripple::hotstuff::NetWork {
public:
	static std::map<ripple::hotstuff::Round, Replica*> replicas;

	using KeyPair = std::pair<ripple::PublicKey, ripple::SecretKey>;

	Replica(const ripple::hotstuff::Author& author, const beast::Journal& journal)
	: io_service_()
	, author_(author)
	, key_pair_(ripple::randomKeyPair(ripple::KeyType::secp256k1))
	, block_store_(ripple::hotstuff::QuorumCertificate(), ripple::hotstuff::QuorumCertificate())
	, epoch_state_()
	, round_state_(&io_service_)
	, proposal_generator_(this, &block_store_, author)
	, hotstuff_(journal, &block_store_, &epoch_state_, &round_state_, &proposal_generator_, this, this) {
		epoch_state_.epoch = 0;
		epoch_state_.verifier = this;

		Replica::replicas[Replica::index++] = this;
	}

	~Replica() {
		Replica::index = 1;
	}

	// for CommandManager
	void extract(ripple::hotstuff::Command& cmd) {

	}

	// for ProposerElection
	ripple::hotstuff::Author GetValidProposer(ripple::hotstuff::Round round) const {
		return Replica::replicas[round]->author();
	}

	// for ValidatorVerifier
	bool signature(
		const ripple::hotstuff::Author& author, 
		const ripple::Slice& message, 
		ripple::hotstuff::Signature& signature) {
		Replica* replica = nullptr;
		for (auto it = Replica::replicas.begin(); it != Replica::replicas.end(); it++) {
			if (it->second->author() == author) {
				replica = it->second;
				break;
			}
		}
		if (replica == nullptr)
			return false;

		const KeyPair& keyPair = replica->keyPair();
		signature = ripple::sign(ripple::KeyType::secp256k1, keyPair.second, message);
		return true;
	}

	bool verifySignature(
		const ripple::hotstuff::Author& author, 
		const ripple::hotstuff::Signature& signature, 
		const ripple::Slice& message) {
		return false;
	}

	// for Network
	void broadcast(
		const ripple::hotstuff::Block& proposal, 
		const ripple::hotstuff::SyncInfo& sync_info) {

	}

	void run() {
		hotstuff_.start();
	}

	const ripple::hotstuff::Author& author() const {
		return author_;
	}
	
	const KeyPair& keyPair() const {
		return key_pair_;
	}
private:
	boost::asio::io_service io_service_;
	ripple::hotstuff::Author author_;
	KeyPair key_pair_;
	ripple::hotstuff::BlockStorage block_store_;
	ripple::hotstuff::EpochState epoch_state_;
	ripple::hotstuff::RoundState round_state_;
	ripple::hotstuff::ProposalGenerator proposal_generator_;
	ripple::hotstuff::Hotstuff hotstuff_;

	static int index;
};
std::map<ripple::hotstuff::Round, Replica*> Replica::replicas;
int Replica::index = 1;

class Hotstuff_test : public beast::unit_test::suite {
public:
	void testCase() {
		jtx::Env env{ *this };
		env.app().logs().threshold(beast::severities::kDisabled);

		Replica replica("1", env.app().journal("testCase"));
		replica.run();
	}
    void run() override {
		testCase();
		BEAST_EXPECT(false);
    }

private:
};

BEAST_DEFINE_TESTSUITE(Hotstuff, consensus, ripple);

} // namespace ripple
} // namespace test