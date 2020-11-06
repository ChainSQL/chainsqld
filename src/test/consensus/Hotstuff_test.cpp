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
#include <future>
#include <algorithm>

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
#include <ripple/basics/Log.h>

#include <test/app/SuitLogs.h>
#include <test/jtx/Env.h>
#include <test/jtx/envconfig.h>

namespace ripple {
namespace test {

class StdOutSuiteSink : public beast::Journal::Sink
{
	std::string partition_;
	beast::unit_test::suite& suite_;

public:
	StdOutSuiteSink(std::string const& partition,
		beast::severities::Severity threshold,
		beast::unit_test::suite& suite)
		: Sink(threshold, false)
		, partition_(partition + " ")
		, suite_(suite)
	{
	}

	// For unit testing, always generate logging text.
	bool active(beast::severities::Severity level) const override
	{
		return true;
	}

	void
		write(beast::severities::Severity level,
			std::string const& text) override
	{
		using namespace beast::severities;
		std::string s;
		switch (level)
		{
		case kTrace:    s = "TRC:"; break;
		case kDebug:    s = "DBG:"; break;
		case kInfo:     s = "INF:"; break;
		case kWarning:  s = "WRN:"; break;
		case kError:    s = "ERR:"; break;
		default:
		case kFatal:    s = "FTL:"; break;
		}

		// Only write the string if the level at least equals the threshold.
		if (level >= threshold())
			std::cout << s << partition_ << text << std::endl;
	}
};



class StdOutSuiteLogs : public Logs
{
	beast::unit_test::suite& suite_;

public:
	explicit
		StdOutSuiteLogs(beast::unit_test::suite& suite)
		: Logs(beast::severities::kError)
		, suite_(suite)
	{
	}

	~StdOutSuiteLogs() override = default;

	std::unique_ptr<beast::Journal::Sink>
		makeSink(std::string const& partition,
			beast::severities::Severity threshold) override
	{
		return std::make_unique<StdOutSuiteSink>(partition, threshold, suite_);
	}
};

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

	struct Config {
		ripple::hotstuff::Round round;
		ripple::hotstuff::Config config;

		Config()
		: round(0)
		, config() {
		}
	};

	Replica(
		const Config& config,
		jtx::Env* env,
		beast::Journal journal,
		bool malicious = false)
	: io_service_()
	, worker_()
	, config_(config)
	, key_pair_(ripple::randomKeyPair(ripple::KeyType::secp256k1))
	, env_(env)
	, ordered_committed_blocks_()
	, committed_blocks_() 
	, hotstuff_(nullptr)
	, malicious_(malicious)
	, epoch_change_hash_()
	, committing_epoch_change_(false)
	, changed_epoch_successed_() {

		ordered_committed_blocks_.reserve(20);
		ordered_committed_blocks_.resize(20);

		std::string epoch_change = "EPOCHCHANGE";
		using beast::hash_append;
		ripple::sha512_half_hasher h;
		hash_append(h, epoch_change);
		epoch_change_hash_ = static_cast<typename sha512_half_hasher::result_type>(h);

		hotstuff_ = ripple::hotstuff::Hotstuff::Builder(io_service_, journal)
			.setConfig(config_.config)
			.setCommandManager(this)
			.setNetWork(this)
			.setProposerElection(this)
			.setStateCompute(this)
			//.setValidatorVerifier(this)
			.build();

		//Replica::replicas[Replica::index++] = this;
		Replica::replicas[config.round] = this;
	}

	~Replica() {
		for (;;) {
			if (env_->app().getJobQueue().getJobCountTotal(jtPROPOSAL_t) == 0)
				break;
		}
		delete env_;
		env_ = nullptr;

		//Replica::index = 1;
		committed_blocks_.clear();
		ordered_committed_blocks_.clear();
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
					//	<< config_.config.id << ": "
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
			ripple::hotstuff::Round round = block.block_data().round;
			std::cout
			//JLOG(debugLog().info())
				<< config_.config.id << ": "
				<< "epoch " << block.block_data().epoch
				<< ", author " << block.block_data().author()
				<< ", round " << round
				<< ", id " << block.id()
				<< std::endl;
			std::size_t capacity = ordered_committed_blocks_.capacity();
			if (capacity < (round + 5)) {
				ordered_committed_blocks_.reserve(2 * capacity);
				ordered_committed_blocks_.resize(2 * capacity);
			}
			ordered_committed_blocks_[round] = block;
			committed_blocks_.emplace(std::make_pair(block.id(), block));

		}
		return 0;
	}

	bool syncState(const ripple::hotstuff::BlockInfo& block_info) {
		for (auto it = committed_blocks_.begin(); it != committed_blocks_.end(); it++) {
		}
		return true;
	}

	bool syncBlock(
		const ripple::hotstuff::HashValue& block_id, 
		const ripple::hotstuff::Author& author,
		ripple::hotstuff::ExecutedBlock& executedBlock) {
		
		std::promise<ripple::hotstuff::ExecutedBlock> execute_block_promise;
		// using job to avoid shared lock
		std::thread sync_block_worker = std::thread([this, &block_id, &execute_block_promise]() {
			ripple::hotstuff::ExecutedBlock executed_block;
			for (auto it = Replica::replicas.begin(); it != Replica::replicas.end(); it++) {
				if (it->second->hotstuff()->unsafetyExpectBlock(block_id, executed_block)) {
					execute_block_promise.set_value(executed_block);
					break;
				}
			}
		});
		sync_block_worker.join();

		std::future<ripple::hotstuff::ExecutedBlock> result = execute_block_promise.get_future();
		std::future_status status = result.wait_for(std::chrono::milliseconds(1500));
		if (status == std::future_status::ready) {
			executedBlock = result.get();
			return true;
		}
		return false;
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

	std::size_t orderedCommittedBlocks(
		const ripple::hotstuff::Epoch& epoch,
		std::vector<ripple::hotstuff::Block>& ordered_committed_blocks) {

		ordered_committed_blocks.clear();
		for (std::size_t i = 1; i < ordered_committed_blocks_.size(); i++) {
			const ripple::hotstuff::Block& block = ordered_committed_blocks_[i];
			if (block.block_data().round != 0 && block.block_data().epoch == epoch)
				ordered_committed_blocks.push_back(block);
		}
		return ordered_committed_blocks.size();
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
		if(Replica::replicas.find(logic_round) != Replica::replicas.end())
			return Replica::replicas[logic_round]->author();

		return ripple::hotstuff::Author();
	}

	// for ValidatorVerifier
	const ripple::hotstuff::Author& Self() const {
		return config_.config.id;
	}

	bool signature(
		const ripple::hotstuff::HashValue& message, 
		ripple::hotstuff::Signature& signature) {
		Replica* replica = nullptr;
		for (auto it = Replica::replicas.begin(); it != Replica::replicas.end(); it++) {
			if (it->second->author() == config_.config.id) {
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
			if (it->second->malicious() == false) {
				it->second->handleProposal(proposal, sync);
			}
		}
	}
	
	void handleProposal(
		const ripple::hotstuff::Block& proposal,
		const ripple::hotstuff::SyncInfo& sync_info) {

		env_->app().getJobQueue().addJob(
			jtPROPOSAL_t,
			"broadcast_proposal",
			[this, proposal, sync_info](Job&) {
				if (hotstuff_->CheckProposal(proposal, sync_info))
					hotstuff_->handleProposal(proposal);
			});
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
			if (it->second->malicious() == false) {
				it->second->handleVote(v, sync);
			}
		}
	}

	void broadcast(const ripple::hotstuff::EpochChange& epoch_change) {
		Replica::epoch = Replica::epoch + 1;

		for (auto it = Replica::replicas.begin(); it != Replica::replicas.end(); it++) {
			it->second->handleEpochChange(epoch_change);
		}
	}
	
	void handleEpochChange(const ripple::hotstuff::EpochChange& epoch_change) {
		//env_->app().getJobQueue().addJob(
		//	jtPROPOSAL_t,
		//	"broadcast_proposal",
		//	[this](Job&) {
		//		//std::cout
		//		//	<< it->second->author() << ": "
		//		//	<< "changing epoch and next epoch is " << Replica::epoch
		//		//	<< std::endl;

		//		stop();
		//		setChangedEpochSuccessed(true);
		//	});
		//std::cout
		//	<< author() << ": "
		//	<< "changing epoch and next epoch is " << Replica::epoch
		//	<< std::endl;
		stop();
		setChangedEpochSuccessed(true);
	}

	void sendVote(
		const ripple::hotstuff::Author& author, 
		const ripple::hotstuff::Vote& vote, 
		const ripple::hotstuff::SyncInfo& sync_info) {

		if (malicious())
			return;

		for (auto it = Replica::replicas.begin(); it != Replica::replicas.end(); it++) {
			if (it->second->author() == author 
				&& it->second->malicious() == false) {
				it->second->handleVote(vote, sync_info);
			}
		}
	}

	void handleVote(
		const ripple::hotstuff::Vote& vote,
		const ripple::hotstuff::SyncInfo& sync_info) {

		env_->app().getJobQueue().addJob(
			jtPROPOSAL_t,
			"send_vote",
			[this, vote, sync_info](Job&) {
				hotstuff_->handleVote(vote, sync_info);
			});
	}

	std::shared_ptr<ripple::hotstuff::Hotstuff>& hotstuff() {
		return hotstuff_;
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
		return config_.config.id;
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
	Config config_;
	KeyPair key_pair_;
	jtx::Env* env_;
	std::vector<ripple::hotstuff::Block> ordered_committed_blocks_;
	std::map<ripple::hotstuff::HashValue, ripple::hotstuff::Block> committed_blocks_;
	ripple::hotstuff::Hotstuff::pointer hotstuff_;
	bool malicious_;
	ripple::hotstuff::HashValue epoch_change_hash_;
	bool committing_epoch_change_;
	std::promise<bool> changed_epoch_successed_;

	//static int index;
};
ripple::hotstuff::Epoch Replica::epoch = 0;
std::map<ripple::hotstuff::Round, Replica*> Replica::replicas;
ripple::LedgerInfo Replica::genesis_ledger_info;
//int Replica::index = 1;

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

	// make a group of malicious strategy
	// As an example, suppose the set S = { 1, 2, 3, ...., n}
	// and we pick r= 2 out of it. 
	std::size_t makeMaliciousStrategy(
		int malicious,
		std::vector<std::set<std::size_t>>& strategies) {

		if (malicious <= 0)
			return strategies.size();

		std::vector<bool> v(replicas_);
		std::fill(v.end() - malicious, v.end(), true);

		do {
			std::set<std::size_t> strategy;
			for (std::size_t i = 0; i < replicas_; ++i) {
				if (v[i]) {
					strategy.insert(i + 1);
				}
			}
			strategies.push_back(strategy);

		} while (std::next_permutation(v.begin(), v.end()));

		return strategies.size();
	}

	void printMaliciousStrategy(const std::set<std::size_t>& strategy) {
		for (auto it = strategy.begin(); it != strategy.end(); it++) {
			std::cout << *it << " ";
		}
		std::cout << std::endl;
	}
	
	Replica* newReplica(
		const std::string& journal_name,
		const ripple::hotstuff::Round& round,
		bool malicious = false) {

		ripple::hotstuff::Config config;
		config.epoch = Replica::epoch;
		config.timeout = timeout_;
		config.disable_nil_block = disable_nil_block_;
		config.id = (boost::format("%1%") % round).str();

		Replica::Config replicaConfig;
		replicaConfig.round = round;
		replicaConfig.config = config;

		jtx::Env* env = new jtx::Env(
			*this,
			ripple::test::jtx::envconfig(),
			std::make_unique<StdOutSuiteLogs>(*this));
		if (disable_log_)
			env->app().logs().threshold(beast::severities::kDisabled);
		else
			env->app().logs().threshold(beast::severities::kInfo);

		return new Replica(
			replicaConfig,
			env,
			env->app().logs().journal(journal_name),
			malicious);
	}

	std::vector<Replica*> newReplicas(
		const std::string& journal_name, 
		int replicas,
		std::set<std::size_t> maliciousAuthors = std::set<std::size_t>()) {

		std::vector<Replica*> replicas_vec;
		std::size_t base = Replica::replicas.size();

		for (std::size_t i = base; i < (replicas + base); i++) {
			std::size_t index = i + 1;
			bool fake_malicious_replica = std::find(
				maliciousAuthors.begin(),
				maliciousAuthors.end(),
				index) != maliciousAuthors.end();
			replicas_vec.push_back(newReplica(journal_name, index, fake_malicious_replica));
		}

		return replicas_vec;
	}

	void runReplica(const ripple::hotstuff::Round& round) {
		ripple::hotstuff::EpochState init_epoch_state;
		init_epoch_state.epoch = Replica::epoch;
		init_epoch_state.verifier = nullptr;

		ripple::hotstuff::RecoverData recover_data =
			ripple::hotstuff::RecoverData{ Replica::genesis_ledger_info, init_epoch_state };

		Replica::replicas[round]->run(recover_data);
	}

	void runReplicas() {
		for (auto it = Replica::replicas.begin();
			it != Replica::replicas.end();
			it++) {
			if (it->first != 1)
				runReplica(it->first);
		}
		runReplica(1);
	}

	void stopReplicas(int /*replicas*/) {
		for (auto it = Replica::replicas.begin();
			it != Replica::replicas.end(); 
			it++) {
			it->second->stop();
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

	bool waitUntilCommittedBlocks(
		int committedBlocks, 
		std::set<std::size_t> malicious_replicas = std::set<std::size_t>()) {

		while (true) {
			bool satisfied = true;
			for (auto it = Replica::replicas.begin(); it != Replica::replicas.end(); it++) {
				bool fake_malicious_replica = std::find(
					malicious_replicas.begin(),
					malicious_replicas.end(),
					it->first) != malicious_replicas.end();

				std::map<ripple::hotstuff::HashValue, ripple::hotstuff::Block> committed_blocks;
				if(fake_malicious_replica == false
					&& it->second->committedBlocks(Replica::epoch, committed_blocks) < committedBlocks) {
					satisfied = false;
					continue;
				}
			}

			if (satisfied)
				break;
		}
		return true;
	}

	bool hasConsensusedCommittedBlocks(
		int committedBlocks, 
		std::set<std::size_t> malicious_replicas = std::set<std::size_t>()) {

		std::vector<ripple::hotstuff::HashValue> summary_hash;

		committedBlocks += 1;
		for (auto it = Replica::replicas.begin(); it != Replica::replicas.end(); it++) {

			bool fake_malicious_replica = std::find(
				malicious_replicas.begin(),
				malicious_replicas.end(),
				it->first) != malicious_replicas.end();

			if (fake_malicious_replica)
				continue;

			std::map<ripple::hotstuff::HashValue, ripple::hotstuff::Block> committedBlocksContainer;
			it->second->committedBlocks(Replica::epoch, committedBlocksContainer);
			//const std::map<ripple::hotstuff::HashValue, ripple::hotstuff::Block>& committedBlocksContainer = it->second->committedBlocks();
			using beast::hash_append;
			ripple::sha512_half_hasher h;
			for (int i = 1; i < committedBlocks; i++) {
				for (auto block_it = committedBlocksContainer.begin(); block_it != committedBlocksContainer.end(); block_it++) {
					if (i == block_it->second.block_data().round) {
						hash_append(h, block_it->first);
						break;
					}
				}
			}

			summary_hash.push_back(
				static_cast<typename sha512_half_hasher::result_type>(h)
			);
		}

		if (summary_hash.size() != (Replica::replicas.size() - malicious_replicas.size()))
			return false;
		ripple::hotstuff::HashValue hash = summary_hash[0];
		for (std::size_t i = 1; i < summary_hash.size(); i++) {
			if (hash != summary_hash[i])
				return false;
		}
		return true;
	}

	bool hasConsensusedOrderedCommittedBlocks(
		int committedBlocks,
		std::set<std::size_t> malicious_replicas = std::set<std::size_t>()) {

		std::vector<ripple::hotstuff::HashValue> summary_hash;
		std::map<ripple::hotstuff::Author, std::vector<ripple::hotstuff::Block>> committedOrderedBlocksContainer;

		for (auto it = Replica::replicas.begin(); it != Replica::replicas.end(); it++) {

			bool fake_malicious_replica = std::find(
				malicious_replicas.begin(),
				malicious_replicas.end(),
				it->first) != malicious_replicas.end();

			if (fake_malicious_replica)
				continue;

			std::vector<ripple::hotstuff::Block> committedOrderedBlocks;
			it->second->orderedCommittedBlocks(Replica::epoch, committedOrderedBlocks);
			committedOrderedBlocksContainer[it->second->Self()] = committedOrderedBlocks;
		}

		auto it = committedOrderedBlocksContainer.begin();
		assert(it != committedOrderedBlocksContainer.end());
		std::size_t min_size = it->second.size();
		for (; it != committedOrderedBlocksContainer.end(); it++) {
			if (it->second.size() < min_size)
				min_size = it->second.size();
		}

		for (auto it = committedOrderedBlocksContainer.begin(); 
			it != committedOrderedBlocksContainer.end();
			it++) {

			using beast::hash_append;
			ripple::sha512_half_hasher h;

			std::size_t index = it->second.size();
			for (int i = 0; i < min_size; i++) {
				hash_append(h, it->second[--index].id());
			}

			summary_hash.push_back(
				static_cast<typename sha512_half_hasher::result_type>(h));
		}

		if (summary_hash.size() != (Replica::replicas.size() - malicious_replicas.size()))
			return false;
		ripple::hotstuff::HashValue hash = summary_hash[0];
		for (std::size_t i = 1; i < summary_hash.size(); i++) {
			if (hash != summary_hash[i])
				return false;
		}
		return true;
	}

	void waitAllChangedEpochSuccessed(std::set<std::size_t> malicious_replicas = std::set<std::size_t>()) {
		for (auto it = Replica::replicas.begin(); it != Replica::replicas.end(); it++) {

			bool fake_malicious_replica = std::find(
				malicious_replicas.begin(),
				malicious_replicas.end(),
				it->first) != malicious_replicas.end();

			if (fake_malicious_replica)
				continue;

			it->second->waitChangedEpochSuccessed();
		}
	}
	
	void testNormalRound() {
		std::cout << "begin testNormalRound" << std::endl;
		newReplicas("testNormalRound", replicas_);
		runReplicas();
		BEAST_EXPECT(waitUntilCommittedBlocks(blocks_) == true);
		stopReplicas(replicas_);
		BEAST_EXPECT(hasConsensusedCommittedBlocks(blocks_) == true);
		releaseReplicas(replicas_);
		std::cout << "passed on testNormalRound" << std::endl;
	}

	// run all replicas, but excludes a replica take charge FirstRound
	void testRunReplicasExcludeFirstRoundReplica() {
		std::cout << "begin testRunReplicasExcludeFirstRoundReplica" << std::endl;

		newReplica("testRunReplicasExcludeFirstRoundReplica", 2);
		newReplica("testRunReplicasExcludeFirstRoundReplica", 3);
		newReplica("testRunReplicasExcludeFirstRoundReplica", 4);

		runReplica(2);
		runReplica(3);
		runReplica(4);

		BEAST_EXPECT(waitUntilCommittedBlocks(blocks_) == true);
		stopReplicas(replicas_);
		BEAST_EXPECT(hasConsensusedCommittedBlocks(blocks_) == true);

		releaseReplicas(3);
		std::cout << "passed on testRunReplicasExcludeFirstRoundReplica" << std::endl;
	}
	
	// Firstly run other replicas then run first replica after seconds
	void testRunOtherReplicasAndThenRunFirstReplicaAfterSeconds() {
		std::cout << "begin testRunOtherReplicasAndThenRunFirstReplicaAfterSeconds" << std::endl;
		newReplicas("testRunOtherReplicasAndThenRunFirstReplicaAfterSeconds", replicas_);
		for (int i = 2; i <= replicas_; i++) {
			runReplica(i);
		}

		std::thread sleep = std::thread([this]() {
			std::this_thread::sleep_for(std::chrono::seconds(2*(timeout_  + 5)));
			runReplica(1);
		});
		sleep.join();

		BEAST_EXPECT(waitUntilCommittedBlocks(blocks_) == true);
		stopReplicas(replicas_);
		BEAST_EXPECT(hasConsensusedOrderedCommittedBlocks(blocks_) == true);
		releaseReplicas(replicas_);
		std::cout << "passed on testRunOtherReplicasAndThenRunFirstReplicaAfterSeconds" << std::endl;
	}

	// Fistly run first replica then run other replicas after seconds orderly
	void testRunFirstReplicasAndThenRunOtherReplicasAfterSecondsOreder() {
		std::cout << "begin testRunFirstReplicasAndThenRunOtherReplicasAfterSecondsOreder" << std::endl;
		newReplicas("testRunFirstReplicasAndThenRunOtherReplicasAfterSecondsOreder", replicas_);
		runReplica(1);

		std::thread sleep = std::thread([this]() {
			std::this_thread::sleep_for(std::chrono::seconds(2 * (timeout_ + 5)));
				for (int i = 2; i <= replicas_; i++) {
					runReplica(i);
				}
			});
		sleep.join();

		BEAST_EXPECT(waitUntilCommittedBlocks(blocks_) == true);
		stopReplicas(replicas_);
		BEAST_EXPECT(hasConsensusedOrderedCommittedBlocks(blocks_) == true);
		releaseReplicas(replicas_);
		std::cout << "passed on testRunFirstReplicasAndThenRunOtherReplicasAfterSecondsOreder" << std::endl;
	}

	void testTimeoutRound() {
		std::cout << "begin testTimeoutRound" << std::endl;
		std::vector<std::set<std::size_t>> malicious_strategies;
		makeMaliciousStrategy(false_replicas_, malicious_strategies);
		//std::set<std::size_t> strategy;
		//strategy.insert(6);
		//strategy.insert(7);
		//strategy.insert(10);
		//malicious_strategies.push_back(strategy);
		for (std::size_t i = 0; i < malicious_strategies.size(); i++) {
			std::cout << "malicious strategy " << i << ": ";
			printMaliciousStrategy(malicious_strategies[i]);
			newReplicas("testTimeoutRound", replicas_, malicious_strategies[i]);
			runReplicas();
			BEAST_EXPECT(waitUntilCommittedBlocks(blocks_, malicious_strategies[i]) == true);
			stopReplicas(replicas_);
			BEAST_EXPECT(hasConsensusedCommittedBlocks(blocks_, malicious_strategies[i]) == true);
			releaseReplicas(replicas_);
		}
		std::cout << "passed on testTimeoutRound" << std::endl;
	}

	void testAddReplicas() {
		// 新增节点的步骤
		// 1. 新节点以新的 epoch 和配置运行
		// 2. 久节点收到 epoch change 事件后停止当前的 hotstuff
		// 3. 久节点使用新配置重新启动 hotstuff 

		std::cout << "begin testAddReplicas" << std::endl;
		ripple::hotstuff::Epoch current_epoch = Replica::epoch;

		newReplicas("testAddReplicas", replicas_);
		runReplicas();
		BEAST_EXPECT(waitUntilCommittedBlocks(2) == true);
		BEAST_EXPECT(Replica::epoch == current_epoch);

		Replica::replicas[1]->committing_epoch_change(true);
		waitAllChangedEpochSuccessed();
		BEAST_EXPECT(Replica::epoch == (current_epoch + 1));
		// Add a new replica and run it
		newReplicas("testAddReplicas", 1);
		// Re-run replicas
		runReplicas();
		waitUntilCommittedBlocks(blocks_);

		stopReplicas(replicas_);
		BEAST_EXPECT(hasConsensusedCommittedBlocks(blocks_) == true);
		releaseReplicas(replicas_);
		std::cout << "passed on testAddReplicas" << std::endl;
	}

	void testRemoveReplicas() {
		// 移除节点的步骤
		// 1. 发布 epoch change 事件
		// 2. 收到 epoch change 事件后停止当前的 hotstuff
		// 3. 使用新配置重新启动 hotstuff 
		std::cout << "begin testRemoveReplicas" << std::endl;
		ripple::hotstuff::Epoch current_epoch = Replica::epoch;

		newReplicas("testRemoveReplicas", replicas_);
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

		stopReplicas(Replica::replicas.size());
		BEAST_EXPECT(hasConsensusedCommittedBlocks(blocks_) == true);
		releaseReplicas(Replica::replicas.size());
		std::cout << "passed on testRemoveReplicas" << std::endl;
	}

	void testDisableNilBlock() {
		std::cout << "begin testDisableNilBlock" << std::endl;
		// 禁用空块
		disable_nil_block_ = true;

		std::vector<std::set<std::size_t>> malicious_strategies;
		makeMaliciousStrategy(false_replicas_, malicious_strategies);
		for (std::size_t i = 0; i < malicious_strategies.size(); i++) {
			std::cout << "malicious strategy " << i << ": ";
			printMaliciousStrategy(malicious_strategies[i]);

			newReplicas("testDisableNilBlock", replicas_, malicious_strategies[i]);
			runReplicas();
			BEAST_EXPECT(waitUntilCommittedBlocks(blocks_, malicious_strategies[i]) == true);
			stopReplicas(replicas_);
			BEAST_EXPECT(hasConsensusedCommittedBlocks(blocks_, malicious_strategies[i]) == true);
			releaseReplicas(replicas_);
		}
		std::cout << "passed on testDisableNilBlock" << std::endl;
	}

    void run() override {
		parse_args();

		testNormalRound();
		testRunReplicasExcludeFirstRoundReplica();
		testRunOtherReplicasAndThenRunFirstReplicaAfterSeconds();
		testRunFirstReplicasAndThenRunOtherReplicasAfterSecondsOreder();
		testTimeoutRound();
		testAddReplicas();
		testRemoveReplicas();
		testDisableNilBlock();
    }

	Hotstuff_test()
	: false_replicas_(1)
	, replicas_(3* false_replicas_ + 1)
	, blocks_(4)
	, timeout_(60)
	, disable_log_(false)
	, disable_nil_block_(false) {

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
	bool disable_nil_block_;
};

BEAST_DEFINE_TESTSUITE(Hotstuff, consensus, ripple);

} // namespace ripple
} // namespace test