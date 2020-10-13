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

#include <boost/format.hpp>

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
	, public ripple::hotstuff::StateCompute
	, public ripple::hotstuff::NetWork {
public:
	static std::map<ripple::hotstuff::Round, Replica*> replicas;

	using KeyPair = std::pair<ripple::PublicKey, ripple::SecretKey>;

	Replica(
		const ripple::hotstuff::Author& author, 
		jtx::Env* env,
		const beast::Journal& journal)
	: io_service_()
	, author_(author)
	, key_pair_(ripple::randomKeyPair(ripple::KeyType::secp256k1))
	, env_(env)
	, hotstuff_(io_service_, journal, author_, this, this, this, this, this) {
		Replica::replicas[Replica::index++] = this;
	}

	~Replica() {
		Replica::index = 1;
	}

	// for StateCompute
	bool compute(const ripple::hotstuff::Block& block, ripple::LedgerInfo& ledger_info) {
		return true;
	}

	bool verify(const ripple::LedgerInfo& ledger_info, const ripple::LedgerInfo& parent_ledger_info) {
		return true;
	}

	int commit(const ripple::hotstuff::Block& block) {
		return 0;
	}

	// for CommandManager
	void extract(ripple::hotstuff::Command& cmd) {
		cmd.clear();

		for (std::size_t i = 0; i < 100; i++) {
			cmd.push_back(generate_random_string(32));
		}
	}

	// for ProposerElection
	ripple::hotstuff::Author GetValidProposer(ripple::hotstuff::Round round) const {
		std::size_t replicas = Replica::replicas.size();
		ripple::hotstuff::Round logic_round = round - ((round - 1) / replicas) * replicas;
		return Replica::replicas[logic_round]->author();
	}

	// for ValidatorVerifier
	const ripple::hotstuff::Author& Self() const {
		return author_;
	}

	bool signature(
		const ripple::Slice& message, 
		ripple::hotstuff::Signature& signature) {
		Replica* replica = nullptr;
		for (auto it = Replica::replicas.begin(); it != Replica::replicas.end(); it++) {
			if (it->second->author() == author_) {
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
		return ripple::verify(
			keyPair.first, 
			message, 
			ripple::Slice((const void*)signature.data(), signature.size()));
	}

	const bool verifyLedgerInfo(
		const ripple::hotstuff::BlockInfo& commit_info,
		const ripple::hotstuff::HashValue& consensus_data_hash,
		const std::map<ripple::hotstuff::Author, ripple::hotstuff::Signature>& signatures) const {
		return true;
	}


	const bool checkVotingPower(const std::map<ripple::hotstuff::Author, ripple::hotstuff::Signature>& signatures) const {
		int size = Replica::replicas.size();
		return signatures.size() >= (size - (size - 1)/3);
	}

	// for Network
	void broadcast(
		const ripple::hotstuff::Block& proposal, 
		const ripple::hotstuff::SyncInfo& sync_info) {
		
		for (auto it = Replica::replicas.begin(); it != Replica::replicas.end(); it++) {
			env_->app().getJobQueue().addJob(
				jtPROPOSAL_t,
				"broadcast_proposal",
				[this, it, proposal, sync_info](Job&) {
					it->second->hotstuff_.handleProposal(proposal, sync_info);
				});
		}
	}

	void sendVote(
		const ripple::hotstuff::Author& author, 
		const ripple::hotstuff::Vote& vote, 
		const ripple::hotstuff::SyncInfo& sync_info) {

		for (auto it = Replica::replicas.begin(); it != Replica::replicas.end(); it++) {
			if (it->second->author() == author) {
				env_->app().getJobQueue().addJob(
					jtPROPOSAL_t,
					"send_vote",
					[this, it, vote, sync_info](Job&) {
						it->second->hotstuff_.handleVote(vote, sync_info);
					});
			}
		}
	}

	void run() {
		hotstuff_.start();
	}

	void stop() {

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
	jtx::Env* env_;
	ripple::hotstuff::Hotstuff hotstuff_;

	static int index;
};
std::map<ripple::hotstuff::Round, Replica*> Replica::replicas;
int Replica::index = 1;

class Hotstuff_test : public beast::unit_test::suite {
public:
	void runReplicas(jtx::Env* env, beast::Journal& journal, int replicas) {
		for (int i = 0; i < replicas; i++) {
			ripple::hotstuff::Author author = (boost::format("%1%") %(i + 1)).str();
			new Replica(author, env, journal);
		}

		for (auto it = Replica::replicas.begin();
			it != Replica::replicas.end(); 
			it++) {
			it->second->run();
		}
	}

	void stopReplicas(jtx::Env* env, int replicas) {

		BEAST_EXPECT(replicas == Replica::replicas.size());

		for (auto it = Replica::replicas.begin();
			it != Replica::replicas.end(); 
			it++) {
			it->second->stop();
		}

		for (;;) {
			if (env->app().getJobQueue().getJobCountTotal(jtPROPOSAL_t) == 0)
				break;
		}

		for (auto it = Replica::replicas.begin();
			it != Replica::replicas.end(); 
			it++) {
			delete it->second;
		}

		Replica::replicas.clear();
	}
	
	void testCase() {
		jtx::Env env{ *this };
		env.app().logs().threshold(beast::severities::kDisabled);

		runReplicas(&env, env.app().journal("testCase"), 4);
		stopReplicas(&env, 4);
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