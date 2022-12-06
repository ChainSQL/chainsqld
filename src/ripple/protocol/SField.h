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

#ifndef RIPPLE_PROTOCOL_SFIELD_H_INCLUDED
#define RIPPLE_PROTOCOL_SFIELD_H_INCLUDED

#include <ripple/basics/safe_cast.h>
#include <ripple/json/json_value.h>
#include <cstdint>
#include <map>
#include <utility>

namespace ripple {

/*

Some fields have a different meaning for their
    default value versus not present.
        Example:
            QualityIn on a TrustLine

*/

//------------------------------------------------------------------------------

// Forwards
class STAccount;
class STEntry;
class STAmount;
class STBlob;
template <std::size_t>
class STBitString;
template <class>
class STInteger;
class STVector256;
class STMap256;

enum SerializedTypeID {
    // special types
    STI_UNKNOWN = -2,
    STI_DONE = -1,
    STI_NOTPRESENT = 0,

    // // types (common)
    STI_UINT16 = 1,
    STI_UINT32 = 2,
    STI_UINT64 = 3,
    STI_HASH128 = 4,
    STI_HASH256 = 5,
    STI_AMOUNT = 6,
    STI_VL = 7,
    STI_ACCOUNT = 8,

    STI_ENTRY = 9,
    // 9-13 are reserved
    STI_OBJECT = 14,
    STI_ARRAY = 15,

    // types (uncommon)
    STI_UINT8 = 16,
    STI_HASH160 = 17,
    STI_PATHSET = 18,
    STI_VECTOR256 = 19,
	STI_MAP256 = 29,

    // high level types
    // cannot be serialized inside other types
    STI_TRANSACTION = 10001,
    STI_LEDGERENTRY = 10002,
    STI_VALIDATION  = 10003,
    STI_METADATA    = 10004,

    STI_PROPOSESET  = 10005,
    STI_VIEWCHANGE  = 10006,
    STI_PROPOSAL    = 10007,
    STI_VOTE        = 10008,
    STI_INITANNOUNCE= 10009,
    STI_EPOCHCHANGE = 10010,
    STI_VALIDATIONSET = 10011
};

// constexpr
inline int
field_code(SerializedTypeID id, int index)
{
    return (safe_cast<int>(id) << 16) | index;
}

// constexpr
inline int
field_code(int id, int index)
{
    return (id << 16) | index;
}

/** Identifies fields.

    Fields are necessary to tag data in signed transactions so that
    the binary format of the transaction can be canonicalized.  All
    SFields are created at compile time.

    Each SField, once constructed, lives until program termination, and there
    is only one instance per fieldType/fieldValue pair which serves the entire
    application.
*/
class SField
{
public:
    enum {
        sMD_Never = 0x00,
        sMD_ChangeOrig = 0x01,   // original value when it changes
        sMD_ChangeNew = 0x02,    // new value when it changes
        sMD_DeleteFinal = 0x04,  // final value when it is deleted
        sMD_Create = 0x08,       // value when it's created
        sMD_Always = 0x10,  // value when node containing it is affected at all
        sMD_Default =
            sMD_ChangeOrig | sMD_ChangeNew | sMD_DeleteFinal | sMD_Create
    };

    enum class IsSigning : unsigned char { no, yes };
    static IsSigning const notSigning = IsSigning::no;

    int const fieldCode;               // (type<<16)|index
    SerializedTypeID const fieldType;  // STI_*
    int const fieldValue;              // Code number for protocol
    std::string const fieldName;
    int const fieldMeta;
    int const fieldNum;
    IsSigning const signingField;
    Json::StaticString const jsonName;

    SField(SField const&) = delete;
    SField&
    operator=(SField const&) = delete;
    SField(SField&&) = delete;
    SField&
    operator=(SField&&) = delete;

public:
    struct private_access_tag_t;  // public, but still an implementation detail

    // These constructors can only be called from SField.cpp
    SField(
        private_access_tag_t,
        SerializedTypeID tid,
        int fv,
        const char* fn,
        int meta = sMD_Default,
        IsSigning signing = IsSigning::yes);
	SField(SerializedTypeID tid, int fv,
		const char* fn, int meta = sMD_Default,
		IsSigning signing = IsSigning::yes);
    explicit SField(private_access_tag_t, int fc);

    static const SField&
    getField(int fieldCode);
    static const SField&
    getField(std::string const& fieldName);
    static const SField&
    getField(int type, int value)
    {
        return getField(field_code(type, value));
    }

    static const SField&
    getField(SerializedTypeID type, int value)
    {
        return getField(field_code(type, value));
    }

    std::string const&
    getName() const
    {
        return fieldName;
    }

    bool
    hasName() const
    {
        return fieldCode > 0;
    }

    Json::StaticString const&
    getJsonName() const
    {
        return jsonName;
    }

    bool
    isGeneric() const
    {
        return fieldCode == 0;
    }
    bool
    isInvalid() const
    {
        return fieldCode == -1;
    }
    bool
    isUseful() const
    {
        return fieldCode > 0;
    }
    bool
    isKnown() const
    {
        return fieldType != STI_UNKNOWN;
    }
    bool
    isBinary() const
    {
        return fieldValue < 256;
    }

    // A discardable field is one that cannot be serialized, and
    // should be discarded during serialization,like 'hash'.
    // You cannot serialize an object's hash inside that object,
    // but you can have it in the JSON representation.
    bool
    isDiscardable() const
    {
        return fieldValue > 256;
    }

    int
    getCode() const
    {
        return fieldCode;
    }
    int
    getNum() const
    {
        return fieldNum;
    }
    static int
    getNumFields()
    {
        return num;
    }

    bool
    isSigningField() const
    {
        return signingField == IsSigning::yes;
    }
    bool
    shouldMeta(int c) const
    {
        return (fieldMeta & c) != 0;
    }

    bool
    shouldInclude(bool withSigningField) const
    {
        return (fieldValue < 256) &&
            (withSigningField || (signingField == IsSigning::yes));
    }

    bool
    operator==(const SField& f) const
    {
        return fieldCode == f.fieldCode;
    }

    bool
    operator!=(const SField& f) const
    {
        return fieldCode != f.fieldCode;
    }

    static int
    compare(const SField& f1, const SField& f2);

private:
    static int num;
    static std::map<int, SField const*> knownCodeToField;
};

/** A field with a type known at compile time. */
template <class T>
struct TypedField : SField
{
    using type = T;

    template <class... Args>
    explicit TypedField(Args&&... args) : SField(std::forward<Args>(args)...)
    {
    }

    TypedField(TypedField&& u) : SField(std::move(u))
    {
    }
};

/** Indicate boost::optional field semantics. */
template <class T>
struct OptionaledField
{
    TypedField<T> const* f;

    explicit OptionaledField(TypedField<T> const& f_) : f(&f_)
    {
    }
};

template <class T>
inline OptionaledField<T>
operator~(TypedField<T> const& f)
{
    return OptionaledField<T>(f);
}

//------------------------------------------------------------------------------

//------------------------------------------------------------------------------

using SF_U8 = TypedField<STInteger<std::uint8_t>>;
using SF_U16 = TypedField<STInteger<std::uint16_t>>;
using SF_U32 = TypedField<STInteger<std::uint32_t>>;
using SF_U64 = TypedField<STInteger<std::uint64_t>>;
using SF_U128 = TypedField<STBitString<128>>;
using SF_U160 = TypedField<STBitString<160>>;
using SF_U256 = TypedField<STBitString<256>>;
using SF_Account = TypedField<STAccount>;
using SF_Entry = TypedField<STEntry>;
using SF_Amount = TypedField<STAmount>;
using SF_Blob = TypedField<STBlob>;
using SF_Vec256 = TypedField<STVector256>;
using SF_Map256 = TypedField<STMap256>;

//------------------------------------------------------------------------------

extern SField const sfInvalid;
extern SField const sfGeneric;
extern SField const sfLedgerEntry;
extern SField const sfTransaction;
extern SField const sfValidation;
extern SField const sfValidationSet;
extern SField const sfMetadata;
extern SField const sfProposeSet;
extern SField const sfViewChange;
extern SField const sfProposal;
extern SField const sfVote;
extern SField const sfInitAnnounce;
extern SField const sfEpochChange;

// 8-bit integers
extern SF_U8 const sfCloseResolution;
extern SF_U8 const sfMethod;
extern SF_U8 const sfTransactionResultOld;
extern SF_U8 const sfTickSize;
extern SF_U8 const sfSchemaStrategy;
extern SF_U8 const sfSigned;
extern SF_U8 const sfUNLModifyDisabling;

// 16-bit integers
extern SF_U16 const sfLedgerEntryType;
extern SF_U16 const sfTransactionType;
extern SF_U16 const sfSignerWeight;
extern SF_U16 const sfTransactionResult;
// 16-bit integers (uncommon)
extern SF_U16 const sfVersion;
extern SF_U16 const sfOpType;
extern SF_U16 const sfContractOpType;

// 16-bit integers (uncommon)
extern SF_U16 const sfVersion;

// 32-bit integers (common)
extern SF_U32 const sfFlags;
extern SF_U32 const sfSourceTag;
extern SF_U32 const sfSequence;
extern SF_U32 const sfPreviousTxnLgrSeq;
extern SF_U32 const sfLedgerSequence;
extern SF_U32 const sfCloseTime;
extern SF_U32 const sfParentCloseTime;
extern SF_U32 const sfSigningTime;
extern SF_U32 const sfExpiration;
extern SF_U32 const sfTransferRate;
extern SF_U32 const sfWalletSize;
extern SF_U32 const sfOwnerCount;
extern SF_U32 const sfDestinationTag;
extern SF_U32 const sfNeedVerify;
extern SF_U32 const sfTxnLgrSeq;
extern SF_U32 const sfCreateLgrSeq;

// 32-bit integers (uncommon)
extern SF_U32 const sfHighQualityIn;
extern SF_U32 const sfHighQualityOut;
extern SF_U32 const sfLowQualityIn;
extern SF_U32 const sfLowQualityOut;
extern SF_U32 const sfQualityIn;
extern SF_U32 const sfQualityOut;
extern SF_U32 const sfStampEscrow;
extern SF_U32 const sfBondAmount;
extern SF_U32 const sfLoadFee;
extern SF_U32 const sfOfferSequence;
extern SF_U32 const sfFirstLedgerSequence;
extern SF_U32 const sfLastLedgerSequence;
extern SF_U32 const sfTransactionIndex;
extern SF_U32 const sfOperationLimit;
extern SF_U32 const sfReferenceFeeUnits;
extern SF_U32 const sfReserveBase;
extern SF_U32 const sfReserveIncrement;
extern SF_U32 const sfSetFlag;
extern SF_U32 const sfClearFlag;
extern SF_U32 const sfSignerQuorum;
extern SF_U32 const sfCancelAfter;
extern SF_U32 const sfFinishAfter;
extern SF_U32 const sfSignerListID;
extern SF_U32 const sfSettleDelay;
extern SF_U32 const sfGas;
extern SF_U32 const sfContractCreateCountField;
extern SF_U32 const sfContractCallCountField;
extern SF_U32 const sfTxSuccessCountField;
extern SF_U32 const sfTxFailureCountField;
extern SF_U32 const sfAccountCountField;
extern SF_U32 const sfValidatedSequence;
// 64-bit integers
extern SF_U64 const sfIndexNext;
extern SF_U64 const sfIndexPrevious;
extern SF_U64 const sfBookNode;
extern SF_U64 const sfOwnerNode;
extern SF_U64 const sfBaseFee;
extern SF_U64 const sfExchangeRate;
extern SF_U64 const sfLowNode;
extern SF_U64 const sfHighNode;
extern SF_U64 const sfIssuerNode;
extern SF_U64 const sfDestinationNode;
extern SF_U64 const sfCookie;
extern SF_U64 const sfDropsPerByte;
extern SF_U64 const sfServerVersion;
extern SF_U64 const sfView;
extern SF_U64 const sfGasPrice;

// 128-bit
extern SF_U128 const sfEmailHash;
extern SF_U160 const sfNameInDB;
extern SF_U64 const sfDestinationNode;

// 160-bit (common)
extern SF_U160 const sfTakerPaysCurrency;
extern SF_U160 const sfTakerPaysIssuer;
extern SF_U160 const sfTakerGetsCurrency;
extern SF_U160 const sfTakerGetsIssuer;

// 256-bit (common)
extern SF_U256 const sfLedgerHash;
extern SF_U256 const sfParentHash;
extern SF_U256 const sfTransactionHash;
extern SF_U256 const sfAccountHash;
extern SF_U256 const sfPreviousTxnID;
extern SF_U256 const sfLedgerIndex;
extern SF_U256 const sfWalletLocator;
extern SF_U256 const sfRootIndex;
extern SF_U256 const sfAccountTxnID;
extern SF_U256 const sfPrevTxnLedgerHash;
extern SF_U256 const sfTxnLedgerHash;
extern SF_U256 const sfCreatedLedgerHash;
extern SF_U256 const sfCreatedTxnHash;
extern SF_U256 const sfCurTxHash;
extern SF_U256 const sfFutureTxHash;
extern SF_U256 const sfChainId;
extern SF_U256 const sfAnchorLedgerHash;

// 256-bit (uncommon)
extern SF_U256 const sfBookDirectory;
extern SF_U256 const sfInvoiceID;
extern SF_U256 const sfNickname;
extern SF_U256 const sfAmendment;
extern SF_U256 const sfTicketID;
extern SF_U256 const sfDigest;
extern SF_U256 const sfPayChannel;
extern SF_U256 const sfTxCheckHash;
extern SF_U256 const sfConsensusHash;
extern SF_U256 const sfCheckID;
extern SF_U256 const sfSchemaID;
extern SF_U256 const sfValidatedHash;

// currency amount (common)
extern SF_Amount const sfAmount;
extern SF_Amount const sfBalance;
extern SF_Amount const sfLimitAmount;
extern SF_Amount const sfTakerPays;
extern SF_Amount const sfTakerGets;
extern SF_Amount const sfLowLimit;
extern SF_Amount const sfHighLimit;
extern SF_Amount const sfFee;
extern SF_Amount const sfSendMax;
extern SF_Amount const sfDeliverMin;

// currency amount (uncommon)
extern SF_Amount const sfMinimumOffer;
extern SF_Amount const sfRippleEscrow;
extern SF_Amount const sfDeliveredAmount;
extern SF_Amount const sfContractValue;

// variable length (common)
extern SF_Blob const sfPublicKey;
extern SF_Blob const sfMessageKey;
extern SF_Blob const sfSigningPubKey;
extern SF_Blob const sfTxnSignature;
extern SF_Blob const sfSignature;
extern SF_Blob const sfDomain;
extern SF_Blob const sfFundCode;
extern SF_Blob const sfRemoveCode;
extern SF_Blob const sfExpireCode;
extern SF_Blob const sfCreateCode;
extern SF_Blob const sfMemoType;
extern SF_Blob const sfMemoData;
extern SF_Blob const sfMemoFormat;
extern SF_Blob const sfTableNewName;
extern SF_Blob const sfTableName;
extern SF_Blob const sfRaw;
extern SF_Blob const sfAutoFillField;
extern SF_Blob const sfToken;
extern SF_Blob const sfStatements;
extern SF_Blob const sfOperationRule;
extern SF_Blob const sfInsertRule;
extern SF_Blob const sfUpdateRule;
extern SF_Blob const sfDeleteRule;
extern SF_Blob const sfGetRule;
extern SF_Blob const sfTxOperateRule;
extern SF_Blob const sfInsertCountMap;
extern SF_Blob const sfContractTxs;
extern SF_Blob const sfContractLogs;
extern SF_Blob const sfBlock;
extern SF_Blob const sfVoteImp;
extern SF_Blob const sfEpochChangeImp;
extern SF_Blob const sfSyncInfo;
extern SF_Blob const sfContractDetailMsg;
extern SF_Blob const sfTxsHashFillField;
extern SF_Blob const sfLedgerSeqField;
extern SF_Blob const sfLedgerTimeField;

// variable length (uncommon)
extern SF_Blob const sfFulfillment;
extern SF_Blob const sfCondition;
extern SF_Blob const sfMasterSignature;
extern SF_Blob const sfTransferFeeMin;
extern SF_Blob const sfTransferFeeMax;
extern SF_Blob const sfContractCode;
extern SF_Blob const sfContractData;
extern SF_Blob const sfSchemaName;
extern SF_Blob const sfEndpoint;
extern SF_Blob const sfUNLModifyValidator;
extern SF_Blob const sfValidatorToDisable;
extern SF_Blob const sfValidatorToReEnable;

// account
extern SF_Account const sfAccount;
extern SF_Account const sfOwner;
extern SF_Account const sfDestination;
extern SF_Account const sfIssuer;
extern SF_Account const sfAuthorize;
extern SF_Account const sfUnauthorize;
extern SF_Account const sfTarget;
extern SF_Account const sfRegularKey;
extern SF_Account const sfUser;
extern SF_Account const sfOriginalAddress;
extern SF_Account const sfContractAddress;
extern SF_Account const sfSchemaAdmin;

// Table Entry
extern SF_Entry const sfEntry;

// path set
extern SField const sfPaths;

// vector of 256-bit
extern SF_Vec256 const sfIndexes;
extern SF_Vec256 const sfHashes;
extern SF_Vec256 const sfAmendments;
extern SF_Vec256 const sfSchemaIndexes;

// map of 256-bit
extern SF_Map256 const sfStorageOverlay;
extern SF_Map256 const sfStorageExtension;

// inner object
// OBJECT/1 is reserved for end of object
extern SField const sfTransactionMetaData;
extern SField const sfCreatedNode;
extern SField const sfDeletedNode;
extern SField const sfModifiedNode;
extern SField const sfPreviousFields;
extern SField const sfFinalFields;
extern SField const sfNewFields;
extern SField const sfTemplateEntry;
extern SField const sfMemo;
extern SField const sfSignerEntry;
extern SField const sfTable;
extern SField const sfSigner;
extern SField const sfMajority;
extern SField const sfRules;
extern SField const sfValidator;
extern SField const sfPeer;
extern SField const sfDisabledValidator;
extern SField const sfWhiteList;
extern SField const sfFrozen;
extern SField const sfTableEntry;

    // array of objects
// ARRAY/1 is reserved for end of array
// extern SField const sfSigningAccounts;  // Never been used.
extern SField const sfSigners;
extern SField const sfSignerEntries;
extern SField const sfTemplate;
extern SField const sfNecessary;
extern SField const sfSufficient;
extern SField const sfAffectedNodes;
extern SField const sfMemos;
extern SField const sfMajorities;
extern SField const sfTableEntries;
extern SField const sfTables;
extern SField const sfUsers;
extern SField const sfValidators;
extern SField const sfPeerList;
extern SField const sfTransactions;
extern SField const sfWhiteLists;
extern SField const sfFrozenAccounts;
extern SField const sfValidations;

// certificate
extern SF_Blob const sfCertificate;

extern SField const sfDisabledValidators;
//------------------------------------------------------------------------------

}  // namespace ripple

#endif
