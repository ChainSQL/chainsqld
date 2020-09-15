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

#include <test/app/SuitLogs.h>

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
    using Key = ripple::hotstuff::BlockHash;
    using Value = ripple::hotstuff::Block;

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

    bool blockOf(const ripple::hotstuff::BlockHash& hash, 
        ripple::hotstuff::Block& block) const {
        auto it = blocks_.find(hash);
        if(it == blocks_.end()) {
            return false;
        }
        
        block = it->second;
        return true;
    }

    bool expectBlock(const ripple::hotstuff::BlockHash& hash, 
        ripple::hotstuff::Block& block) {

        if(blockOf(hash, block))
            return true;
        // sync blocks
        return false;
    }

    std::map<Key, Value>& blocks() {
        return blocks_;
    }

private:
    std::size_t base_size_;
    std::map<Key, Value> blocks_;
};

class FakeExecutor : public ripple::hotstuff::Executor {
public:
    FakeExecutor(Replicas* replicas)
    : replicas_(replicas)
    , last_()
    , consented_counts_(0) {

    }

    ~FakeExecutor() {

    }

    bool accept(const ripple::hotstuff::Command& cmd) {
        return true;
    }

    void consented(const ripple::hotstuff::Block& block) {
        last_ = block;
        consented_counts_++;
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

    const int& consentedSize() const {
        return consented_counts_;
    }

private:
    Replicas* replicas_;
    ripple::hotstuff::Block last_;
    int consented_counts_;
};

class TestHotstuffCore {
public:
    using pointer = std::shared_ptr<TestHotstuffCore>;

    TestHotstuffCore(
        const ripple::hotstuff::ReplicaID& id,
        const beast::Journal& journal,
        Replicas* replicas)
    : storage_()
    , executor_(replicas)
    , hotstuffCore_(id, journal, ripple::hotstuff::Signal::weak(), &storage_, &executor_) {

    }

    ~TestHotstuffCore() {

    }

    ripple::hotstuff::Block CreatePropose() {
        return hotstuffCore_.CreatePropose(5);
    }

    bool OnReceiveProposal(const ripple::hotstuff::Block& block, ripple::hotstuff::PartialCert& cert) {
        return hotstuffCore_.OnReceiveProposal(block, cert);
    }

    void OnReceiveVote(const ripple::hotstuff::PartialCert& cert) {
        return hotstuffCore_.OnReceiveVote(cert);
    }

    const int Height() {
        return hotstuffCore_.Height();
    }

    const ripple::hotstuff::Block& last() const {
        return executor_.last();
    }

    const int& consentedSize() const {
        return executor_.consentedSize();
    }

    FakeStorage& storage() {
        return storage_;
    }

    const ripple::hotstuff::QuorumCert HightQC() {
        return hotstuffCore_.HightQC();
    }

    const ripple::hotstuff::Block leaf() {
        return hotstuffCore_.leaf();
    }

    const ripple::hotstuff::Block& votedBlock() {
        return hotstuffCore_.votedBlock();
    }

    void setLeaf(const ripple::hotstuff::Block &block) {
        hotstuffCore_.setLeaf(block);
    }

    ripple::hotstuff::Block CreateLeaf(const ripple::hotstuff::Block& leaf, 
        const ripple::hotstuff::Command& cmd, 
        const ripple::hotstuff::QuorumCert& qc, 
        int height) {
        ripple::hotstuff::Block block = hotstuffCore_.CreateLeaf(leaf, cmd, qc, height);
        block.hash = ripple::hotstuff::Block::blockHash(block);
        return block;
    }

private:
    FakeStorage storage_;
    FakeExecutor executor_;
    ripple::hotstuff::HotstuffCore hotstuffCore_;
};

class HotstuffCore_test : public beast::unit_test::suite {
public:
    HotstuffCore_test()
    : false_replicas_(1)
    , replicas_(false_replicas_*3 + 1)
    , blocks_(4)
    , logs_(std::make_unique<SuiteLogs>(*this)) {
        logs_->threshold(beast::severities::kAll);
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

            std::string arg_key = token.substr(0, npos);
            std::string arg_value = token.substr(npos + 1);
            if (arg_key.compare("blocks") == 0) {
                blocks_ = std::atoi(arg_value.c_str());
            } else if (arg_key.compare("disable_log") == 0) {
                if(arg_value.compare("1") == 0) {
                    logs_->threshold(beast::severities::kDisabled);
                }
            } else if (arg_key.compare("false_repplicas") == 0) {
                false_replicas_ = std::atoi(arg_value.c_str());
                replicas_ = false_replicas_*3 + 1;
            }
        }
    }

    bool AllLastConsentedBlocksAreEqual(const std::vector<TestHotstuffCore::pointer>& hotstuffs) {
        
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
            hotstuffs.push_back(std::make_shared<TestHotstuffCore>(
                replicas.next_id(), 
                logs_->journal("testFixedLeaderChainedHotstuff"),
                &replicas));
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
                BEAST_EXPECT(AllLastConsentedBlocksAreEqual(hotstuffs) == true);
            }
        }
    }

    // leader 节点轮询出块
    void testRoundRobinLeaderChainedHotstuff() {
        Replicas replicas(replicas_);
        std::vector<TestHotstuffCore::pointer> hotstuffs;
        for(int i = 0; i < replicas_; i++) {
            hotstuffs.push_back(std::make_shared<TestHotstuffCore>(
                replicas.next_id(), 
                logs_->journal("testRoundRobinLeaderChainedHotstuff"),
                &replicas));
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
                BEAST_EXPECT(AllLastConsentedBlocksAreEqual(hotstuffs) == true);
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
            hotstuffs.push_back(std::make_shared<TestHotstuffCore>(
                replicas.next_id(), 
                logs_->journal("testVoteOfChainedHotstuffInNetworkAnomaly"),
                &replicas));
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
                BEAST_EXPECT(AllLastConsentedBlocksAreEqual(hotstuffs) == true);
            }
        }
    }

    // 模拟场景：
    // 当前 leader 创建的 block1 只被少许的 replicas 收到，
    // 而此时新的轮次 leaders 会分别创建新的 blocks，
    // 无论经过多少新轮次，都没有 blocks 被共识。
    // 即使在 block1 被多数 replicas 收到，最终也会没有 blocks 被共识
    void testMissProposalChainedHotstuff() {
        int replica_count = 4;
        int blocks = 24;
        Replicas replicas(replica_count);
        std::vector<TestHotstuffCore::pointer> hotstuffs;
        for(int i = 0; i < replica_count; i++) {
            hotstuffs.push_back(std::make_shared<TestHotstuffCore>(
                replicas.next_id(), 
                logs_->journal("testMissProposalChainedHotstuff"),
                &replicas));
        }

        ripple::hotstuff::ReplicaID next_header = 0;
        ripple::hotstuff::Block block1 = hotstuffs[next_header]->CreatePropose();

        // 模拟下一个 leader 和非 leader 节点收到 block1
        ripple::hotstuff::PartialCert cert1, cert2;
        // 非 leader 节点
        bool bok = hotstuffs[next_header]->OnReceiveProposal(block1, cert1);
        BEAST_EXPECT(bok == true);
        // 模拟下一个 leader 节点
        next_header = (next_header + 1) % replica_count;
        bok = hotstuffs[next_header]->OnReceiveProposal(block1, cert2);
        BEAST_EXPECT(bok == true);
        
        hotstuffs[next_header]->OnReceiveVote(cert1);
        hotstuffs[next_header]->OnReceiveVote(cert2);

        // 模拟上一轮中 block1 没有处理完，接着新轮次的 leader 开始创建 block2
        // 这种情况不会有新 block 被共识
        do {
            ripple::hotstuff::Block block2 = hotstuffs[next_header]->CreatePropose();
            BEAST_EXPECT(block1.height == block2.height);
            std::vector<ripple::hotstuff::PartialCert> paritalCerts;
            for (int id = 0; id < replica_count; id++)
            {
                ripple::hotstuff::PartialCert cert;
                if (hotstuffs[id]->OnReceiveProposal(block2, cert))
                    paritalCerts.push_back(cert);
            }

            // 下一个 leader 处理 vote 消息
            next_header = (next_header + 1) % replica_count;
            for (int i = 0; i < paritalCerts.size(); i++)
            {
                hotstuffs[next_header]->OnReceiveVote(paritalCerts[i]);
            }

            bool hasConsented = false;
            for (int id = 0; id < replica_count; id++)
            {
                if (hotstuffs[id]->consentedSize() > 0)
                {
                    hasConsented = true;
                    break;
                }
            }
            BEAST_EXPECT(hasConsented == false);

        } while (blocks--);

        // 模拟 block1 重新被一个节点接收到
        ripple::hotstuff::PartialCert cert3;
        bok = hotstuffs[2]->OnReceiveProposal(block1, cert3);
        BEAST_EXPECT(bok == false);

        blocks = 24;
        next_header = (next_header + 1) % replica_count;
        do {
            ripple::hotstuff::Block block2 = hotstuffs[next_header]->CreatePropose();
            BEAST_EXPECT(block1.height == block2.height);
            std::vector<ripple::hotstuff::PartialCert> paritalCerts;
            for (int id = 0; id < replica_count; id++)
            {
                ripple::hotstuff::PartialCert cert;
                if (hotstuffs[id]->OnReceiveProposal(block2, cert))
                    paritalCerts.push_back(cert);
            }

            // 下一个 leader 处理 vote 消息
            next_header = (next_header + 1) % replica_count;
            for (int i = 0; i < paritalCerts.size(); i++)
            {
                hotstuffs[next_header]->OnReceiveVote(paritalCerts[i]);
            }

            bool hasConsented = false;
            for (int id = 0; id < replica_count; id++)
            {
                if (hotstuffs[id]->consentedSize() > 0)
                {
                    hasConsented = true;
                    break;
                }
            }
            BEAST_EXPECT(hasConsented == false);

        } while (blocks--);
    }

    // 模拟修复 testMissProposalChainedHotstuff 测试场景
    void testFixedMissProposalChainedHotstuff() {
        int replica_count = 4;
        int blocks = 24;
        Replicas replicas(replica_count);
        std::vector<TestHotstuffCore::pointer> hotstuffs;
        for(int i = 0; i < replica_count; i++) {
            hotstuffs.push_back(std::make_shared<TestHotstuffCore>(
                replicas.next_id(), 
                logs_->journal("testFixedMissProposalChainedHotstuff"),
                &replicas));
        }

        ripple::hotstuff::ReplicaID next_header = 0;
        ripple::hotstuff::Block block1 = hotstuffs[next_header]->CreatePropose();

        // 模拟下一个 leader 和非 leader 节点收到 block1
        ripple::hotstuff::PartialCert cert1, cert2;
        // 非 leader 节点
        bool bok = hotstuffs[next_header]->OnReceiveProposal(block1, cert1);
        BEAST_EXPECT(bok == true);
        // 模拟下一个 leader 节点
        next_header = (next_header + 1) % replica_count;
        bok = hotstuffs[next_header]->OnReceiveProposal(block1, cert2);
        BEAST_EXPECT(bok == true);
        // 下一个 leader 处理 vote 消息
        hotstuffs[next_header]->OnReceiveVote(cert1);
        hotstuffs[next_header]->OnReceiveVote(cert2);

        // 修复
        bool canNotProposal = hotstuffs[next_header]->Height() < block1.height;
        BEAST_EXPECT(canNotProposal == true);

        // 模拟 leader 收到第三个 vote 消息
        ripple::hotstuff::PartialCert cert3;
        bok = hotstuffs[2]->OnReceiveProposal(block1, cert3);
        BEAST_EXPECT(bok == true);
        hotstuffs[next_header]->OnReceiveVote(cert3);
        canNotProposal = hotstuffs[next_header]->Height() < block1.height;
        BEAST_EXPECT(canNotProposal == false);

        for(int block = 0; block < blocks; block++) {
            ripple::hotstuff::Block block2 = hotstuffs[next_header]->CreatePropose();
            std::vector<ripple::hotstuff::PartialCert> paritalCerts;
            for (int id = 0; id < replica_count; id++)
            {
                ripple::hotstuff::PartialCert cert;
                if (hotstuffs[id]->OnReceiveProposal(block2, cert))
                    paritalCerts.push_back(cert);
            }

            // 下一个 leader 处理 vote 消息
            next_header = (next_header + 1) % replica_count;
            for (int i = 0; i < paritalCerts.size(); i++)
            {
                hotstuffs[next_header]->OnReceiveVote(paritalCerts[i]);
            }
            
            if(block > 2) {
                // 对于 block == 2 的情况下，replica id == 3 因为缺少 block.height == 2 
                // 的块，所以这里忽略 block == 2 的情况。但是，我们无需担心 hotstuff 算法的
                // 一致性和可靠性，因为大部分的节点都已经对 block.height == 2 块做了共识。 replica id == 3
                // 的节点只需要在应用层同步 block.height == 2 的块就能达到和其他的节点的一致的状态
                for(int id = 0; id < replica_count; id++) {
                    BEAST_EXPECT(hotstuffs[id]->last().height == block);
                }
                BEAST_EXPECT(AllLastConsentedBlocksAreEqual(hotstuffs) == true);
            }
        }
    }

    // 模拟场景
    // 新增一个 replica 节点
    // 待新增的 replica 节点成为 leader 前，先同步区块
    void testAddReplicaChainedHotstuff() {
        int replica_count = 4;
        Replicas replicas(replica_count);
        std::vector<TestHotstuffCore::pointer> hotstuffs;
        for(int i = 0; i < replica_count; i++) {
            hotstuffs.push_back(std::make_shared<TestHotstuffCore>(
                replicas.next_id(), 
                logs_->journal("testAddReplicaChainedHotstuff"),
                &replicas));
        }

        ripple::hotstuff::ReplicaID next_header = 0;
        while(true) {
            ripple::hotstuff::Block block = hotstuffs[next_header]->CreatePropose();
            std::vector<ripple::hotstuff::PartialCert> paritalCerts;
            for(int id = 0; id < replica_count - 1; id++) {
                ripple::hotstuff::PartialCert cert;
                if(hotstuffs[id]->OnReceiveProposal(block, cert))
                    paritalCerts.push_back(cert);
            }

            next_header = (next_header + 1) % (replica_count - 1);
            for(int i = 0; i < paritalCerts.size(); i++) {
                hotstuffs[next_header]->OnReceiveVote(paritalCerts[i]);
            }

            bool hasFourBlocks = true;
            for (int id = 0; id < replica_count - 1; id++)
            {
                if (hotstuffs[id]->consentedSize() < 4)
                {
                    hasFourBlocks = false;
                    break;
                }
            }
            if(hasFourBlocks)
                break;
        }

        // 将 replica_count - 1 节点的 blocks 补齐
        std::map<FakeStorage::Key, FakeStorage::Value>& fullBlocks = hotstuffs[0]->storage().blocks();
        for(auto it = fullBlocks.begin(); it != fullBlocks.end(); it++) {
            hotstuffs[replica_count - 1]->storage().addBlock(it->second);
        }

        next_header = replica_count - 1;
        while(true) {
            ripple::hotstuff::Block block2 = hotstuffs[next_header]->CreatePropose();
            std::vector<ripple::hotstuff::PartialCert> paritalCerts;
            for (int id = 0; id < replica_count; id++)
            {
                ripple::hotstuff::PartialCert cert;
                if (hotstuffs[id]->OnReceiveProposal(block2, cert))
                    paritalCerts.push_back(cert);
            }

            next_header = (next_header + 1) % replica_count;
            for(int i = 0; i < paritalCerts.size(); i++) {
                hotstuffs[next_header]->OnReceiveVote(paritalCerts[i]);
            }

            bool hasSixBlocks = true;
            for (int id = 0; id < replica_count - 1; id++)
            {
                if (hotstuffs[id]->consentedSize() < 6)
                {
                    hasSixBlocks = false;
                    break;
                }
            }

            if(hasSixBlocks) {
                BEAST_EXPECT(AllLastConsentedBlocksAreEqual(hotstuffs) == true);
                break;
            }
        }
    }

    void testBenchmarkChainedHotstuff() {
        Replicas replicas(replicas_);
        std::vector<TestHotstuffCore::pointer> hotstuffs;
        for(int i = 0; i < replicas_; i++) {
            hotstuffs.push_back(std::make_shared<TestHotstuffCore>(
                replicas.next_id(),
                logs_->journal("testBenchmarkChainedHotstuff"),
                &replicas));
        }

        int quorum_size = quorumSize();
        std::map<ripple::hotstuff::ReplicaID,ripple::hotstuff::PartialCert> lastParitalCerts;
        for(int view = 1; view < blocks_; view++) {
            ripple::hotstuff::ReplicaID next_header = (view - 1) % replicas_;
            if(next_header == 3) {
                for (int id = 0; id < quorum_size; id++) {
                    ripple::hotstuff::Command cmd;
                    TestHotstuffCore::pointer& hotstuff = hotstuffs[id];
                    ripple::hotstuff::Block votedBlock = hotstuff->votedBlock();

                    next_header = (votedBlock.height + 1) % replicas_;
                    hotstuffs[next_header]->OnReceiveVote(lastParitalCerts.find(id)->second);
                }

                ripple::hotstuff::Block block = hotstuffs[next_header]->CreatePropose();
                std::vector<ripple::hotstuff::PartialCert> paritalCerts;
                for(int id = 0; id < quorum_size; id ++) {
                    ripple::hotstuff::PartialCert cert;
                    if(hotstuffs[id]->OnReceiveProposal(block, cert)) {
                        paritalCerts.push_back(cert);
                    }
                }

                for(std::size_t i = 0; i < paritalCerts.size(); i++) {
                    hotstuffs[next_header]->OnReceiveVote(paritalCerts[i]);
                }
            } else {
                ripple::hotstuff::Block block = hotstuffs[next_header]->CreatePropose();
                std::vector<ripple::hotstuff::PartialCert> paritalCerts;
                lastParitalCerts.clear();
                for(int id = 0; id < quorum_size; id ++) {
                    ripple::hotstuff::PartialCert cert;
                    if(hotstuffs[id]->OnReceiveProposal(block, cert)) {
                        paritalCerts.push_back(cert);
                        lastParitalCerts[id] = cert;
                    }
                }

                for(std::size_t i = 0; i < paritalCerts.size(); i++) {
                    hotstuffs[next_header + 1]->OnReceiveVote(paritalCerts[i]);
                }
            }
        }

        bool bok = true;
        int count = hotstuffs[0]->consentedSize();
        std::cout << "0 -> " << count << std::endl;
        for (int id = 1; id < quorum_size; id++) {
            std::cout << id << " -> " << hotstuffs[id]->consentedSize() << std::endl;
            if(count != hotstuffs[id]->consentedSize()) {
                bok = false;
                break;
            }
        }

        if(bok == false) {
            BEAST_EXPECT(bok);
            return;
        }

        const ripple::hotstuff::Block& last = hotstuffs[0]->last();
        for (int id = 1; id < quorum_size; id++) {
            if(last.hash != hotstuffs[id]->last().hash) {
                bok = false;
                break;
            }
        }

        BEAST_EXPECT(bok);
    }

    void run() override {
        parse_args();

        //testFixedLeaderChainedHotstuff();
        //testRoundRobinLeaderChainedHotstuff();

        //testVoteOfChainedHotstuffInNetworkAnomaly();
        //testMissProposalChainedHotstuff();
        //testFixedMissProposalChainedHotstuff();
        //testAddReplicaChainedHotstuff();
        testBenchmarkChainedHotstuff();
    }

private:
    int false_replicas_;
    int replicas_;
    int blocks_;
    std::unique_ptr<ripple::Logs> logs_;
};

BEAST_DEFINE_TESTSUITE(HotstuffCore, consensus, ripple);

} // namespace ripple
} // namespace test