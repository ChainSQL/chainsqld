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

#ifndef RIPPLE_SERIALIZATION_LEDGERINFO_H
#define RIPPLE_SERIALIZATION_LEDGERINFO_H

#include <peersafe/serialization/Serialization.h>
#include <peersafe/serialization/base_unit.h>
#include <peersafe/serialization/NetClock.h>
#include <peersafe/serialization/ZXCAmount.h>

#include <ripple/ledger/ReadView.h>

namespace ripple {

template<class Archive>
void save(
	Archive& ar,
	const ripple::LedgerInfo& ledger_info,
	const unsigned int /*version*/) {

	ar & ledger_info.seq;
	ar & ledger_info.parentCloseTime;
	ar & ledger_info.hash;
	ar & ledger_info.txHash;
	ar & ledger_info.accountHash;
	ar & ledger_info.parentHash;
	ar & ledger_info.drops;
	uint32_t closeTimeResolution = ledger_info.closeTimeResolution.count();
	ar & closeTimeResolution;
	ar & ledger_info.closeTime;
}

template<class Archive>
void load(
	Archive& ar,
	ripple::LedgerInfo& ledger_info,
	const unsigned int /*version*/) {

	ar & ledger_info.seq;
	ar & ledger_info.parentCloseTime;
	ar & ledger_info.hash;
	ar & ledger_info.txHash;
	ar & ledger_info.accountHash;
	ar & ledger_info.parentHash;
	ar & ledger_info.drops;
	uint32_t closeTimeResolution;
	ar & closeTimeResolution;
	ledger_info.closeTimeResolution = NetClock::duration{closeTimeResolution};
	ar & ledger_info.closeTime;
}
RIPPE_SERIALIZATION_SPLIT_FREE(ripple::LedgerInfo);

} // namespace ripple

#endif // RIPPLE_SERIALIZATION_LEDGERINFO_H