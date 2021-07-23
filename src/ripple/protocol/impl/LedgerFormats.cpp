//------------------------------------------------------------------------------
/*
    This file is part of rippled: https://github.com/ripple/rippled
    Copyright (c) 2012, 2013 Ripple Labs Inc.

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

#include <ripple/protocol/ErrorCodes.h>
#include <ripple/protocol/LedgerFormats.h>
#include <ripple/protocol/jss.h>
#include <algorithm>
#include <array>
#include <utility>

namespace ripple {

LedgerFormats::LedgerFormats()
{
    // Fields shared by all ledger formats:
    static const std::initializer_list<SOElement> commonFields
    {
        { sfLedgerIndex,             soeOPTIONAL },
        { sfLedgerEntryType,         soeREQUIRED },
        { sfFlags,                   soeREQUIRED },
    };

    add (jss::AccountRoot, ltACCOUNT_ROOT,
        {
            { sfAccount,             soeREQUIRED },
            { sfSequence,            soeREQUIRED },
            { sfBalance,             soeREQUIRED },
            { sfOwnerCount,          soeREQUIRED },
            { sfPreviousTxnID,       soeREQUIRED },
            { sfPreviousTxnLgrSeq,   soeREQUIRED },
            { sfAccountTxnID,        soeOPTIONAL },
            { sfRegularKey,          soeOPTIONAL },
            { sfEmailHash,           soeOPTIONAL },
            { sfWalletLocator,       soeOPTIONAL },
            { sfWalletSize,          soeOPTIONAL },
            { sfMessageKey,          soeOPTIONAL },
            { sfTransferRate,        soeOPTIONAL },
            { sfTransferFeeMin,      soeOPTIONAL },
            { sfTransferFeeMax,      soeOPTIONAL },
            { sfDomain,              soeOPTIONAL },
            { sfTickSize,            soeOPTIONAL },
			{ sfMemos,				 soeOPTIONAL },
			{ sfStorageOverlay,		 soeOPTIONAL },
			{ sfContractCode,		 soeOPTIONAL },
            { sfWhiteLists,          soeOPTIONAL },
        },
        commonFields);

    add (jss::DirectoryNode, ltDIR_NODE,
        {
            { sfOwner,               soeOPTIONAL },  // for owner directories
            { sfTakerPaysCurrency,   soeOPTIONAL },  // for order book directories
            { sfTakerPaysIssuer,     soeOPTIONAL },  // for order book directories
            { sfTakerGetsCurrency,   soeOPTIONAL },  // for order book directories
            { sfTakerGetsIssuer,     soeOPTIONAL },  // for order book directories
            { sfExchangeRate,        soeOPTIONAL },  // for order book directories
            { sfIndexes,             soeREQUIRED },
            { sfRootIndex,           soeREQUIRED },
            { sfIndexNext,           soeOPTIONAL },
            { sfIndexPrevious,       soeOPTIONAL },
        },
        commonFields);

    add (jss::Offer, ltOFFER,
        {
            { sfAccount,             soeREQUIRED },
            { sfSequence,            soeREQUIRED },
            { sfTakerPays,           soeREQUIRED },
            { sfTakerGets,           soeREQUIRED },
            { sfBookDirectory,       soeREQUIRED },
            { sfBookNode,            soeREQUIRED },
            { sfOwnerNode,           soeREQUIRED },
            { sfPreviousTxnID,       soeREQUIRED },
            { sfPreviousTxnLgrSeq,   soeREQUIRED },
            { sfExpiration,          soeOPTIONAL },
        },
        commonFields);

    add (jss::RippleState, ltRIPPLE_STATE,
        {
            { sfBalance,             soeREQUIRED },
            { sfLowLimit,            soeREQUIRED },
            { sfHighLimit,           soeREQUIRED },
            { sfPreviousTxnID,       soeREQUIRED },
            { sfPreviousTxnLgrSeq,   soeREQUIRED },
            { sfLowNode,             soeOPTIONAL },
            { sfLowQualityIn,        soeOPTIONAL },
            { sfLowQualityOut,       soeOPTIONAL },
            { sfHighNode,            soeOPTIONAL },
            { sfHighQualityIn,       soeOPTIONAL },
			{ sfHighQualityOut,      soeOPTIONAL },
			{ sfMemos,				 soeOPTIONAL },
        },
        commonFields);

    add (jss::Escrow, ltESCROW,
        {
            { sfAccount,             soeREQUIRED },
            { sfDestination,         soeREQUIRED },
            { sfAmount,              soeREQUIRED },
            { sfCondition,           soeOPTIONAL },
            { sfCancelAfter,         soeOPTIONAL },
            { sfFinishAfter,         soeOPTIONAL },
            { sfSourceTag,           soeOPTIONAL },
            { sfDestinationTag,      soeOPTIONAL },
            { sfOwnerNode,           soeREQUIRED },
            { sfPreviousTxnID,       soeREQUIRED },
            { sfPreviousTxnLgrSeq,   soeREQUIRED },
			{ sfDestinationNode,     soeOPTIONAL },
			{ sfIssuerNode,			 soeOPTIONAL },
        },
        commonFields);

    add (jss::LedgerHashes, ltLEDGER_HASHES,
        {
            { sfFirstLedgerSequence, soeOPTIONAL }, // Remove if we do a ledger restart
            { sfLastLedgerSequence,  soeOPTIONAL },
            { sfHashes,              soeREQUIRED },
        },
        commonFields);

    add (jss::Amendments, ltAMENDMENTS,
        {
            { sfAmendments,          soeOPTIONAL }, // Enabled
            { sfMajorities,          soeOPTIONAL },
        },
        commonFields);

    add (jss::FeeSettings, ltFEE_SETTINGS,
        {
            { sfBaseFee,             soeREQUIRED },
            { sfReferenceFeeUnits,   soeREQUIRED },
            { sfReserveBase,         soeREQUIRED },
			{ sfReserveIncrement,    soeREQUIRED },
			{ sfDropsPerByte,		 soeOPTIONAL },
            { sfGasPrice,            soeOPTIONAL },
        },
        commonFields);

    add (jss::Ticket, ltTICKET,
        {
            { sfAccount,             soeREQUIRED },
            { sfSequence,            soeREQUIRED },
            { sfOwnerNode,           soeREQUIRED },
            { sfTarget,              soeOPTIONAL },
            { sfExpiration,          soeOPTIONAL },
        },
        commonFields);

    // All fields are soeREQUIRED because there is always a
    // SignerEntries.  If there are no SignerEntries the node is deleted.
    add(jss::SignerList,
        ltSIGNER_LIST,
        {
            {sfOwnerNode, soeREQUIRED},
            {sfSignerQuorum, soeREQUIRED},
            {sfSignerEntries, soeREQUIRED},
            {sfSignerListID, soeREQUIRED},
            {sfPreviousTxnID, soeREQUIRED},
            {sfPreviousTxnLgrSeq, soeREQUIRED},
        },
        commonFields);

    add(jss::PayChannel,
        ltPAYCHAN,
        {
            {sfAccount, soeREQUIRED},
            {sfDestination, soeREQUIRED},
            {sfAmount, soeREQUIRED},
            {sfBalance, soeREQUIRED},
            {sfPublicKey, soeREQUIRED},
            {sfSettleDelay, soeREQUIRED},
            {sfExpiration, soeOPTIONAL},
            {sfCancelAfter, soeOPTIONAL},
            {sfSourceTag, soeOPTIONAL},
            {sfDestinationTag, soeOPTIONAL},
            {sfOwnerNode, soeREQUIRED},
            {sfPreviousTxnID, soeREQUIRED},
            {sfPreviousTxnLgrSeq, soeREQUIRED},
            {sfDestinationNode, soeOPTIONAL},
        },
        commonFields);

    add(jss::Check,
        ltCHECK,
        {
            {sfAccount, soeREQUIRED},
            {sfDestination, soeREQUIRED},
            {sfSendMax, soeREQUIRED},
            {sfSequence, soeREQUIRED},
            {sfOwnerNode, soeREQUIRED},
            {sfDestinationNode, soeREQUIRED},
            {sfExpiration, soeOPTIONAL},
            {sfInvoiceID, soeOPTIONAL},
            {sfSourceTag, soeOPTIONAL},
            {sfDestinationTag, soeOPTIONAL},
            {sfPreviousTxnID, soeREQUIRED},
            {sfPreviousTxnLgrSeq, soeREQUIRED},
        },
        commonFields);

    add(jss::DepositPreauth,
        ltDEPOSIT_PREAUTH,
        {
            {sfAccount, soeREQUIRED},
            {sfAuthorize, soeREQUIRED},
            {sfOwnerNode, soeREQUIRED},
            {sfPreviousTxnID, soeREQUIRED},
            {sfPreviousTxnLgrSeq, soeREQUIRED},
        },
        commonFields);

    add(jss::NegativeUNL,
        ltNEGATIVE_UNL,
        {
            {sfDisabledValidators, soeOPTIONAL},
            {sfValidatorToDisable, soeOPTIONAL},
            {sfValidatorToReEnable, soeOPTIONAL},
        },
        commonFields);
        
    add(jss::TableList, ltTABLELIST,
		{
			{ sfOwnerNode,          soeREQUIRED },
			{ sfPreviousTxnID,      soeREQUIRED },
			{ sfPreviousTxnLgrSeq,	soeREQUIRED },
			{ sfTableEntries,       soeREQUIRED },
			{ sfFutureTxHash,		soeOPTIONAL },
		},
		commonFields);

	add(jss::InsertLimt, ltINSERTMAP,
		{
			{ sfInsertCountMap,    soeREQUIRED },
		},
		commonFields);

	add(jss::ChainId, ltCHAINID,
		{
			{ sfChainId,          soeREQUIRED },
		},
		commonFields);

	add(jss::Schema, ltSCHEMA,
		{
			{ sfAccount,             soeREQUIRED },
			{ sfSchemaName,          soeREQUIRED },
			{ sfSchemaStrategy,      soeREQUIRED },
			{ sfAnchorLedgerHash,    soeOPTIONAL },
			{ sfSchemaAdmin,		 soeOPTIONAL },
			{ sfValidators,          soeREQUIRED },
			{ sfPeerList,            soeREQUIRED },
			{ sfOwnerNode,           soeREQUIRED },
		}, commonFields);
}

LedgerFormats const&
LedgerFormats::getInstance()
{
    static LedgerFormats instance;
    return instance;
}

}  // namespace ripple
