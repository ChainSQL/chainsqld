//------------------------------------------------------------------------------
/*
This file is part of chainsqld: https://github.com/chainsql/chainsqld
Copyright (c) 2016-2018 Peersafe Technology Co., Ltd.

chainsqld is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

chainsqld is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
You should have received a copy of the GNU General Public License
along with cpp-ethereum.  If not, see <http://www.gnu.org/licenses/>.
*/
//==============================================================================

#include <peersafe/app/shard/NodeBase.h>
#include <peersafe/app/shard/ShardManager.h>
#include <peersafe/app/misc/TxPool.h>
#include <ripple/app/misc/NetworkOPs.h>


namespace ripple {

NodeBase::NodeBase(ShardManager& m, Application& app, Config& cfg, beast::Journal journal)
    : mShardManager(m)
    , app_(app)
    , journal_(journal)
    , cfg_(cfg)
{
}

void NodeBase::onMessage(std::shared_ptr<protocol::TMTransactions> const& m)
{
    using beast::hash_append;

    PublicKey const publicKey(makeSlice(m->nodepubkey()));

    boost::optional<PublicKey> pubKey =
        mShardManager.lookup().validators().getTrustedKey(publicKey);
    if (!pubKey)
    {
        JLOG(journal_.info()) << "Transactions package from untrusted lookup node";
        return;
    }

    sha512_half_hasher checkHash;
    std::vector<std::shared_ptr<Transaction>> txs;

    for (auto const& TMTransaction : m->transactions())
    {
        SerialIter sit(makeSlice(TMTransaction.rawtransaction()));
        auto stx = std::make_shared<STTx const>(sit);
        std::string reason;
        txs.emplace_back(std::make_shared<Transaction>(stx, reason, app_));
        hash_append(checkHash, stx->getTransactionID());
    }

    if (!verifyDigest(
        *pubKey,
        static_cast<typename sha512_half_hasher::result_type>(checkHash),
        makeSlice(m->signature())))
    {
        JLOG(journal_.info()) << "Transactions package signature verification failed";
        return;
    }

    for (auto& tx : txs)
    {
        app_.getOPs().processTransaction(
            tx, false, false, NetworkOPs::FailHard::no);
    }
}

}
