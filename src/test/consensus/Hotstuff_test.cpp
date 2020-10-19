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
	static ripple::LedgerInfo genesis_ledger_info;

	using KeyPair = std::pair<ripple::PublicKey, ripple::SecretKey>;

	Replica(
		const ripple::hotstuff::Config& config,
		jtx::Env* env,
		const beast::Journal& journal,
		bool malicious = false)
	: io_service_()
	, config_(config)
	, recover_data_(ripple::hotstuff::RecoverData{ Replica::genesis_ledger_info })
	, key_pair_(ripple::randomKeyPair(ripple::KeyType::secp256k1))
	, env_(env)
	, committed_blocks_() 
	, hotstuff_(nullptr)
	, malicious_(malicious) {
		
		hotstuff_ = ripple::hotstuff::Hotstuff::Builder(io_service_, journal)
			.setConfig(config_)
			.setCommandManager(this)
			.setNetWork(this)
			.setProposerElection(this)
			.setStateCompute(this)
			.setValidatorVerifier(this)
			.build();

		Replica::replicas[Replica::index++] = this;
	}

	~Replica() {
		Replica::index = 1;
		committed_blocks_.clear();
	}

	// for StateCompute
	bool compute(const ripple::hotstuff::Block& block, ripple::LedgerInfo& ledger_info) {
		return true;
	}

	bool verify(const ripple::LedgerInfo& ledger_info, const ripple::LedgerInfo& parent_ledger_info) {
		return true;
	}

	int commit(const ripple::hotstuff::Block& block) {
		auto it = committed_blocks_.find(block.id());
		if (it == committed_blocks_.end())
			committed_blocks_.emplace(std::make_pair(block.id(), block));
		return 0;
	}

	const std::map<ripple::hotstuff::HashValue, ripple::hotstuff::Block>& committedBlocks() const {
		return committed_blocks_;
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
		return config_.id;
	}

	bool signature(
		const ripple::Slice& message, 
		ripple::hotstuff::Signature& signature) {
		Replica* replica = nullptr;
		for (auto it = Replica::replicas.begin(); it != Replica::replicas.end(); it++) {
			if (it->second->author() == config_.id) {
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

	const bool verifySignature(
		const ripple::hotstuff::Author& author, 
		const ripple::hotstuff::Signature& signature, 
		const ripple::Slice& message) const {

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

		ripple::Slice message(consensus_data_hash.data(), consensus_data_hash.size());
		for (auto it = signatures.begin(); it != signatures.end(); it++) {
			if (verifySignature(it->first, it->second, message) == false)
				return false;
		}
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

		if (malicious())
			return;
		
		for (auto it = Replica::replicas.begin(); it != Replica::replicas.end(); it++) {
			env_->app().getJobQueue().addJob(
				jtPROPOSAL_t,
				"broadcast_proposal",
				[this, it, proposal, sync_info](Job&) {
					it->second->hotstuff_->handleProposal(proposal, sync_info);
				});
		}
	}

	void broadcast(
		const ripple::hotstuff::Vote& vote, 
		const ripple::hotstuff::SyncInfo& sync_info) {

		if (malicious())
			return;

		for (auto it = Replica::replicas.begin(); it != Replica::replicas.end(); it++) {
			env_->app().getJobQueue().addJob(
				jtPROPOSAL_t,
				"broadcast_proposal",
				[this, it, vote, sync_info](Job&) {
					it->second->hotstuff_->handleVote(vote, sync_info);
				});
		}
	}

	void sendVote(
		const ripple::hotstuff::Author& author, 
		const ripple::hotstuff::Vote& vote, 
		const ripple::hotstuff::SyncInfo& sync_info) {

		if (malicious())
			return;

		for (auto it = Replica::replicas.begin(); it != Replica::replicas.end(); it++) {
			if (it->second->author() == author) {
				env_->app().getJobQueue().addJob(
					jtPROPOSAL_t,
					"send_vote",
					[this, it, vote, sync_info](Job&) {
						it->second->hotstuff_->handleVote(vote, sync_info);
					});
			}
		}
	}

	void run() {
		hotstuff_->start(recover_data_);
	}

	void stop() {
		hotstuff_->stop();
	}

	const ripple::hotstuff::Author& author() const {
		return config_.id;
	}
	
	const KeyPair& keyPair() const {
		return key_pair_;
	}

	const bool malicious() const {
		return malicious_;
	}
private:
	boost::asio::io_service io_service_;
	ripple::hotstuff::Config config_;
	ripple::hotstuff::RecoverData recover_data_;
	KeyPair key_pair_;
	jtx::Env* env_;
	std::map<ripple::hotstuff::HashValue, ripple::hotstuff::Block> committed_blocks_;
	ripple::hotstuff::Hotstuff::pointer hotstuff_;
	bool malicious_;

	static int index;
};
std::map<ripple::hotstuff::Round, Replica*> Replica::replicas;
ripple::LedgerInfo Replica::genesis_ledger_info;
int Replica::index = 1;

class Hotstuff_test : public beast::unit_test::suite {
public:

	void parse_args() {
		std::istringstream is(arg());
		std::string token;
		std::vector<std::string> tokens;
		while (std::getline(is, token, ' ')) {
			tokens.push_back(token);
		}

		for (std::size_t i = 0; i < tokens.size(); i++) {
			const std::string& token = tokens[i];
			std::size_t npos = token.find("=");
			if (npos == -1) {
				continue;
			}

			std::string arg_key = token.substr(0, npos);
			std::string arg_value = token.substr(npos + 1);
			if (arg_key.compare("disable_log") == 0) {
				if (arg_value.compare("1") == 0) {
					disable_log_ = true;
				}
			}
			else if (arg_key.compare("false_replicas") == 0) {
				false_replicas_ = std::atoi(arg_value.c_str());
				replicas_ = 3 * false_replicas_ + 1;
			}
			else if (arg_key.compare("blocks") == 0) {
				blocks_ = std::atoi(arg_value.c_str());
			}
			else if (arg_key.compare("timeout") == 0) {
				timeout_ = std::atoi(arg_value.c_str());
			}
		}
	}

	void newAndRunReplicas(
		jtx::Env* env, 
		beast::Journal& journal, 
		int replicas,
		int malicious = 0) {

		// ��������쳣�ڵ�
		std::set<int> maliciousAuthors;
		if (malicious > 0) {
			for (;;) {
				// use current time as seed for random generator
				std::srand(std::time(nullptr));
				int id = (std::rand() % replicas) + 1;
				maliciousAuthors.insert(id);

				if (maliciousAuthors.size() == malicious)
					break;
			}
		}

		ripple::hotstuff::Config config;
		config.timeout = timeout_;
		for (int i = 0; i < replicas; i++) {
			int index = i + 1;
			config.id = (boost::format("%1%") % index).str();
			bool fake_malicious_replica = std::find(
				maliciousAuthors.begin(), 
				maliciousAuthors.end(), 
				index) != maliciousAuthors.end();
			new Replica(config, env, journal, fake_malicious_replica);
		}

		// ���ز��Ե�һ��������ʱ����Ҫ��֤�� Leader �ڵ��� run ����
		// Ȼ�� leader �ڵ��� run��ԭ���� leader �ڵ� run ����������̷���
		// proposal������ʱ�� leader �ڵ���ܻ�û�� run���Ӷ����� assert
		for (auto it = Replica::replicas.begin();
			it != Replica::replicas.end(); 
			it++) {
			if(it->first != 1)
				it->second->run();
		}
		Replica::replicas[1]->run();
	}

	void stopReplicas(jtx::Env* env, int replicas) {
		for (auto it = Replica::replicas.begin();
			it != Replica::replicas.end(); 
			it++) {
			it->second->stop();
		}

		for (;;) {
			if (env->app().getJobQueue().getJobCountTotal(jtPROPOSAL_t) == 0)
				break;
		}
	}

	void releaseReplicas(int replicas) {
		for (auto it = Replica::replicas.begin();
			it != Replica::replicas.end(); 
			it++) {
			delete it->second;
		}
		Replica::replicas.clear();
	}

	bool waitUntilCommittedBlocks(int committedBlocks) {
		while (true) {
			bool satisfied = true;
			for (auto it = Replica::replicas.begin(); it != Replica::replicas.end(); it++) {
				if (it->second->committedBlocks().size() < committedBlocks) {
					satisfied = false;
					continue;
				}
			}

			if (satisfied)
				break;
		}
		return true;
	}

	bool hasConsensusedCommittedBlocks(int committedBlocks) {
		std::vector<ripple::hotstuff::HashValue> summary_hash;

		for (auto it = Replica::replicas.begin(); it != Replica::replicas.end(); it++) {
			const std::map<ripple::hotstuff::HashValue, ripple::hotstuff::Block>& committedBlocksContainer = it->second->committedBlocks();
			using beast::hash_append;
			ripple::sha512_half_hasher h;
			for (int i = 0; i < committedBlocks; i++) {
				for (auto block_it = committedBlocksContainer.begin(); block_it != committedBlocksContainer.end(); block_it++) {
					if (i == block_it->second.block_data().round) {
						hash_append(h, block_it->first);

						if (i == 0) {
							BEAST_EXPECT(block_it->second.block_data().block_type == ripple::hotstuff::BlockData::Genesis);
						}
					}
				}
			}

			summary_hash.push_back(
				static_cast<typename sha512_half_hasher::result_type>(h)
			);
		}

		if (summary_hash.size() != Replica::replicas.size())
			return false;
		ripple::hotstuff::HashValue hash = summary_hash[0];
		for (std::size_t i = 1; i < summary_hash.size(); i++) {
			if (hash != summary_hash[i])
				return false;
		}
		return true;
	}
	
	void testNormalRound() {
		jtx::Env env{ *this };
		if(disable_log_)
			env.app().logs().threshold(beast::severities::kDisabled);

		newAndRunReplicas(&env, env.app().journal("testCase"), replicas_);
		BEAST_EXPECT(waitUntilCommittedBlocks(blocks_) == true);
		stopReplicas(&env, replicas_);
		BEAST_EXPECT(hasConsensusedCommittedBlocks(blocks_) == true);
		releaseReplicas(replicas_);
	}

	void testTimeoutRound() {
		jtx::Env env{ *this };
		if(disable_log_)
			env.app().logs().threshold(beast::severities::kDisabled);

		newAndRunReplicas(&env, env.app().journal("testCase"), replicas_, false_replicas_);
		BEAST_EXPECT(waitUntilCommittedBlocks(blocks_) == true);
	}

    void run() override {
		parse_args();

		testNormalRound();
		//testTimeoutRound();
    }

	Hotstuff_test()
	: false_replicas_(1)
	, replicas_(3* false_replicas_ + 1)
	, blocks_(4)
	, timeout_(60)
	, disable_log_(false) {
		std::string genesis_info = "This is a test for hotstuff";
		using beast::hash_append;
		ripple::sha512_half_hasher h;
		hash_append(h, genesis_info);
		Replica::genesis_ledger_info.txHash = static_cast<typename sha512_half_hasher::result_type>(h);
	}

private:
	int false_replicas_;
	int replicas_;
	int blocks_;
	int timeout_;
	bool disable_log_;
};

BEAST_DEFINE_TESTSUITE(Hotstuff, consensus, ripple);

} // namespace ripple
} // namespace test