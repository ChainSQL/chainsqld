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
#include <future>
#include <chrono>

#include <boost/format.hpp>

#include <BeastConfig.h>
#include <peersafe/consensus/hotstuff/Hotstuff.h>

// for serialization
#include <peersafe/serialization/Serialization.h>
#include <peersafe/serialization/hotstuff/Block.h>
#include <peersafe/serialization/hotstuff/Vote.h>
#include <peersafe/serialization/hotstuff/SyncInfo.h>

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
	static ripple::hotstuff::Epoch epoch;
	static std::map<ripple::hotstuff::Round, Replica*> replicas;
	static ripple::LedgerInfo genesis_ledger_info;
	static std::string epoch_change;

	using KeyPair = std::pair<ripple::PublicKey, ripple::SecretKey>;

	Replica(
		const ripple::hotstuff::Config& config,
		jtx::Env* env,
		const beast::Journal& journal,
		bool malicious = false)
	: io_service_()
	, worker_()
	, config_(config)
	, key_pair_(ripple::randomKeyPair(ripple::KeyType::secp256k1))
	, env_(env)
	, committed_blocks_() 
	, hotstuff_(nullptr)
	, malicious_(malicious)
	, epoch_change_hash_()
	, committing_epoch_change_(false)
	, changed_epoch_successed_() {

		std::string epoch_change = "EPOCHCHANGE";
		using beast::hash_append;
		ripple::sha512_half_hasher h;
		hash_append(h, epoch_change);
		epoch_change_hash_ = static_cast<typename sha512_half_hasher::result_type>(h);

		hotstuff_ = ripple::hotstuff::Hotstuff::Builder(io_service_, journal)
			.setConfig(config_)
			.setCommandManager(this)
			.setNetWork(this)
			.setProposerElection(this)
			.setStateCompute(this)
			//.setValidatorVerifier(this)
			.build();

		Replica::replicas[Replica::index++] = this;
	}

	~Replica() {
		//io_service_.stop();
		//while (true) {
		//	if (io_service_.stopped())
		//		break;
		//}

		//if (worker_.joinable())
		//	worker_.join();
		
		Replica::index = 1;
		committed_blocks_.clear();
	}

	// for StateCompute
	bool compute(
		const ripple::hotstuff::Block& block, 
		ripple::hotstuff::StateComputeResult& state_compute_result) {
		if (block.block_data().block_type == ripple::hotstuff::BlockData::Proposal) {
			auto payload = block.block_data().payload;
			ripple::hotstuff::Command cmd;
			if (payload)
				cmd = payload->cmd;
			bool has_reconfig = false;
			for (std::size_t i = 0; i < cmd.size(); i++) {
				if (cmd == epoch_change_hash_) {
					//std::cout
					//	<< config_.id << ": "
					//	<< "change reconfiguration" 
					//	<< std::endl;
					has_reconfig = true;
					break;
				}
			}

			if (has_reconfig) {
				ripple::hotstuff::EpochState epoch_state;
				epoch_state.epoch = block.block_data().epoch;
				epoch_state.verifier = this;

				state_compute_result.ledger_info = ripple::LedgerInfo();
				state_compute_result.epoch_state = epoch_state;
			}
		}
		return true;
	}

	bool verify(const ripple::hotstuff::Block& block, const ripple::hotstuff::StateComputeResult& state_compute_result) {
		return true;
	}

	int commit(const ripple::hotstuff::Block& block) {
		auto it = committed_blocks_.find(block.id());
		if (it == committed_blocks_.end()) {
			//std::cout
			//	<< config_.id << ": "
			//	<< "epoch " << block.block_data().epoch
			//	<< ", round " << block.block_data().round 
			//	<< ", id "<< block.id()
			//	<< std::endl;
			committed_blocks_.emplace(std::make_pair(block.id(), block));
		}
		return 0;
	}

	//const std::map<ripple::hotstuff::HashValue, ripple::hotstuff::Block>& 
	//committedBlocks() const {
	//	return committed_blocks_;
	//}

	std::size_t committedBlocks(
		const ripple::hotstuff::Epoch& epoch,
		std::map<ripple::hotstuff::HashValue, ripple::hotstuff::Block>& committed_blocks) {

		committed_blocks.clear();
		for (auto it = committed_blocks_.begin(); it != committed_blocks_.end(); it++) {
			if (it->second.block_data().epoch == epoch)
				committed_blocks.insert(*it);
		}
		return committed_blocks.size();
	}

	// for CommandManager
	void extract(ripple::hotstuff::Command& cmd) {

		if (committing_epoch_change_) {
			cmd = epoch_change_hash_;
			committing_epoch_change(false);
			return;
		}

		using beast::hash_append;
		ripple::sha512_half_hasher h;
		hash_append(h, generate_random_string(32));
		cmd = static_cast<typename sha512_half_hasher::result_type>(h);
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
		const ripple::hotstuff::HashValue& message, 
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
		signature = ripple::signDigest(ripple::KeyType::secp256k1, keyPair.second, message);
		return true;
	}

	const bool verifySignature(
		const ripple::hotstuff::Author& author, 
		const ripple::hotstuff::Signature& signature, 
		const ripple::hotstuff::HashValue& message) const {

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
		return ripple::verifyDigest(
			keyPair.first, 
			message, 
			ripple::Slice((const void*)signature.data(), signature.size()));
	}

	const bool verifyLedgerInfo(
		const ripple::hotstuff::BlockInfo& commit_info,
		const ripple::hotstuff::HashValue& consensus_data_hash,
		const std::map<ripple::hotstuff::Author, ripple::hotstuff::Signature>& signatures) const {

		//ripple::Slice message(consensus_data_hash.data(), consensus_data_hash.size());
		for (auto it = signatures.begin(); it != signatures.end(); it++) {
			if (verifySignature(it->first, it->second, consensus_data_hash) == false)
				return false;
		}
		return true;
	}


	const bool checkVotingPower(
		const std::map<ripple::hotstuff::Author, 
		ripple::hotstuff::Signature>& signatures) const {
		int size = Replica::replicas.size();
		return signatures.size() >= (size - (size - 1)/3);
	}

	// for Network
	void broadcast(
		const ripple::hotstuff::Block& block, 
		const ripple::hotstuff::SyncInfo& sync_info) {
		
		if (malicious())
			return;

		ripple::Buffer s_proposal = ripple::serialization::serialize(block);
		ripple::hotstuff::Block proposal = ripple::serialization::deserialize<ripple::hotstuff::Block>(s_proposal);

		ripple::Buffer s_sync_info = ripple::serialization::serialize(sync_info);
		ripple::hotstuff::SyncInfo sync = ripple::serialization::deserialize<ripple::hotstuff::SyncInfo>(s_sync_info);

		for (auto it = Replica::replicas.begin(); it != Replica::replicas.end(); it++) {
			env_->app().getJobQueue().addJob(
				jtPROPOSAL_t,
				"broadcast_proposal",
				[this, it, proposal, sync](Job&) {
					if(it->second->hotstuff_->CheckProposal(proposal, sync))
						it->second->hotstuff_->handleProposal(proposal);
				});
		}
	}

	void broadcast(
		const ripple::hotstuff::Vote& vote, 
		const ripple::hotstuff::SyncInfo& sync_info) {

		if (malicious())
			return;

		ripple::Buffer s_vote = ripple::serialization::serialize(vote);
		ripple::hotstuff::Vote v = ripple::serialization::deserialize<ripple::hotstuff::Vote>(s_vote);

		ripple::Buffer s_sync_info = ripple::serialization::serialize(sync_info);
		ripple::hotstuff::SyncInfo sync = ripple::serialization::deserialize<ripple::hotstuff::SyncInfo>(s_sync_info);

		for (auto it = Replica::replicas.begin(); it != Replica::replicas.end(); it++) {
			env_->app().getJobQueue().addJob(
				jtPROPOSAL_t,
				"broadcast_proposal",
				[this, it, v, sync](Job&) {
					it->second->hotstuff_->handleVote(v, sync);
				});
		}
	}

	void broadcast(const ripple::hotstuff::EpochChange& epoch_change) {
		Replica::epoch = Replica::epoch + 1;

		for (auto it = Replica::replicas.begin(); it != Replica::replicas.end(); it++) {
			env_->app().getJobQueue().addJob(
				jtPROPOSAL_t,
				"broadcast_proposal",
				[this, it](Job&) {
					//std::cout
					//	<< it->second->author() << ": "
					//	<< "changing epoch and next epoch is " << Replica::epoch
					//	<< std::endl;
					
					it->second->stop();
					it->second->setChangedEpochSuccessed(true);
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

	void run(ripple::hotstuff::RecoverData& recover_data) {
		// initial 
		worker_ = std::thread([this]() {
			boost::asio::io_service::work work(io_service_);
			io_service_.run();
		});
		
		recover_data.epoch_state.epoch = Replica::epoch;
		recover_data.epoch_state.verifier = this;
		hotstuff_->start(recover_data);
	}

	void stop() {
		io_service_.stop();
		while (true) {
			if (io_service_.stopped())
				break;
		}

		if (worker_.joinable())
			worker_.join();

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
	
	void committing_epoch_change(bool change) {
		committing_epoch_change_ = change;
	}
	
	void setChangedEpochSuccessed(bool successed) {
		changed_epoch_successed_.set_value(successed);
	}

	int waitChangedEpochSuccessed() {
		std::future<bool> changed_epoch_future = changed_epoch_successed_.get_future();
		std::future_status status;
		do {
			status = changed_epoch_future.wait_for(std::chrono::seconds(1));
		} while (status != std::future_status::ready);

		return 0;
	}
private:
	boost::asio::io_service io_service_;
	std::thread worker_;
	ripple::hotstuff::Config config_;
	KeyPair key_pair_;
	jtx::Env* env_;
	std::map<ripple::hotstuff::HashValue, ripple::hotstuff::Block> committed_blocks_;
	ripple::hotstuff::Hotstuff::pointer hotstuff_;
	bool malicious_;
	ripple::hotstuff::HashValue epoch_change_hash_;
	bool committing_epoch_change_;
	std::promise<bool> changed_epoch_successed_;

	static int index;
};
ripple::hotstuff::Epoch Replica::epoch = 0;
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

	void newReplicas(
		jtx::Env* env,
		beast::Journal& journal, 
		int replicas,
		int malicious = 0) {

		std::size_t base = Replica::replicas.size();

		// 随机设置异常节点
		std::set<std::size_t> maliciousAuthors;
		if (malicious > 0) {
			for (;;) {
				// use current time as seed for random generator
				std::srand(std::time(nullptr));
				std::size_t id = (std::rand() % replicas) + 1;
				maliciousAuthors.insert(id);

				if (maliciousAuthors.size() == malicious)
					break;
			}
		}

		ripple::hotstuff::Config config;
		config.epoch = Replica::epoch;
		config.timeout = timeout_;
		for (std::size_t i = base; i < (replicas + base); i++) {
			std::size_t index = i + 1;
			config.id = (boost::format("%1%") % index).str();
			bool fake_malicious_replica = std::find(
				maliciousAuthors.begin(), 
				maliciousAuthors.end(), 
				index) != maliciousAuthors.end();
			new Replica(config, env, journal, fake_malicious_replica);
		}
	}

	void runReplicas() {
		ripple::hotstuff::EpochState init_epoch_state;
		init_epoch_state.epoch = Replica::epoch;
		init_epoch_state.verifier = nullptr;

		ripple::hotstuff::RecoverData recover_data =
			ripple::hotstuff::RecoverData{ Replica::genesis_ledger_info, init_epoch_state };

		for (auto it = Replica::replicas.begin();
			it != Replica::replicas.end();
			it++) {
			if (it->first != 1)
				it->second->run(recover_data);
		}
		Replica::replicas[1]->run(recover_data);
	}

	void stopReplicas(jtx::Env* env, int /*replicas*/) {
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

	void releaseReplicas(int /*replicas*/) {
		for (auto it = Replica::replicas.begin();
			it != Replica::replicas.end(); 
			it++) {
			delete it->second;
		}
		Replica::replicas.clear();
	}

	void removeOneReplica(int index) {
		if (index <= 0 
			|| index  > Replica::replicas.size())
			return;

		Replica::replicas.erase(index);
	}

	bool waitUntilCommittedBlocks(int committedBlocks) {
		while (true) {
			bool satisfied = true;
			for (auto it = Replica::replicas.begin(); it != Replica::replicas.end(); it++) {
				std::map<ripple::hotstuff::HashValue, ripple::hotstuff::Block> committed_blocks;
				if(it->second->committedBlocks(Replica::epoch, committed_blocks) < committedBlocks) {
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

		committedBlocks += 1;
		for (auto it = Replica::replicas.begin(); it != Replica::replicas.end(); it++) {
			std::map<ripple::hotstuff::HashValue, ripple::hotstuff::Block> committedBlocksContainer;
			it->second->committedBlocks(Replica::epoch, committedBlocksContainer);
			//const std::map<ripple::hotstuff::HashValue, ripple::hotstuff::Block>& committedBlocksContainer = it->second->committedBlocks();
			using beast::hash_append;
			ripple::sha512_half_hasher h;
			for (int i = 1; i < committedBlocks; i++) {
				for (auto block_it = committedBlocksContainer.begin(); block_it != committedBlocksContainer.end(); block_it++) {
					if (i == block_it->second.block_data().round) {
						hash_append(h, block_it->first);
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

	void waitAllChangedEpochSuccessed() {
		for (auto it = Replica::replicas.begin(); it != Replica::replicas.end(); it++) {
			it->second->waitChangedEpochSuccessed();
		}
	}
	
	void testNormalRound() {
		jtx::Env env{ *this };
		if(disable_log_)
			env.app().logs().threshold(beast::severities::kDisabled);

		newReplicas(&env, env.app().journal("testCase"), replicas_);
		runReplicas();
		BEAST_EXPECT(waitUntilCommittedBlocks(blocks_) == true);
		stopReplicas(&env, replicas_);
		BEAST_EXPECT(hasConsensusedCommittedBlocks(blocks_) == true);
		releaseReplicas(replicas_);
	}

	void testTimeoutRound() {
		jtx::Env env{ *this };
		if(disable_log_)
			env.app().logs().threshold(beast::severities::kDisabled);

		newReplicas(&env, env.app().journal("testCase"), replicas_, false_replicas_);
		runReplicas();
		BEAST_EXPECT(waitUntilCommittedBlocks(blocks_) == true);
		stopReplicas(&env, replicas_);
		BEAST_EXPECT(hasConsensusedCommittedBlocks(blocks_) == true);
		releaseReplicas(replicas_);
	}

	void testAddReplicas() {
		// 新增节点的步骤
		// 1. 新节点以新的 epoch 和配置运行
		// 2. 久节点收到 epoch change 事件后停止当前的 hotstuff
		// 3. 久节点使用新配置重新启动 hotstuff 

		jtx::Env env{ *this };
		if(disable_log_)
			env.app().logs().threshold(beast::severities::kDisabled);

		ripple::hotstuff::Epoch current_epoch = Replica::epoch;

		newReplicas(&env, env.app().journal("testCase"), replicas_);
		runReplicas();
		BEAST_EXPECT(waitUntilCommittedBlocks(2) == true);
		BEAST_EXPECT(Replica::epoch == current_epoch);

		Replica::replicas[1]->committing_epoch_change(true);
		waitAllChangedEpochSuccessed();
		BEAST_EXPECT(Replica::epoch == (current_epoch + 1));
		// Add a new replica and run it
		newReplicas(&env, env.app().journal("testCase"), 1);
		// Re-run replicas
		runReplicas();
		waitUntilCommittedBlocks(blocks_);

		stopReplicas(&env, replicas_);
		BEAST_EXPECT(hasConsensusedCommittedBlocks(blocks_) == true);
		releaseReplicas(replicas_);
	}

	void testRemoveReplicas() {
		// 移除节点的步骤
		// 1. 发布 epoch change 事件
		// 2. 收到 epoch change 事件后停止当前的 hotstuff
		// 3. 使用新配置重新启动 hotstuff 

		jtx::Env env{ *this };
		if(disable_log_)
			env.app().logs().threshold(beast::severities::kDisabled);

		ripple::hotstuff::Epoch current_epoch = Replica::epoch;

		newReplicas(&env, env.app().journal("testCase"), replicas_);
		runReplicas();
		BEAST_EXPECT(waitUntilCommittedBlocks(2) == true);
		BEAST_EXPECT(Replica::epoch == current_epoch );

		Replica::replicas[1]->committing_epoch_change(true);
		waitAllChangedEpochSuccessed();
		BEAST_EXPECT(Replica::epoch == (current_epoch + 1));
		// remove a replica
		removeOneReplica(replicas_);
		BEAST_EXPECT(Replica::replicas.size() == (replicas_ - 1));
		// Re-run replicas
		runReplicas();
		waitUntilCommittedBlocks(blocks_);

		stopReplicas(&env, Replica::replicas.size());
		BEAST_EXPECT(hasConsensusedCommittedBlocks(blocks_) == true);
		releaseReplicas(Replica::replicas.size());
	}

    void run() override {
		parse_args();

		testNormalRound();
		testTimeoutRound();
		testAddReplicas();
		testRemoveReplicas();
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