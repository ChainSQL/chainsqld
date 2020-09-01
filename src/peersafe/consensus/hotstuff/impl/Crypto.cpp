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

#include <peersafe/consensus/hotstuff/impl/Crypto.h>

namespace ripple { namespace hotstuff {

////////////////////////////////////////////////////////////////////////////
// PartialCert
////////////////////////////////////////////////////////////////////////////
PartialCert::PartialCert() {

}

PartialCert::~PartialCert() {

}

////////////////////////////////////////////////////////////////////////////
// QuorumCert
////////////////////////////////////////////////////////////////////////////

QuorumCert::QuorumCert()
: sigs_()
, blockHash_() {

}

QuorumCert::QuorumCert(const BlockHash& hash)
: sigs_()
, blockHash_(hash) {

}

QuorumCert::~QuorumCert() {

}

bool QuorumCert::addPartiSig(const PartialCert& cert) {
    if (sigs_.find(cert.partialSig.ID) != sigs_.end())
        return false;
    
    if (blockHash_ != cert.blockHash)
        return false;

    sigs_[cert.partialSig.ID] = cert.partialSig;
    return true;
}

std::size_t QuorumCert::sizeOfSig() const {
    return sigs_.size();
}

const BlockHash& QuorumCert::hash() const {
    return blockHash_;
}

const std::map<QuorumCert::key, QuorumCert::value>& QuorumCert::sigs() const {
    return sigs_;
}

ripple::Blob QuorumCert::toBytes() const {
    ripple::Blob blob;

    for(auto it = blockHash_.begin(); it != blockHash_.end(); it++) {
        blob.push_back(*it);
    }

    return blob;
}

} // namespace hotstuff
} // namespace ripple