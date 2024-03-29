//------------------------------------------------------------------------------
/*
    This file is part of rippled: https://github.com/ripple/rippled
    Copyright (c) 2012-2016 Ripple Labs Inc.

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


#include <peersafe/consensus/impl/Adaptor.cpp>
#include <peersafe/consensus/impl/RpcaPopAdaptor.cpp>

#include <peersafe/consensus/rpca/impl/RpcaAdaptor.cpp>
#include <peersafe/consensus/rpca/impl/RpcaConsensus.cpp>

#include <peersafe/consensus/pop/impl/ViewChangeManager.cpp>
#include <peersafe/consensus/pop/impl/PopAdaptor.cpp>
#include <peersafe/consensus/pop/impl/PopConsensus.cpp>

#include <peersafe/consensus/hotstuff/impl/Block.cpp>
#include <peersafe/consensus/hotstuff/impl/BlockStorage.cpp>
#include <peersafe/consensus/hotstuff/impl/EpochChange.cpp>
#include <peersafe/consensus/hotstuff/impl/EpochState.cpp>
#include <peersafe/consensus/hotstuff/impl/HotstuffCore.cpp>
#include <peersafe/consensus/hotstuff/impl/PendingVotes.cpp>
#include <peersafe/consensus/hotstuff/impl/ProposalGenerator.cpp>
#include <peersafe/consensus/hotstuff/impl/QuorumCert.cpp>
#include <peersafe/consensus/hotstuff/impl/RoundManager.cpp>
#include <peersafe/consensus/hotstuff/impl/RoundState.cpp>
#include <peersafe/consensus/hotstuff/impl/SyncInfo.cpp>
#include <peersafe/consensus/hotstuff/impl/Vote.cpp>
#include <peersafe/consensus/hotstuff/impl/VoteData.cpp>
#include <peersafe/consensus/hotstuff/impl/HotstuffAdaptor.cpp>
#include <peersafe/consensus/hotstuff/impl/Hotstuff.cpp>
#include <peersafe/consensus/hotstuff/impl/HotstuffConsensus.cpp>

