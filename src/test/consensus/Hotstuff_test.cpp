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

#include <BeastConfig.h>
#include <peersafe/consensus/hotstuff/Pacemaker.h>
#include <peersafe/consensus/hotstuff/impl/Types.h>
#include <peersafe/consensus/hotstuff/impl/Block.h>
#include <peersafe/consensus/hotstuff/impl/Crypto.h>

#include <ripple/protocol/SecretKey.h>
#include <ripple/protocol/PublicKey.h>
#include <ripple/beast/unit_test.h>

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

class Replica : public ripple::hotstuff::Sender
                , public ripple::hotstuff::Storage
                , public ripple::hotstuff::Executor {
public:
    using ReplicaID = ripple::hotstuff::ReplicaID;
    using Block = ripple::hotstuff::Block;
    using BlockHash = ripple::hotstuff::BlockHash;
    using KeyPair = std::pair<ripple::PublicKey, ripple::SecretKey>;

    static std::map<ReplicaID, Replica*> replicas;

    Replica(
        jtx::Env *env,
        const ripple::hotstuff::Config& config)
    : io_service_()
    , worker_()
    , jobQueue_(&env->app().getJobQueue())
    , config_(config)
    , journal_(env->app().journal("testHotstuff"))
    , pacemaker_(&io_service_)
    , hotstuff_(nullptr)
    , key_pair_(ripple::randomKeyPair(ripple::KeyType::secp256k1))
    , malicious_(false)
    , cache_blocks_()
    , consented_blocks_() {
        Replica::replicas[config.id] = this;
    }

    void init() {
        if(hotstuff_ != nullptr)
            return;

        //create worker thread
        worker_ = std::thread([this]() {
            boost::asio::io_service::work work(io_service_);
            io_service_.run();
        });

        hotstuff_ = new ripple::hotstuff::Hotstuff(
            jobQueue_,
            config_, 
            journal_, 
            this, 
            this, 
            this, 
            &pacemaker_);
    }

    virtual ~Replica() {
        io_service_.stop();
        while(true) {
            if(io_service_.stopped())
                break;
        }
        if(worker_.joinable())
            worker_.join();

        delete hotstuff_;
    }

    // for ripple::hotstuff::Sender
    void proposal(
        const ripple::hotstuff::ReplicaID& id, 
        const ripple::hotstuff::Block& block) {
        
        auto it = Replica::replicas.find(id);
        if(it != Replica::replicas.end()) {
            if(it->second->malicious() == false)
                it->second->hotstuff_->handlePropose(block);
        }
    }

    void vote(
        const ripple::hotstuff::ReplicaID& id, 
        const ripple::hotstuff::PartialCert& cert) {

        auto it = Replica::replicas.find(id);
        if(it != Replica::replicas.end()) {
            if(it->second->malicious() == false)
                it->second->hotstuff_->handleVote(cert);
        }
    }

    void newView(const ReplicaID& id, const ripple::hotstuff::QuorumCert& qc) {
        auto it = Replica::replicas.find(id);
        if(it != Replica::replicas.end()) {
            if(it->second->malicious() == false) {
                jobQueue_->addJob(
                    jtPROPOSAL_t,
                    "sendNewView",
                    [this, it, qc](Job&) {
                        it->second->hotstuff_->handleNewView(qc);
                    }
                );
            }
        }
    }

    // for ripple::hotstuff::Storage
    // for transactions
    void command(std::size_t batch_size, ripple::hotstuff::Command& cmd) {
        cmd.clear();

        for(std::size_t i = 0; i < batch_size; i ++) {
            cmd.push_back(generate_random_string(32));
        }
    }

    // for blocks
    bool addBlock(const ripple::hotstuff::Block& block) {

        if(cache_blocks_.find(block.hash) != cache_blocks_.end())
            return false;
        
        cache_blocks_[block.hash] = block;
        return true;
    }

    bool blockOf(
        const ripple::hotstuff::BlockHash& hash, 
        ripple::hotstuff::Block& block) const {
        auto it = cache_blocks_.find(hash);
        if(it == cache_blocks_.end()) {
            return false;
        }
        
        block = it->second;
        return true;
    }

    bool expectBlock(
        const ripple::hotstuff::BlockHash& hash, 
        ripple::hotstuff::Block& block) {
        if(blockOf(hash, block))
            return true;

        // sync block
        for(auto it = Replica::replicas.begin(); it != Replica::replicas.end(); it++) {
            if(it->second->blockOf(hash, block)) {
                addBlock(block);
                return true;
            }
        }
        return false;
    }

    // for ripple::hotstuff::Executor
    bool accept(const ripple::hotstuff::Command& cmd) {
        return true;
    }

    void consented(const ripple::hotstuff::Block& block) {
        std::cout 
            << hotstuff_->id()
            << " consented block -> " << block.height << std::endl;
        consented_blocks_.push_back(block);
    }

    int quorumSize() {
        std::size_t size = config_.leader_schedule.size();
        return static_cast<int>(size - (size - 1)/3);
    }

    bool signature(
        const ripple::hotstuff::ReplicaID& id, 
        const ripple::hotstuff::BlockHash& hash, 
        ripple::hotstuff::PartialSig& partialSig) {

        auto it = Replica::replicas.find(id);
        if(it == Replica::replicas.end()) {
            return false;
        }

        const KeyPair& r = it->second->keyPair();
        partialSig.ID = id;
        partialSig.sig = ripple::sign(ripple::KeyType::secp256k1, 
            r.second,
            ripple::Slice((const void*)hash.data(), hash.size()));

        return true;
    }

    bool verifySignature(
        const ripple::hotstuff::PartialSig& partialSig, 
        const ripple::hotstuff::BlockHash& hash) {

        auto it = Replica::replicas.find(partialSig.ID);
        if(it == Replica::replicas.end()) {
            return false;
        }

        const KeyPair& r = it->second->keyPair();
        return ripple::verify(
            r.first, 
            ripple::Slice((const void*)hash.data(), hash.size()), 
            ripple::Slice((const void*)partialSig.sig.data(), partialSig.sig.size()));
    }

    void run() {
        pacemaker_.run();
    }
    
    void stop() {
        pacemaker_.stop();
    }

    const std::vector<Block>& consentedBlocks() const {
        return consented_blocks_;
    }
    
    const KeyPair& keyPair() const {
        return key_pair_;
    }

    void malicious(bool m) {
        malicious_ = m;
    }

    const bool malicious() const {
        return malicious_;
    }

    std::vector<ReplicaID>& leader_schedule() {
        return config_.leader_schedule;
    } 

private:
    boost::asio::io_service io_service_;
    std::thread worker_;
    ripple::JobQueue* jobQueue_;
    ripple::hotstuff::Config config_;
    beast::Journal journal_;
    ripple::hotstuff::RoundRobinLeader pacemaker_;
    ripple::hotstuff::Hotstuff* hotstuff_;
    KeyPair key_pair_;
    bool malicious_;

    std::map<BlockHash, Block> cache_blocks_;
    std::vector<Block> consented_blocks_;
};
std::map<Replica::ReplicaID, Replica*> Replica::replicas;


class Hotstuff_test : public beast::unit_test::suite {
public:
    Hotstuff_test()
    : false_replicas_(1)
    , replicas_(false_replicas_*3 + 1)
    , blocks_(4)
    , view_change_(1)
    , cmd_batch_size_(100)
    , timeout_(7)
    , disable_log_(false)
    , logs_(std::make_unique<SuiteLogs>(*this)) {
        logs_->threshold(beast::severities::kAll);
    }

    void parse_args() {
        std::istringstream is(arg());
        std::string token;
        std::vector<std::string> tokens;
        while(std::getline(is, token, ' ')) {
            tokens.push_back(token);
        }

        for(std::size_t i = 0; i < tokens.size(); i++) {
            const std::string& token = tokens[i];
            std::size_t npos = token.find("=");
            if(npos == -1) {
                continue;
            }

            std::string arg_key = token.substr(0, npos);
            std::string arg_value = token.substr(npos + 1);
            if (arg_key.compare("disable_log") == 0) {
                if(arg_value.compare("1") == 0) {
                    disable_log_ = true;
                    logs_->threshold(beast::severities::kDisabled);
                }
            } else if (arg_key.compare("false_replicas") == 0) {
                false_replicas_ = std::atoi(arg_value.c_str());
                replicas_ = false_replicas_*3 + 1;
            } else if (arg_key.compare("blocks") == 0) {
                blocks_ = std::atoi(arg_value.c_str());
            } else if (arg_key.compare("view_change") == 0) {
                view_change_ = std::atoi(arg_value.c_str());
            } else if (arg_key.compare("timeout") == 0) {
                timeout_ = std::atoi(arg_value.c_str());
            } else if (arg_key.compare("batch_size") == 0) {
                cmd_batch_size_ = std::atoi(arg_value.c_str());
            }
        }
    }

    // initial a replica
    Replica* initReplica(jtx::Env* env, const ripple::hotstuff::Config& config) {
        Replica* r = new Replica(env, config);
        r->init();
        return r;
    }

    // create replica instances by parameter
    std::size_t newReplicas(jtx::Env* env, int replicas) {
        //return addReplicas(env, replicas);
        ripple::hotstuff::Config config;
        config.view_change = view_change_;
        config.timeout = timeout_;
        for(int i = 0; i < replicas; i++) {
            config.leader_schedule.push_back( i + 1);
        }
        
        for(int i = 0; i < replicas; i++) {
            config.id = config.leader_schedule[i];
            initReplica(env, config);
        }
        return Replica::replicas.size();
    }

    // add replicas adn run replicas
    std::size_t addReplicas(jtx::Env* env, int replicas) {
        // update old replicas' leader schedule
        for(auto it = Replica::replicas.begin(); it != Replica::replicas.end(); it++) {
            std::size_t size = it->second->leader_schedule().size();
            for(int i = 0; i < replicas; i++) {
                it->second->leader_schedule().push_back(size + i + 1);
            }
        }

        ripple::hotstuff::Config config;
        config.view_change = view_change_;
        config.timeout = timeout_;
        config.cmd_batch_size = cmd_batch_size_;
        if(Replica::replicas.size() > 0)
            config.leader_schedule = Replica::replicas[1]->leader_schedule();

        std::size_t size = config.leader_schedule.size();
        for(int i = size - replicas; i < size; i++) {
            config.id = config.leader_schedule[i];
            initReplica(env, config);
        }
        return Replica::replicas.size();
    }

    // release replicas created by test case
    void freeReplicas(const Replica::ReplicaID& id = -1) {
        if(id == -1) {
            for(auto it = Replica::replicas.begin(); it != Replica::replicas.end(); it++) {
                delete it->second;
            }
        } else {
            auto it = Replica::replicas.find(id);
            if(it != Replica::replicas.end())
                delete it->second;
        }

        clearReplicas(id);
    }

    void clearReplicas(const Replica::ReplicaID& id = -1) {
        if(id == -1)
            Replica::replicas.clear();
        else {
            auto it = Replica::replicas.find(id);
            if(it != Replica::replicas.end())
                Replica::replicas.erase(it);
        }
    }

    void runReplicas() {
        for(auto it = Replica::replicas.begin(); it != Replica::replicas.end(); it++) {
            it->second->run();
        }
    }

    void stopReplicas(jtx::Env* env, const Replica::ReplicaID& id = -1) {
        if(id == -1) {
            for(auto it = Replica::replicas.begin(); it != Replica::replicas.end(); it++) {
                it->second->stop();
            }

            for(;;) {
                if(env->app().getJobQueue().getJobCountTotal(jtPROPOSAL_t) == 0)
                    break;
            }
        } else {
            auto it = Replica::replicas.find(id);
            if(it != Replica::replicas.end()) {
                it->second->stop();
            }
        }
    }

    void setReplicaMalicious(const Replica::ReplicaID& id, bool mailicious) {
        auto it = Replica::replicas.find(id);
        if(it != Replica::replicas.end()) {
            it->second->malicious(mailicious);
            it->second->stop();
        }
    }

    bool waitUntilConsentedBlocks(std::size_t consentedBlockNumber, int retry = 0x7fffffff) {
        // 满足共识的出块量
        int timeout = 0;
        while(true) {
            bool satisfied = true;
            for(auto it = Replica::replicas.begin(); it != Replica::replicas.end(); it++) {
                const std::vector<Replica::Block>& consentedBlocks = it->second->consentedBlocks();
                if(consentedBlocks.size() < consentedBlockNumber) {
                    satisfied = false;
                    break;
                }
            }

            if(++timeout > retry)
                return false;

            if(satisfied)
                break;
        }

        // 判断 consentedBlocks 块是否相等
        for(std::size_t block = 0; block < consentedBlockNumber; block ++) {
            auto begin = Replica::replicas.begin();
            ripple::hotstuff::BlockHash hash = begin->second->consentedBlocks()[block].hash;
            begin++;
            for(;begin != Replica::replicas.end(); begin++) {
                if(hash != begin->second->consentedBlocks()[block].hash) {
                    return false;
                }
            }
        }
        return true;
    }

    void testElectLeader() {
        jtx::Env env{*this};
        boost::asio::io_service ios;
        ripple::hotstuff::RoundRobinLeader rrl(&ios);
        ripple::hotstuff::Config config;
        config.id = 1;
        config.view_change = 1;
        config.leader_schedule.push_back(config.id);
        config.leader_schedule.push_back(2);
        config.leader_schedule.push_back(3);
        config.leader_schedule.push_back(4);
        Replica replica(&env, config);
        BEAST_EXPECT(rrl.GetLeader(0) == -1);
        ripple::hotstuff::Hotstuff ht(
            nullptr,
            config,
            logs_->journal("testElectLeader"),
            &replica,
            &replica,
            &replica,
            &rrl
        );

        // view 0,4, ... -> replicaID 1
        // view 1,5, ... -> replicaID 2
        // view 2,6, ... -> replicaID 3 
        // view 3,7, ... -> replicaID 4 
        std::size_t size = config.leader_schedule.size();
        for(int view = 0; view < 8; view++) {
            ripple::hotstuff::ReplicaID nextLeader = rrl.GetLeader(view);
            BEAST_EXPECT(nextLeader == (view % size) + 1);
        }

        clearReplicas();
    }

    // 正常情况测试用例
    void testNormalRoundRobinLeader() {
        jtx::Env env{*this};
        if(disable_log_ == true)
            env.app().logs().threshold(beast::severities::kDisabled);
        // create replicas
        newReplicas(&env, replicas_);

        runReplicas();
        BEAST_EXPECT(waitUntilConsentedBlocks(blocks_) == true);
        stopReplicas(&env);
        //env.app().signalStop();
        freeReplicas();
        BEAST_EXPECT(Replica::replicas.size() == 0);
    }

    // 测试增加 replicas
    void testAddReplicasRoundRobinLeader() {
        jtx::Env env{*this};
        if(disable_log_ == true)
            env.app().logs().threshold(beast::severities::kDisabled);
        // create replicas
        newReplicas(&env, replicas_);

        runReplicas();
        BEAST_EXPECT(waitUntilConsentedBlocks(blocks_) == true);

        // add some replicas
        addReplicas(&env, 2);
        runReplicas();
        BEAST_EXPECT(waitUntilConsentedBlocks(2*blocks_) == true);

        stopReplicas(&env);
        //env.app().signalStop();
        freeReplicas();
        BEAST_EXPECT(Replica::replicas.size() == 0);
    }

    // 有恶意节点测试用例
    // 恶意节点不发送 proposal
    void testMaliciousRoundRobinLeader() {
        jtx::Env env{*this};
        if(disable_log_ == true)
            env.app().logs().threshold(beast::severities::kDisabled);
        // create replicas
        newReplicas(&env, replicas_);

        std::set<Replica::ReplicaID> maliciousIDs;
        for(;;) {
            // use current time as seed for random generator
            std::srand(std::time(nullptr)); 
            Replica::ReplicaID id = (std::rand() % replicas_) + 1;
            maliciousIDs.insert(id);
            if(maliciousIDs.size() == false_replicas_)
                break;
        }
        for(auto it = maliciousIDs.begin(); it != maliciousIDs.end(); it++) {
            std::cout << "malicious id -> " << (*it) << std::endl;
        }

        runReplicas();
        BEAST_EXPECT(waitUntilConsentedBlocks(4) == true);
        // 设置 maliciousIDs 的节点为恶意节点
        for(auto it = maliciousIDs.begin(); it != maliciousIDs.end(); it++) {
            setReplicaMalicious(*it, true);
        }
        for(auto it = maliciousIDs.begin(); it != maliciousIDs.end(); it++) {
            stopReplicas(&env, *it);
            clearReplicas(*it);
        }
        BEAST_EXPECT(waitUntilConsentedBlocks(4 + blocks_) == true);

        stopReplicas(&env);
        //env.app().signalStop();
        freeReplicas();
        BEAST_EXPECT(Replica::replicas.size() == 0);
    }

    void run() override {
        parse_args();

        testElectLeader();
        testNormalRoundRobinLeader();
        //testAddReplicasRoundRobinLeader();
        testMaliciousRoundRobinLeader();
    }

private:
    int false_replicas_;
    int replicas_;
    int blocks_;
    int view_change_;
    int cmd_batch_size_;
    int timeout_;
    bool disable_log_;
    std::unique_ptr<ripple::Logs> logs_;
};

BEAST_DEFINE_TESTSUITE(Hotstuff, consensus, ripple);

} // namespace ripple
} // namespace test