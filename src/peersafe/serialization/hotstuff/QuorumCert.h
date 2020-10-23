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

#ifndef RIPPLE_SERIALIZATION_HOTSTUFF_QUORUMCERT_H
#define RIPPLE_SERIALIZATION_HOTSTUFF_QUORUMCERT_H

#include <peersafe/serialization/Serialization.h>
#include <peersafe/serialization/hotstuff/BlockInfo.h>
#include <peersafe/serialization/hotstuff/VoteData.h>

#include <peersafe/consensus/hotstuff/impl/QuorumCert.h>

namespace ripple { namespace hotstuff {

template<class Archive>
void serialize(
	Archive& ar, 
	ripple::hotstuff::LedgerInfoWithSignatures::LedgerInfo& ledger_info, 
	const unsigned int /*version*/) {
	ar & ledger_info.commit_info;
	ar & ledger_info.consensus_data_hash;
}

template<class Archive>
void serialize(
	Archive& ar, 
	ripple::hotstuff::LedgerInfoWithSignatures& ls, 
	const unsigned int /*version*/) {
	ar & ls.ledger_info;
	ar & ls.signatures;
}

template<class Archive>
void serialize(
	Archive& ar,
	ripple::hotstuff::QuorumCertificate& qc,
	const unsigned int /*version*/) {
	ar & qc.vote_data();
	ar & qc.ledger_info();
}

template<class Archive>
void serialize(
	Archive& ar,
	ripple::hotstuff::TimeoutCertificate& tc,
	const unsigned int /*version*/) {
	ar & tc.timeout();
	ar & tc.signatures();
}

} // namespace serialization
} // namespace ripple

#endif // RIPPLE_SERIALIZATION_HOTSTUFF_QUORUMCERT_H