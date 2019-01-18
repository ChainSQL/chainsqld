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

#include <BeastConfig.h>
#include <ripple/protocol/TER.h>
#include <boost/range/adaptor/transformed.hpp>
#include <unordered_map>
#include <type_traits>

namespace ripple {

namespace detail {

static
std::unordered_map<
    std::underlying_type_t<TER>,
    std::pair<char const* const, char const* const>> const&
transResults()
{
    static
    std::unordered_map<
        std::underlying_type_t<TER>,
        std::pair<char const* const, char const* const>> const
    results
    {
        { tecCLAIM,                  { "tecCLAIM",                 "Fee claimed. Sequence used. No action."                                        } },
        { tecDIR_FULL,               { "tecDIR_FULL",              "Can not add entry to full directory."                                          } },
        { tecFAILED_PROCESSING,      { "tecFAILED_PROCESSING",     "Failed to correctly process transaction."                                      } },
        { tecINSUF_RESERVE_LINE,     { "tecINSUF_RESERVE_LINE",    "Insufficient reserve to add trust line."                                       } },
        { tecINSUF_RESERVE_OFFER,    { "tecINSUF_RESERVE_OFFER",   "Insufficient reserve to create offer."                                         } },
        { tecNO_DST,                 { "tecNO_DST",                "Destination does not exist. Send ZXC to create it."                            } },
        { tecNO_DST_INSUF_ZXC,       { "tecNO_DST_INSUF_ZXC",      "Destination does not exist. Too little ZXC sent to create it."                 } },
        { tecNO_LINE_INSUF_RESERVE,  { "tecNO_LINE_INSUF_RESERVE", "No such line. Too little reserve to create it."                                } },
        { tecNO_LINE_REDUNDANT,      { "tecNO_LINE_REDUNDANT",     "Can't set non-existent line to default."                                       } },
        { tecPATH_DRY,               { "tecPATH_DRY",              "Path could not send partial amount."                                           } },
        { tecPATH_PARTIAL,           { "tecPATH_PARTIAL",          "Path could not send full amount."                                              } },
        { tecNO_ALTERNATIVE_KEY,     { "tecNO_ALTERNATIVE_KEY",    "The operation would remove the ability to sign transactions with the account." } },
        { tecNO_REGULAR_KEY,         { "tecNO_REGULAR_KEY",        "Regular key is not set."                                                       } },
        { tecOVERSIZE,               { "tecOVERSIZE",              "Object exceeded serialization limits."                                         } },
        { tecUNFUNDED,               { "tecUNFUNDED",              "One of _ADD, _OFFER, or _SEND. Deprecated."                                    } },
        { tecUNFUNDED_ADD,           { "tecUNFUNDED_ADD",          "Insufficient ZXC balance for WalletAdd."                                       } },
        { tecUNFUNDED_OFFER,         { "tecUNFUNDED_OFFER",        "Insufficient balance to fund created offer."                                   } },
        { tecUNFUNDED_PAYMENT,       { "tecUNFUNDED_PAYMENT",      "Insufficient ZXC balance to send."                                             } },
		{ tecUNFUNDED_ESCROW,		 { "tecUNFUNDED_ESCROW",       "Insufficient balance to create escrow."											   } },
        { tecOWNERS,                 { "tecOWNERS",                "Non-zero owner count."                                                         } },
        { tecNO_ISSUER,              { "tecNO_ISSUER",             "Issuer account does not exist."                                                } },
        { tecNO_AUTH,                { "tecNO_AUTH",               "Not authorized to hold asset."                                                 } },
        { tecNO_LINE,                { "tecNO_LINE",               "No such line."                                                                 } },
        { tecINSUFF_FEE,             { "tecINSUFF_FEE",            "Insufficient balance to pay fee."                                              } },
        { tecFROZEN,                 { "tecFROZEN",                "Asset is frozen."                                                              } },
        { tecNO_TARGET,              { "tecNO_TARGET",             "Target account does not exist."                                                } },
        { tecNO_PERMISSION,          { "tecNO_PERMISSION",         "No permission to perform requested operation."                                 } },
        { tecNO_ENTRY,               { "tecNO_ENTRY",              "No matching entry found."                                                      } },
        { tecINSUFFICIENT_RESERVE,   { "tecINSUFFICIENT_RESERVE",  "Insufficient reserve to complete requested operation."                         } },
		{ tefTABLE_GRANTFULL,		 { "tefTABLE_GRANTFULL",	   "A table can only grant 500 uses."												} },
		{ tefTABLE_COUNTFULL,		 { "tefTABLE_COUNTFULL",	   "One account can own at most 100 tables,now you are creating the 101 one."		} },
        { tecNEED_MASTER_KEY,        { "tecNEED_MASTER_KEY",       "The operation requires the use of the Master Key."                             } },
		{ tecDST_TAG_NEEDED, { "tecDST_TAG_NEEDED",        "A destination tag is required." } },
		{ tecINTERNAL,               { "tecINTERNAL",              "An internal error has occurred during processing."                             } },
		{ tecCRYPTOCONDITION_ERROR,  { "tecCRYPTOCONDITION_ERROR", "Malformed, invalid, or mismatched conditional or fulfillment."                 } },
		{ tecINVARIANT_FAILED,		 { "tecINVARIANT_FAILED",      "One or more invariants for the transaction were not satisfied."				   } },
		{ tefALREADY,                { "tefALREADY",               "The exact transaction was already in this ledger."                             } },
		{ tefBAD_ADD_AUTH,           { "tefBAD_ADD_AUTH",          "Not authorized to add account."                                                } },
		{ tefBAD_AUTH,               { "tefBAD_AUTH",              "Transaction's public key is not authorized."                                   } },
		{ tefBAD_AUTH_EXIST,         { "tefBAD_AUTH_EXIST",        "Auth has been assigned" } },
		{ tefBAD_AUTH_NO,            { "tefBAD_AUTH_NO",           "Current user doesn't have this auth" } },
		{ tefBAD_LEDGER,             { "tefBAD_LEDGER",            "Ledger in unexpected state."                                                   } },
		{ tefBAD_QUORUM,             { "tefBAD_QUORUM",            "Signatures provided do not meet the quorum."                                   } },
		{ tefBAD_SIGNATURE,          { "tefBAD_SIGNATURE",         "A signature is provided for a non-signer."                                     } },
		{ tefCREATED,                { "tefCREATED",               "Can't add an already created account."                                         } },
		{ tefEXCEPTION,              { "tefEXCEPTION",             "Unexpected program state."                                                     } },
		{ tefFAILURE,                { "tefFAILURE",               "Failed to apply."                                                              } },
		{ tefINTERNAL,               { "tefINTERNAL",              "Internal error."                                                               } },
		{ tefMASTER_DISABLED,        { "tefMASTER_DISABLED",       "Master key is disabled."                                                       } },
		{ tefMAX_LEDGER,             { "tefMAX_LEDGER",            "Ledger sequence too high."                                                     } },
		{ tefNO_AUTH_REQUIRED,       { "tefNO_AUTH_REQUIRED",      "Auth is not required."                                                         } },
		{ tefNOT_MULTI_SIGNING,      { "tefNOT_MULTI_SIGNING",     "Account has no appropriate list of multi-signers."                             } },
		{ tefPAST_SEQ,               { "tefPAST_SEQ",              "This sequence number has already past."                                        } },
		{ tefWRONG_PRIOR,            { "tefWRONG_PRIOR",           "This previous transaction does not match."                                     } },
		{ tefBAD_AUTH_MASTER,        { "tefBAD_AUTH_MASTER",       "Auth for unclaimed account needs correct master key."                          } },
		{ tefGAS_INSUFFICIENT,		 { "tefGAS_INSUFFICIENT",	   "Gas insufficient." } },
		{ tefCONTRACT_EXEC_EXCEPTION,{ "tefCONTRACT_EXEC_EXCEPTION","Exception occurred while executing contract ." } },
		{ tefCONTRACT_REVERT_INSTRUCTION,{ "tefCONTRACT_REVERT_INSTRUCTION","Contract reverted,maybe 'require' condition not satisfied." } },
		{ tefCONTRACT_CANNOT_BEPAYED ,{ "tefCONTRACT_CANNOT_BEPAYED","Contract address cannot be 'Destination' for 'Payment'." } },
		{ tefCONTRACT_NOT_EXIST ,	 {"tefCONTRACT_NOT_EXIST",		"Contract does not exist,maybe destructed."				}},
        
		{ telLOCAL_ERROR,            { "telLOCAL_ERROR",           "Local failure."                                                                } },
		{ telBAD_DOMAIN,             { "telBAD_DOMAIN",            "Domain too long."                                                              } },
		{ telBAD_PATH_COUNT,         { "telBAD_PATH_COUNT",        "Malformed: Too many paths."                                                    } },
		{ telBAD_PUBLIC_KEY,         { "telBAD_PUBLIC_KEY",        "Public key too long."                                                          } },
		{ telFAILED_PROCESSING,      { "telFAILED_PROCESSING",     "Failed to correctly process transaction."                                      } },
		{ telINSUF_FEE_P,            { "telINSUF_FEE_P",           "Fee insufficient."                                                             } },
		{ telNO_DST_PARTIAL,         { "telNO_DST_PARTIAL",        "Partial payment to create account not allowed."                                } },
		{ telCAN_NOT_QUEUE,          { "telCAN_NOT_QUEUE",         "Can not queue at this time."                                                   } },
        { telCAN_NOT_QUEUE_BALANCE,  { "telCAN_NOT_QUEUE_BALANCE", "Can not queue at this time: insufficient balance to pay all queued fees."      } },
        { telCAN_NOT_QUEUE_BLOCKS,   { "telCAN_NOT_QUEUE_BLOCKS",  "Can not queue at this time: would block later queued transaction(s)."          } },
        { telCAN_NOT_QUEUE_BLOCKED,  { "telCAN_NOT_QUEUE_BLOCKED", "Can not queue at this time: blocking transaction in queue."                    } },
        { telCAN_NOT_QUEUE_FEE,      { "telCAN_NOT_QUEUE_FEE",     "Can not queue at this time: fee insufficient to replace queued transaction."   } },
        { telCAN_NOT_QUEUE_FULL,     { "telCAN_NOT_QUEUE_FULL",    "Can not queue at this time: queue is full."                                    } },


		{ temMALFORMED,              { "temMALFORMED",             "Malformed transaction."                                                        } },
		{ temBAD_AMOUNT,             { "temBAD_AMOUNT",            "Can only send positive amounts."                                               } },
		{ temBAD_CURRENCY,           { "temBAD_CURRENCY",          "Malformed: Bad currency."                                                      } },
		{ temBAD_EXPIRATION,         { "temBAD_EXPIRATION",        "Malformed: Bad expiration."                                                    } },
		{ temBAD_FEE,                { "temBAD_FEE",               "Invalid fee, negative or not ZXC."                                             } },
		{ temBAD_ISSUER,             { "temBAD_ISSUER",            "Malformed: Bad issuer."                                                        } },
		{ temBAD_LIMIT,              { "temBAD_LIMIT",             "Limits must be non-negative."                                                  } },
		{ temBAD_OFFER,              { "temBAD_OFFER",             "Malformed: Bad offer."                                                         } },
		{ temBAD_PATH,               { "temBAD_PATH",              "Malformed: Bad path."                                                          } },
		{ temBAD_PATH_LOOP,          { "temBAD_PATH_LOOP",         "Malformed: Loop in path."                                                      } },
		{ temBAD_QUORUM,             { "temBAD_QUORUM",            "Malformed: Quorum is unreachable."                                             } },
		{ temBAD_SEND_ZXC_LIMIT,     { "temBAD_SEND_ZXC_LIMIT",    "Malformed: Limit quality is not allowed for ZXC to ZXC."                       } },
		{ temBAD_SEND_ZXC_MAX,       { "temBAD_SEND_ZXC_MAX",      "Malformed: Send max is not allowed for ZXC to ZXC."                            } },
		{ temBAD_SEND_ZXC_NO_DIRECT, { "temBAD_SEND_ZXC_NO_DIRECT","Malformed: No Ripple direct is not allowed for ZXC to ZXC."                    } },
		{ temBAD_SEND_ZXC_PARTIAL,   { "temBAD_SEND_ZXC_PARTIAL",  "Malformed: Partial payment is not allowed for ZXC to ZXC."                     } },
		{ temBAD_SEND_ZXC_PATHS,     { "temBAD_SEND_ZXC_PATHS",    "Malformed: Paths are not allowed for ZXC to ZXC."                              } },
		{ temBAD_SEQUENCE,           { "temBAD_SEQUENCE",          "Malformed: Sequence is not in the past."                                       } },
		{ temBAD_SIGNATURE,          { "temBAD_SIGNATURE",         "Malformed: Bad signature."                                                     } },
		{ temBAD_SIGNER,             { "temBAD_SIGNER",            "Malformed: No signer may duplicate account or other signers."                  } },
		{ temBAD_SRC_ACCOUNT,        { "temBAD_SRC_ACCOUNT",       "Malformed: Bad source account."                                                } },
		{ temBAD_TRANSFER_RATE,      { "temBAD_TRANSFER_RATE",     "Malformed: Transfer rate must be >= 1.0 and <= 2.0."                                       } },
		{ temBAD_TRANSFERFEE_BOTH,	 { "temBAD_TRANSFERFEE_BOTH",  "Malformed: TransferFeeMin and TransferFeeMax can not be set individually."	   } },
		{ temBAD_TRANSFERFEE,		 { "temBAD_TRANSFERFEE",	   "Malformed: TransferFeeMin or TransferMax invalid."				   } },
		{ temBAD_FEE_MISMATCH_TRANSFER_RATE,{ "temBAD_FEE_MISMATCH_TRANSFER_RATE",   "Malformed: TransferRate mismatch with TransferFeeMin or TransferFeeMax."}},
		{ temBAD_WEIGHT,             { "temBAD_WEIGHT",            "Malformed: Weight must be a positive value."                                   } },
		{ temDST_IS_SRC,             { "temDST_IS_SRC",            "Destination may not be source."                                                } },
		{ temDST_NEEDED,             { "temDST_NEEDED",            "Destination not specified."                                                    } },
		{ temINVALID,                { "temINVALID",               "The transaction is ill-formed."                                                } },
		{ temINVALID_FLAG,           { "temINVALID_FLAG",          "The transaction has an invalid flag."                                          } },
		{ temREDUNDANT,              { "temREDUNDANT",             "Sends same currency to self."                                                  } },
		{ temRIPPLE_EMPTY,           { "temRIPPLE_EMPTY",          "PathSet with no paths."                                                        } },
		{ temUNCERTAIN,              { "temUNCERTAIN",             "In process of determining result. Never returned."                             } },
		{ temUNKNOWN,                { "temUNKNOWN",               "The transaction requires logic that is not implemented yet."                   } },
		{ temDISABLED,               { "temDISABLED",              "The transaction requires logic that is currently disabled."                    } },
		{ temBAD_OWNER,              { "temBAD_OWNER",             "Malformed: Bad table owner."                                                   } },
		{ temBAD_TABLES,             { "temBAD_TABLES",            "Malformed: Bad table names."                                                   } },
		{ temBAD_TABLEFLAGS,         { "temBAD_TABLEFLAGS",        "Malformed: Bad table authority."                                               } },
		{ temBAD_RAW,                { "temBAD_RAW",               "Malformed: Bad raw sql."                                                       } },
		{ temBAD_OPTYPE,             { "temBAD_OPTYPE",            "Malformed: Bad operator type." } },
		{ temBAD_BASETX,             { "temBAD_BASETX",            "Malformed: Bad base tx check hash." } },
		{ temBAD_PUT,				 { "temBAD_PUT",               "Malformed: Bad base tx format or check hash error" } },
		{ temBAD_DBTX,               { "temBAD_DBTX",              "Malformed: Bad DBTx support."                                                  } },
		{ temBAD_STATEMENTS,         { "temBAD_STATEMENTS",        "Malformed: Bad Statements field."                                              } },
		{ temBAD_NEEDVERIFY,         { "temBAD_NEEDVERIFY",        "Malformed: Bad NeedVerify field."                                              } },
		{ temBAD_STRICTMODE,         { "temBAD_STRICTMODE",        "Malformed: Bad StrictMode support."                                            } },
		{ temBAD_BASELEDGER,         { "temBAD_LEDGER",            "Malformed: Bad base ledger sequence."                                          } },
		{ temBAD_TRANSFERORDER,      { "temBAD_TRANSFERORDER",     "Malformed: Current tx is not the one we expected." } },
		{ temBAD_OPERATIONRULE,		 { "temBAD_OPERATIONRULE",     "Malformed: Operation Rule is not valid." } },
		{ temBAD_DELETERULE,		 { "temBAD_DELETERULE",		   "Malformed: Delete rule must contains '$account' condition because of insert rule." } },
		{ temBAD_UPDATERULE,		 { "ttemBAD_UPDATERULE",	   "Malformed: Update rule is needed and 'Fields' is needed in update rule." } },
		{ temBAD_INSERTLIMIT,		 { "temBAD_INSERTLIMIT",	   "Malformed: Deal with insert count limit error." } },
		{ temBAD_RULEANDTOKEN,		 { "temBAD_RULEANDTOKEN",	   "Malformed: OperationRule and Confidential are not supported in the mean time."} },
		{ temBAD_TICK_SIZE,          { "temBAD_TICK_SIZE",         "Malformed: Tick size out of range."                                            } },
		{ temBAD_NEEDVERIFY_OPERRULE ,{ "temBAD_NEEDVERIFY_OPERRULE","Malformed: NeedVerify must be 1 if there is table has OperatinRule."                                            } },
		{ terRETRY,                  { "terRETRY",                 "Retry transaction."                                                            } },
		{ terFUNDS_SPENT,            { "terFUNDS_SPENT",           "Can't set password, password set funds already spent."                         } },
		{ terINSUF_FEE_B,            { "terINSUF_FEE_B",           "Account balance can't pay fee."                                                } },
		{ terLAST,                   { "terLAST",                  "Process last."                                                                 } },
		{ terNO_RIPPLE,              { "terNO_RIPPLE",             "Path does not permit rippling."                                                } },
		{ terNO_ACCOUNT,             { "terNO_ACCOUNT",            "The source account does not exist."                                            } },
		{ terNO_AUTH,                { "terNO_AUTH",               "Not authorized to hold IOUs."                                                  } },
		{ terNO_LINE,                { "terNO_LINE",               "No such line."                                                                 } },
		{ terPRE_SEQ,                { "terPRE_SEQ",               "Missing/inapplicable prior transaction."                                       } },
		{ terOWNERS,                 { "terOWNERS",                "Non-zero owner count."                                                         } },
		{ terQUEUED,                 { "terQUEUED",                "Held until escalated fee drops."                                               } },
		{ tefTABLE_SAMENAME,         { "tefTABLE_SAMENAME",        "Table name and table new name is same or create exist table."                                        } },
		{ tefTABLE_NOTEXIST,         { "tefTABLE_NOTEXIST",        "Table is not exist." } },
		{ tefTABLE_STATEERROR,       { "tefTABLE_STATEERROR",      "Table's state is error." } },
		{ tefBAD_USER,               { "tefBAD_USER",              "BAD User format."    } },
		{ tefTABLE_EXISTANDNOTDEL,   { "tefTABLE_EXISTANDNOTDEL",  "Table exist and not deleted." } },
		{ tefTABLE_STORAGEERROR,     { "tefTABLE_STORAGEERROR",    "Table storage error." } },
		{ tefTABLE_STORAGENORMALERROR,{ "tefTABLE_STORAGENORMALERROR",    "Table storage normal error." } },
		{ tefTABLE_TXDISPOSEERROR,	 { "tefTABLE_TXDISPOSEERROR",	"Tx Dispose error." } },
		{ tefTABLE_RULEDISSATISFIED,  { "tefTABLE_RULEDISSATISFIED",	"Operation rule not satisfied."}},        
		{ tefINSUFFICIENT_RESERVE,	 { "tefINSUFFICIENT_RESERVE",  "Insufficient reserve to complete requested operation." } },
		{ tefINSU_RESERVE_TABLE,	{ "tefINSU_RESERVE_TABLE",		"Insufficient reserve to create a table." } },
		{ tefDBNOTCONFIGURED,		 { "tefDBNOTCONFIGURED",       "DB is not connected,please checkout 'sync_db'in config file." } },
		{ tefBAD_DBNAME,			{ "tefBAD_DBNAME",            "NameInDB does not match tableName." } },
		{ tefBAD_STATEMENT,			{ "tefBAD_STATEMENT",	       "Statement is error." } },
        { tesSUCCESS,                { "tesSUCCESS",               "The transaction was applied. Only final in a validated ledger."                } },
    };
    return results;
}

}

bool transResultInfo (TER code, std::string& token, std::string& text)
{
    auto& results = detail::transResults();

    auto const r = results.find (
        static_cast<std::underlying_type_t<TER>> (code));

    if (r == results.end())
        return false;

    token = r->second.first;
    text = r->second.second;
    return true;
}

std::string transToken (TER code)
{
    std::string token;
    std::string text;

    return transResultInfo (code, token, text) ? token : "-";
}

std::string transHuman (TER code)
{
    std::string token;
    std::string text;

    return transResultInfo (code, token, text) ? text : "-";
}

boost::optional<TER>
transCode(std::string const& token)
{
    static
    auto const results = []
    {
        auto& byTer = detail::transResults();
        auto range = boost::make_iterator_range(byTer.begin(),
            byTer.end());
        auto tRange = boost::adaptors::transform(
            range,
            [](auto const& r)
            {
            return std::make_pair(r.second.first, r.first);
            }
        );
        std::unordered_map<
            std::string,
            std::underlying_type_t<TER>> const
        byToken(tRange.begin(), tRange.end());
        return byToken;
    }();

    auto const r = results.find(token);

    if (r == results.end())
        return boost::none;

    return static_cast<TER>(r->second);
}
} // ripple
