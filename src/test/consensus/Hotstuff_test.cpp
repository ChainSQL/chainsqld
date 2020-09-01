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

#include <string>
#include <map>
#include <utility>
#include <sstream>
#include <iostream>
#include <memory>
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

namespace ripple {
namespace test {

class Replicas {
public:

    using ReplicaID = ripple::hotstuff::ReplicaID;
    using Replica = std::pair<ripple::PublicKey, ripple::SecretKey>;
    std::map<ReplicaID, Replica> entries;

    Replicas(int quorum_size)
    : entries()
    , next_replica_id_(0) {
        for(int i = 1; i <= quorum_size; i++) {
             entries[i] = ripple::randomKeyPair(ripple::KeyType::secp256k1);
        }
    }

    std::size_t resize(const int quorum_size) {
        entries.clear();
        for(int i = 1; i <= quorum_size; i++) {
             entries[i] = ripple::randomKeyPair(ripple::KeyType::secp256k1);
        }
        return entries.size();
    }

    std::size_t size() const {
        return entries.size();
    }

    const Replica& replica(const ReplicaID& id) const {
        return entries.at(id);
    }

    const ReplicaID INVALIDID = -1;
    const ReplicaID& next_id() {
        auto it = entries.find(++next_replica_id_);
        if(it != entries.end())
            return it->first;
        return INVALIDID;
    }

private:
    ReplicaID next_replica_id_;
};

class FakeStorage : public ripple::hotstuff::Storage {
public:
    FakeStorage()
    : base_size_(1)
    , blocks_() {

    }

    ~FakeStorage() {

    }

    // for transactions
    void command(std::size_t batch_size, 
        ripple::hotstuff::Command& cmd) {
        std::size_t counts = base_size_ + batch_size;
        for(std::size_t i = base_size_; i < counts; i ++) {
            using beast::hash_append;

            std::srand(std::time(nullptr));
            ripple::sha512_half_hasher h;
            hash_append(h, i*std::rand());
            sha512_half_hasher::result_type hash = 
                static_cast<typename sha512_half_hasher::result_type>(h);

            cmd.push_back(std::string((const char*)hash.data(), hash.size()));
        }
        base_size_ += batch_size;
    }

    // for blocks
    bool addBlock(const ripple::hotstuff::Block& block) {
        if(blocks_.find(block.hash) != blocks_.end())
            return false;
        
        blocks_[block.hash] = block;
        return true;
    }

    bool getBlock(const ripple::hotstuff::BlockHash& hash, 
        ripple::hotstuff::Block& block) const {
        auto it = blocks_.find(hash);
        if(it == blocks_.end())
            return false;
        
        block = it->second;
        return true;
    }

private:
    std::size_t base_size_;
    using Key = ripple::hotstuff::BlockHash;
    using Value = ripple::hotstuff::Block;
    std::map<Key, Value> blocks_;
};

class FakeExecutor : public ripple::hotstuff::Executor {
public:
    FakeExecutor(Replicas* replicas)
    : replicas_(replicas)
    , last_() {

    }

    ~FakeExecutor() {

    }

    bool accept(const ripple::hotstuff::Command& cmd) {
        return true;
    }

    void consented(const ripple::hotstuff::Block& block) {
        last_ = block;
    }

    int quorumSize() {
        std::size_t size = replicas_->size();
        return size - (size - 1)/3;
    }

    bool signature(const ripple::hotstuff::ReplicaID& id, 
        const ripple::hotstuff::BlockHash& hash, 
        ripple::hotstuff::PartialSig& partialSig) {
        const Replicas::Replica& r = replicas_->replica(id);
        partialSig.ID = id;
        partialSig.sig = ripple::sign(ripple::KeyType::secp256k1, 
            r.second,
            ripple::Slice((const void*)hash.data(), hash.size()));
        return true;
    }

    bool verifySignature(const ripple::hotstuff::PartialSig& partialSig, 
        const ripple::hotstuff::BlockHash& hash) {
        const Replicas::Replica& r = replicas_->replica(partialSig.ID);
        return ripple::verify(
            r.first, 
            ripple::Slice((const void*)hash.data(), hash.size()), 
            ripple::Slice((const void*)partialSig.sig.data(), partialSig.sig.size()));
    }

    const ripple::hotstuff::Block& last() const {
        return last_;
    }

private:
    Replicas* replicas_;
    ripple::hotstuff::Block last_;
};

class TestHotstuffCore {
public:
    using pointer = std::shared_ptr<TestHotstuffCore>;

    TestHotstuffCore(const ripple::hotstuff::ReplicaID& id, Replicas* replicas)
    : storage_()
    , executor_(replicas)
    , hotstuffCore_(id, &storage_, &executor_) {

    }

    ~TestHotstuffCore() {

    }

    ripple::hotstuff::Block CreatePropose() {
        return hotstuffCore_.CreatePropose();
    }

    bool OnReceiveProposal(const ripple::hotstuff::Block& block, ripple::hotstuff::PartialCert& cert) {
        return hotstuffCore_.OnReceiveProposal(block, cert);
    }

    void OnReceiveVote(const ripple::hotstuff::PartialCert& cert) {
        return hotstuffCore_.OnReceiveVote(cert);
    }

    const ripple::hotstuff::Block& last() const {
        return executor_.last();
    }
private:
    FakeStorage storage_;
    FakeExecutor executor_;
    ripple::hotstuff::HotstuffCore hotstuffCore_;
};

class Hotstuff_test : public beast::unit_test::suite {
public:
    Hotstuff_test()
    : replicas_(4)
    , blocks_(4) {

    }

    const int quorumSize() const {
        return replicas_ - (replicas_ - 1)/3;
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

            std::string arg_key = token.substr(0, npos - 1);
            std::string arg_value = token.substr(npos + 1);
            if (arg_key.compare("block") == 0) {
                blocks_ = std::atoi(arg_value.c_str());
            } else if (arg_key.compare("replicas") == 0) {
                replicas_ = std::atoi(arg_value.c_str());
            }
        }
    }

    bool isAllHashEqual(const std::vector<TestHotstuffCore::pointer>& hotstuffs) {
        
        if(hotstuffs.size() == 0)
            return false;

        bool equal = true;
        std::size_t replicas = hotstuffs.size();
        ripple::hotstuff::BlockHash hash = hotstuffs[0]->last().hash;
        for (int id = 1; id < replicas; id++)
        {
            if (hash != hotstuffs[id]->last().hash)
            {
                equal = false;
                break;
            }
        }
        return equal;
    }

    // 固定 leader 节点出块
    void testFixedLeaderChainedHotstuff() {
        Replicas replicas(replicas_);
        std::vector<TestHotstuffCore::pointer> hotstuffs;
        for(int i = 0; i < replicas_; i++) {
            hotstuffs.push_back(std::make_shared<TestHotstuffCore>(replicas.next_id(), &replicas));
        }

        for(int i = 0; i < blocks_; i++) {
            // leader proposes a block, then handles a block and vote 
            ripple::hotstuff::Block block = hotstuffs[0]->CreatePropose();
            ripple::hotstuff::PartialCert cert1;
            bool bOk = hotstuffs[0]->OnReceiveProposal(block, cert1);
            BEAST_EXPECT(bOk == true);
            hotstuffs[0]->OnReceiveVote(cert1);

            // replicas handle block and vote
            for(int id = 1; id < replicas_; id++) {
                ripple::hotstuff::PartialCert cert2;
                bool bOk = hotstuffs[id]->OnReceiveProposal(block, cert2);
                BEAST_EXPECT(bOk == true);
                hotstuffs[0]->OnReceiveVote(cert2);
            }

            if (i >= 3)
            {
                int expected_block_height = i - 1;
                for(int id = 0; id < replicas_; id++) {
                    BEAST_EXPECT(hotstuffs[id]->last().height == expected_block_height);
                }
                BEAST_EXPECT(isAllHashEqual(hotstuffs) == true);
            }
        }
    }

    // leader 节点轮询出块
    void testRoundRobinLeaderChainedHotstuff() {
        Replicas replicas(replicas_);
        std::vector<TestHotstuffCore::pointer> hotstuffs;
        for(int i = 0; i < replicas_; i++) {
            hotstuffs.push_back(std::make_shared<TestHotstuffCore>(replicas.next_id(), &replicas));
        }
        for(int i = 0; i < blocks_; i++) {
            int leader_id = i % replicas_;
            ripple::hotstuff::Block block = hotstuffs[leader_id]->CreatePropose();

            std::vector<ripple::hotstuff::PartialCert> paritalCerts;
            for(int id = 0; id < replicas_; id++) {
                ripple::hotstuff::PartialCert cert;
                bool bOk = hotstuffs[id]->OnReceiveProposal(block, cert);
                BEAST_EXPECT(bOk == true);
                paritalCerts.push_back(cert);
            }

            // send vote to next leader
            int next_leader_id = (leader_id + 1) % replicas_;
            BEAST_EXPECT(paritalCerts.size() == replicas_);
            for(std::size_t i = 0; i < quorumSize(); i++) {
                hotstuffs[next_leader_id]->OnReceiveVote(paritalCerts[i]);
            }

            if(i >= 3) {
                for(int id = 0; id < replicas_; id++) {
                    int expected_block_height = i - 1;
                    BEAST_EXPECT(hotstuffs[id]->last().height == expected_block_height);
                }
                BEAST_EXPECT(isAllHashEqual(hotstuffs) == true);
            }
        }
    }
    
    // 模拟网络异常情况
    // 由于网络存在异常，可能导致 vote 的信息接收不及时
    void testVoteOfChainedHotstuffInNetworkAnomaly() {
        int count = 4;
        Replicas replicas(count);
        std::vector<TestHotstuffCore::pointer> hotstuffs;
        for(int i = 0; i < count; i++) {
            hotstuffs.push_back(std::make_shared<TestHotstuffCore>(replicas.next_id(), &replicas));
        }
        
        int current_block_num = 0;
        int current_leader_id = current_block_num % count;
        int next_leader_id = (current_leader_id + 1) % count;
        ripple::hotstuff::Block block = hotstuffs[current_leader_id]->CreatePropose();

        // 模拟网络异常情况：
        // 所有的 replicas 都能收到 proposal
        std::vector<ripple::hotstuff::PartialCert> paritalCerts;
        for (int id = 0; id < count; id++)
        {
            ripple::hotstuff::PartialCert cert;
            if(hotstuffs[id]->OnReceiveProposal(block, cert))
                paritalCerts.push_back(cert);
        }

        // 模拟 next leader 由于网络异常只收到两个 vote 消息
        int exception_leader_id = next_leader_id;
        ripple::hotstuff::PartialCert exception_leader_cert = paritalCerts[2];
        for(std::size_t i = 0; i < 2; i++) {
            hotstuffs[next_leader_id]->OnReceiveVote(paritalCerts[i]);
        }

        // 后续的 proposal 的 block.height 都为 2，这样会导致 blocks 无法继续前进
        next_leader_id = (next_leader_id + 1) % count;
        for(int i = 0; i < count; i++) {
            ripple::hotstuff::Block block2 = hotstuffs[next_leader_id]->CreatePropose();
            BEAST_EXPECT(block2.height == block.height);
            next_leader_id = (next_leader_id + 1) % count;

            ripple::hotstuff::PartialCert cert;
            BEAST_EXPECT(hotstuffs[0]->OnReceiveProposal(block2, cert) == false);
            BEAST_EXPECT(hotstuffs[1]->OnReceiveProposal(block2, cert) == false);
            BEAST_EXPECT(hotstuffs[2]->OnReceiveProposal(block2, cert) == false);
            BEAST_EXPECT(hotstuffs[3]->OnReceiveProposal(block2, cert) == false);
        }

        // 过了段时间 exception_leader_id 重新收到 exception_leader_cert
        hotstuffs[exception_leader_id]->OnReceiveVote(exception_leader_cert);
        
        // 模拟同时 next leader 创建 proposal 和 exception_leader_id 处理 exception_leader_cert
        for(int i = 0; i < 8; i++) {
            ripple::hotstuff::Block block3 = hotstuffs[next_leader_id]->CreatePropose();
            std::vector<ripple::hotstuff::PartialCert> paritalCerts;
            paritalCerts.clear();
            for (int id = 0; id < count; id++)
            {
                ripple::hotstuff::PartialCert cert;
                if(hotstuffs[id]->OnReceiveProposal(block3, cert))
                    paritalCerts.push_back(cert);
            }
            next_leader_id = (next_leader_id + 1) % count;
            for (std::size_t i = 0; i < paritalCerts.size(); i++)
            {
                hotstuffs[next_leader_id]->OnReceiveVote(paritalCerts[i]);
            }

            if(i >= 5) {
                for (int id = 0; id < count; id++)
                {
                    int expected_block_height = i - 3;
                    BEAST_EXPECT(hotstuffs[id]->last().height == expected_block_height);
                }
                BEAST_EXPECT(isAllHashEqual(hotstuffs) == true);
            }
        }
    }

    void run() override {
        parse_args();

        testFixedLeaderChainedHotstuff();
        testRoundRobinLeaderChainedHotstuff();

        testVoteOfChainedHotstuffInNetworkAnomaly();
    }

private:
    int replicas_;
    int blocks_;
};

BEAST_DEFINE_TESTSUITE(Hotstuff, consensus, ripple);

} // namespace ripple
} // namespace test