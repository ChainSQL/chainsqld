=============
错误码
=============


返回值格式
==============================
- :ref:`Rpc <RPC返回值>`
- :ref:`Websocket <Websocket返回值>`
- :ref:`Java <Java返回值>`
- :ref:`Node.js <Node.js返回值>`

普通请求错误码
===================
普通请求指的是 ``非交易类`` 请求，一般用于查询链上数据

请求返回示例
---------------------
普通请求成功返回示例(RPC):

.. code-block:: json

    {
        "result": {
            "account_data": {
                "Account": "zHb9CJAWyB4zj91VRWn96DkukG4bwdtyTh",
                "Balance": "8728498",
                "Flags": 0,
                "LedgerEntryType": "AccountRoot",
                "OwnerCount": 3,
                "PreviousTxnID": "F8E2EB473CE0355B1823F767B7E7D41AE4D41E8448151DE4B8F39929179392C6",
                "PreviousTxnLgrSeq": 4302033,
                "Sequence": 78,
                "index": "2B6AC232AA4C4BE41BF49D2459FA4A0347E1B543A4C92FCEE0821C0201E2E9A8"
            },
            "ledger_current_index": 4560272,
            "queue_data": {
                "txn_count": 0
            },
            "status": "success",
            "validated": false
        }
    }

普通请求失败返回示例(RPC):

.. code-block:: json

    {
        "result": {
            "account": "zHePXbLNmrta6nf7imgQawrQVzM8svzhHu",
            "error": "actNotFound",
            "error_code": 19,
            "error_message": "Account not found.",
            "ledger_current_index": 4560257,
            "request": {
                "account": "zHePXbLNmrta6nf7imgQawrQVzM8svzhHu",
                "command": "account_info",
                "ledger_index": "current",
                "queue": true,
                "strict": true
            },
            "status": "error",
            "validated": false
        }
    }

错误码列表：
-------------------------
| 普通请求错误码git源码: `ErrorCodes.cpp <https://github.com/ChainSQL/chainsqld/blob/master/src/ripple/protocol/impl/ErrorCodes.cpp>`_
| 错误码列表：

========================	=====================================================================
错误码 							描述
========================	=====================================================================
actBitcoin        			Account is bitcoin address	
actExists         			Account already exists	
actMalformed      			Account malformed	
actNotFound       			Account not found	
actNotMatchPublic 			Account is not match with publicKey	
amendmentBlocked  			Amendment blocked, need upgrade	
deprecated        			Use the new API or specify a ledger range	
badBlob           			Blob must be a non-empty hex string	
badFeature        			Feature unknown or invalid	
badIssuer         			Issuer account malformed	
badMarket         			No such market	
badSecret         			Secret does not match account	
badSeed           			Disallowed seed	
badSyntax         			Syntax error	
channelMalformed  			Payment channel is malformed	
channelAmtMalformed			Payment channel amount is malformed	
commandMissing    			Missing command entry	
dstActMalformed   			Destination account is malformed	
dstActMissing     			Destination account does not exist	
dstAmtMalformed   			Destination amount/currency/issuer is malformed	
dstIsrMalformed   			Destination issuer is malformed	
forbidden         			Bad credentials	
general           			Generic error reason	
getsActMalformed  			Gets account malformed	
getsAmtMalformed  			Gets amount malformed	
highFee           			Current transaction fee exceeds your limit	
hostIpMalformed   			Host IP is malformed	
insufFunds        			Insufficient funds	
internal          			Internal error	
NoDbConfig	    			Get db connection error,maybe db not configured	
invalidParams     			Invalid parameters	
json_rpc          			JSON-RPC transport error	
lgrIdxsInvalid    			Ledger indexes invalid	
lgrIdxMalformed   			Ledger index malformed	
lgrNotFound       			Ledger not found	
lgrNotValidated   			Ledger not validated	
loadFailed        			Load failed);	
masterDisabled    			Master key is disabled	
notEnabled        			Not enabled in configuration	
notImpl           			Not implemented	
notReady          			Not ready to handle this request	
notStandAlone     			Operation valid in debug mode only	
notSupported      			Operation not supported	
noAccount         			No such account	
noClosed          			Closed ledger is unavailable	
noCurrent         			Current ledger is unavailable	
noEvents          			Current transport does not support events	
noNetwork         			Not synced to Ripple network	
noPath            			Unable to find a ripple path	
noPermission      			You don't have permission for this command	
noPathRequest     			No pathfinding request in progress	
passwdChanged     			Wrong key, password changed	
paysActMalformed  			Pays account malformed	
paysAmtMalformed  			Pays amount malformed	
portMalformed     			Port is malformed	
publicMalformed   			Public key is malformed	
qualityMalformed  			Quality malformed	
signForMalformed  			Signing for account is malformed	
slowDown          			You are placing too much load on the server	
srcActMalformed   			Source account is malformed	
srcActMissing     			Source account not provided	
srcActNotFound    			Source account not found	
srcAmtMalformed   			Source amount/currency/issuer is malformed	
srcCurMalformed   			Source currency is malformed	
srcIsrMalformed   			Source issuer is malformed	
srcMissing        			Source is missing	
srcUnclaimed      			Source account is not claimed	
malformedStream   			Stream malformed	
tooBusy           			The server is too busy to help you now	
txnNotFound       			Transaction not found	
unknownCmd        			Unknown method	
wrongSeed         			The regular key does not point as the master key	
sendMaxMalformed  			SendMax amount malformed	
txJsonParsedErr   			Tx Json parsed error	
disposeSqlErr     			Dispose SQL common error info	
sqlSelectOnly     			First word of SQL must be select	
dbTypeNotSupport  			Do not support this db type	
dbConnectFailed   			Database connection is failed	
tabNotExist       			Table does not exist	
tabUnauthorized   			The user is unauthorized to the table	
rawNotValidated   			Raw field is not validated	
dBNameNotMatchTabNam		DBName is not matched with table name	
userSleTokenMissing			Missing 'Token' field in sle of the corresponding user	
signDataNotMatch  			Signing data does not match tx_json	
signNotInHex      			Signature is not in hex	
getValueInvalid   			Get value invalid from syncTableState	
getLedgerFailed   			Get validated ledger failed	
dumpGeneralError  			General error when start dump	
dumpStopGeneralError		General error when stop dump	
auditGeneralError 			General error when start audit	
auditStopGeneralError		General error when stop audit	
fieldContentEmpty			Field content is empty	
contractEVMexeError			Contract execution exception	
contractEVMcallError		Contract execution exception	
mulQueryNotSupport              OperationRule Table not support multi_table sql_query
========================	=====================================================================

.. _tx-errcode:

交易类返回码
===================
返回码分类
----------------
Ripple中对交易的返回码有专门的说明：`Transaction Results <https://developers.ripple.com/transaction-results.html>`_

一个共识过的区块中，可能包含两种结果的交易:

- tes: 成功（目前只有tesSUCCESS）
- tec: 失败，但扣除交易费用（tecPATH_PARTIAL,tecPATH_DRY等）

=========== ====================    ============== ====================================         
返回码前缀	 说明	                 最终结果        举例
=========== ====================    ============== ====================================   
tef	        failed	                yes	            tefMAX_LEDGER,tefPAST_SEQ
tem	        malformed	            yes             temBAD_RAW, temBAD_AMOUNT
tel	        local error	            yes	            telINSUF_FEE_P
tec	        claim fee only	        no	            tecPATH_PARTIAL,tecPATH_DRY
ter	        will retry	            no	            terQUEUED,terPRE_SEQ
tes	        success                 no	            tesSUCCESS
=========== ====================    ============== ====================================  

请求返回示例
---------------------
交易请求成功返回格式（RPC）：

.. code-block:: json

    {
        "result": {
            "engine_result": "tesSUCCESS",
            "engine_result_code": 0,
            "engine_result_message": "The transaction was applied. Only final in a validated ledger.",
            "status": "success",
            "tx_blob": "12000022800000002400000243201B0000B43D61400000012A05F20068400000000000000A73210330E7FC9D56BB25D6893BA3F317AE5BCF33B3291BD63DB32654A313222F7FD02074473045022100D6FD51CD1C07E5C5877AA2A6CB3279BD25D1E48C6A079A583E5BB650FEC81AFA02202FB542F31A16E23291365DB3C295367E0E284D6364609EEAD1B77D4AAE6A9A2A8114B5F762798A53D543A014CAF8B297CFF8F2F937E88314190FA18FFAEEE774D8B0B9E8A9242397A0EAE73E",
            "tx_json": {
                "Account": "zHb9CJAWyB4zj91VRWn96DkukG4bwdtyTh",
                "Amount": "5000000000",
                "Destination": "zxHWa1crijWU5qhSTGGemfFfMowaS63qJ5",
                "Fee": "10",
                "Flags": 2147483648,
                "LastLedgerSequence": 46141,
                "Sequence": 579,
                "SigningPubKey": "0330E7FC9D56BB25D6893BA3F317AE5BCF33B3291BD63DB32654A313222F7FD020",
                "TransactionType": "Payment",
                "TxnSignature": "3045022100D6FD51CD1C07E5C5877AA2A6CB3279BD25D1E48C6A079A583E5BB650FEC81AFA02202FB542F31A16E23291365DB3C295367E0E284D6364609EEAD1B77D4AAE6A9A2A",
                "hash": "8CE6EE15E23DA98064DEC224549BB7C6B9EA7034F78EC6CAF9965C3B7E9B8461"
            }
        }
    }

交易请求错误返回格式（RPC）：

.. code-block:: json

    {
        "result": {
            "engine_result": "tecNO_DST_INSUF_ZXC",
            "engine_result_code": 125,
            "engine_result_message": "Destination does not exist. Too little ZXC sent to create it.",
            "status": "success",
            "tx_blob": "12000022800000002400000244201B0000B93761400000000000138868400000000000000A73210330E7FC9D56BB25D6893BA3F317AE5BCF33B3291BD63DB32654A313222F7FD02074473045022100885DB315A21A00A043EC918DB59F1B80F542F094485E7B58FFEFE81DABD1313702205537F08AEBA89407903A2A51F61A18E1FCEAB8004484F9C483FD0AA68B5DDF568114B5F762798A53D543A014CAF8B297CFF8F2F937E88314276BBBD610BCD9BE9A7FA9DAEA49CE5B6C8D4BA4",
            "tx_json": {
                "Account": "zHb9CJAWyB4zj91VRWn96DkukG4bwdtyTh",
                "Amount": "5000",
                "Destination": "zhbSQsEK7xQswMgr6v6JSRpMjtnp3snc94",
                "Fee": "10",
                "Flags": 2147483648,
                "LastLedgerSequence": 47415,
                "Sequence": 580,
                "SigningPubKey": "0330E7FC9D56BB25D6893BA3F317AE5BCF33B3291BD63DB32654A313222F7FD020",
                "TransactionType": "Payment",
                "TxnSignature": "3045022100885DB315A21A00A043EC918DB59F1B80F542F094485E7B58FFEFE81DABD1313702205537F08AEBA89407903A2A51F61A18E1FCEAB8004484F9C483FD0AA68B5DDF56",
                "hash": "8B7ACBD85BC1B1AFD634A96CD2EA922B3462CC4F6152B1B980F93DEE0F434FE3"
            }
        }
    }

返回码列表
---------------------
| 交易请求返回码git源码: `TER.cpp <https://github.com/ChainSQL/chainsqld/blob/master/src/ripple/protocol/impl/ErrorCodes.cpp>`_
| 返回码列表：

==================================	======================================================================================
错误码 								描述
==================================	======================================================================================
tecCLAIM							Fee claimed. Sequence used. No action.                                       
tecDIR_FULL							Can not add entry to full directory.                                         
tecFAILED_PROCESSING     			Failed to correctly process transaction.                                     
tecINSUF_RESERVE_LINE    			Insufficient reserve to add trust line.                                      
tecINSUF_RESERVE_OFFER   			Insufficient reserve to create offer.                                        
tecNO_DST                			Destination does not exist. Send ZXC to create it.                           
tecNO_DST_INSUF_ZXC      			Destination does not exist. Too little ZXC sent to create it.                
tecNO_LINE_INSUF_RESERVE 			No such line. Too little reserve to create it.                               
tecNO_LINE_REDUNDANT     			Can't set non-existent line to default.                                      
tecPATH_DRY              			Path could not send partial amount.                                          
tecPATH_PARTIAL          			Path could not send full amount.                                             
tecNO_ALTERNATIVE_KEY    			The operation would remove the ability to sign transactions with the account.
tecNO_REGULAR_KEY        			Regular key is not set.                                                      
tecOVERSIZE              			Object exceeded serialization limits.                                        
tecUNFUNDED              			One of _ADD, _OFFER, or _SEND. Deprecated.                                   
tecUNFUNDED_ADD          			Insufficient ZXC balance for WalletAdd.                                      
tecUNFUNDED_OFFER        			Insufficient balance to fund created offer.                                  
tecUNFUNDED_PAYMENT      			Insufficient ZXC balance to send.                                            
tecUNFUNDED_ESCROW       			Insufficient balance to create escrow.
tecOWNERS                			Non-zero owner count.                                                        
tecNO_ISSUER             			Issuer account does not exist.                                               
tecNO_AUTH               			Not authorized to hold asset.                                                
tecNO_LINE               			No such line.                                                                
tecINSUFF_FEE            			Insufficient balance to pay fee.                                             
tecFROZEN                			Asset is frozen.                                                             
tecNO_TARGET             			Target account does not exist.                                               
tecNO_PERMISSION         			No permission to perform requested operation.                                
tecNO_ENTRY              			No matching entry found.                                                     
tecINSUFFICIENT_RESERVE  			Insufficient reserve to complete requested operation.                        
tefTABLE_GRANTFULL					A table can only grant 500 uses.
tefTABLE_COUNTFULL 					One account can own at most 100 tables,now you are creating the 101 one.
tecNEED_MASTER_KEY       			The operation requires the use of the Master Key.                            
tecDST_TAG_NEEDED        			A destination tag is required. } },
tecINTERNAL              			An internal error has occurred during processing.                            
tecCRYPTOCONDITION_ERROR 			Malformed, invalid, or mismatched conditional or fulfillment.                
tecINVARIANT_FAILED      			One or more invariants for the transaction were not satisfied.
tefALREADY               			The exact transaction was already in this ledger.                            
tefBAD_ADD_AUTH          			Not authorized to add account.                                               
tefBAD_AUTH              			Transaction's public key is not authorized.                                  
tefBAD_AUTH_EXIST        			Auth has been assigned } },
tefBAD_AUTH_NO           			Current user doesn't have this auth } },
tefBAD_LEDGER            			Ledger in unexpected state.                                                  
tefBAD_QUORUM            			Signatures provided do not meet the quorum.                                  
tefBAD_SIGNATURE         			A signature is provided for a non-signer.                                    
tefCREATED               			Can't add an already created account.                                        
tefEXCEPTION             			Unexpected program state.                                                    
tefFAILURE               			Failed to apply.                                                             
tefINTERNAL              			Internal error.                                                              
tefMASTER_DISABLED       			Master key is disabled.                                                      
tefMAX_LEDGER            			Ledger sequence too high.                                                    
tefNO_AUTH_REQUIRED      			Auth is not required.                                                        
tefNOT_MULTI_SIGNING     			Account has no appropriate list of multi-signers.                            
tefPAST_SEQ              			This sequence number has already past.                                       
tefWRONG_PRIOR           			This previous transaction does not match.                                    
tefBAD_AUTH_MASTER       			Auth for unclaimed account needs correct master key.                         
tefGAS_INSUFFICIENT					Gas insufficient. 
tefCONTRACT_EXEC_EXCEPTION			Exception occurred while executing contract . 
tefCONTRACT_REVERT_INSTRUCTION		Contract reverted,maybe 'require' condition not satisfied. 
tefCONTRACT_CANNOT_BEPAYED			Contract address cannot be 'Destination' for 'Payment'. 
tefCONTRACT_NOT_EXIST				Contract does not exist,maybe destructed.
	
telLOCAL_ERROR           			Local failure.                                                               
telBAD_DOMAIN            			Domain too long.                                                             
telBAD_PATH_COUNT        			Malformed: Too many paths.                                                   
telBAD_PUBLIC_KEY        			Public key too long.                                                         
telFAILED_PROCESSING     			Failed to correctly process transaction.                                     
telINSUF_FEE_P           			Fee insufficient.                                                            
telNO_DST_PARTIAL        			Partial payment to create account not allowed.                               
telCAN_NOT_QUEUE         			Can not queue at this time.                                                  
telCAN_NOT_QUEUE_BALANCE 			Can not queue at this time: insufficient balance to pay all queued fees.     
telCAN_NOT_QUEUE_BLOCKS  			Can not queue at this time: would block later queued transaction(s).         
telCAN_NOT_QUEUE_BLOCKED 			Can not queue at this time: blocking transaction in queue.                   
telCAN_NOT_QUEUE_FEE     			Can not queue at this time: fee insufficient to replace queued transaction.  
telCAN_NOT_QUEUE_FULL    			Can not queue at this time: queue is full.                                   

temMALFORMED             			Malformed transaction.                                                       	
temBAD_AMOUNT            			Can only send positive amounts.                                              	
temBAD_CURRENCY          			Malformed: Bad currency.                                                     	
temBAD_EXPIRATION        			Malformed: Bad expiration.                                                   	
temBAD_FEE               			Invalid fee, negative or not ZXC.                                            	
temBAD_ISSUER            			Malformed: Bad issuer.                                                       	
temBAD_LIMIT             			Limits must be non-negative.                                                 	
temBAD_OFFER             			Malformed: Bad offer.                                                        	
temBAD_PATH              			Malformed: Bad path.                                                         	
temBAD_PATH_LOOP         			Malformed: Loop in path.                                                     	
temBAD_QUORUM            			Malformed: Quorum is unreachable.                                            	
temBAD_SEND_ZXC_LIMIT    			Malformed: Limit quality is not allowed for ZXC to ZXC.                      	
temBAD_SEND_ZXC_MAX      			Malformed: Send max is not allowed for ZXC to ZXC.                           	
temBAD_SEND_ZXC_NO_DIRECT			Malformed: No Ripple direct is not allowed for ZXC to ZXC.                   	
temBAD_SEND_ZXC_PARTIAL  			Malformed: Partial payment is not allowed for ZXC to ZXC.                    	
temBAD_SEND_ZXC_PATHS    			Malformed: Paths are not allowed for ZXC to ZXC.                             	
temBAD_SEQUENCE          			Malformed: Sequence is not in the past.                                      	
temBAD_SIGNATURE         			Malformed: Bad signature.                                                    	
temBAD_SIGNER            			Malformed: No signer may duplicate account or other signers.                 	
temBAD_SRC_ACCOUNT       			Malformed: Bad source account.                                               	
temBAD_TRANSFER_RATE     			Malformed: Transfer rate must be >= 1.0 and <= 2.0.                          	
temBAD_TRANSFERFEE_BOTH  			Malformed: TransferFeeMin and TransferFeeMax can not be set individually.	   
temBAD_TRANSFERFEE					Malformed: TransferFeeMin or TransferMax invalid.	
temBAD_FEE_MISMATCH_TRANSFER_RATE	Malformed: TransferRate mismatch with TransferFeeMin or TransferFeeMax.	
temBAD_WEIGHT            			Malformed: Weight must be a positive value.                                  	
temDST_IS_SRC            			Destination may not be source.                                               	
temDST_NEEDED            			Destination not specified.                                                   	
temINVALID               			The transaction is ill-formed.                                               	
temINVALID_FLAG          			The transaction has an invalid flag.                                         	
temREDUNDANT             			Sends same currency to self.                                                 	
temRIPPLE_EMPTY          			PathSet with no paths.                                                       	
temUNCERTAIN             			In process of determining result. Never returned.                            	
temUNKNOWN               			The transaction requires logic that is not implemented yet.                  	
temDISABLED              			The transaction requires logic that is currently disabled.                   	
temBAD_OWNER             			Malformed: Bad table owner.                                                  	
temBAD_TABLES            			Malformed: Bad table names.                                                  	
temBAD_TABLEFLAGS        			Malformed: Bad table authority.                                              	
temBAD_RAW               			Malformed: Bad raw sql.                                                      	
temBAD_OPTYPE            			Malformed: Bad operator type. 	
temBAD_OPTYPE_IN_TRANSACTION		Malformed:create,drop,rename is not allowd in SqlTransaction.	
temBAD_BASETX            			Malformed: Bad base tx check hash. 	
temBAD_PUT               			Malformed: Bad base tx format or check hash error 	
temBAD_DBTX              			Malformed: Bad DBTx support.                                                 	
temBAD_STATEMENTS        			Malformed: Bad Statements field.                                             	
temBAD_NEEDVERIFY        			Malformed: Bad NeedVerify field.                                             	
temBAD_STRICTMODE        			Malformed: Bad StrictMode support.                                           	
temBAD_LEDGER            			Malformed: Bad base ledger sequence.                                         	
temBAD_TRANSFERORDER     			Malformed: Current tx is not the one we expected. 	
temBAD_OPERATIONRULE     			Malformed: Operation Rule is not valid. 	
temBAD_DELETERULE					Malformed: Delete rule must contains '$account' condition because of insert rule
ttemBAD_UPDATERULE					Malformed: Update rule is needed and 'Fields' is needed in update rule. 	
temBAD_INSERTLIMIT					Malformed: Deal with insert count limit error. 	
temBAD_RULEANDTOKEN					Malformed: OperationRule and Confidential are not supported in the mean time.	
temBAD_TICK_SIZE         			Malformed: Tick size out of range.                                           	
temBAD_NEEDVERIFY_OPERRULE			Malformed: NeedVerify must be 1 if there is table has OperatinRule.     
      	                             
terRETRY                 			Retry transaction.                                                           	
terFUNDS_SPENT           			Can't set password, password set funds already spent.                        	
terINSUF_FEE_B           			Account balance can't pay fee.                                               	
terLAST                  			Process last.                                                                	
terNO_RIPPLE             			Path does not permit rippling.                                               	
terNO_ACCOUNT            			The source account does not exist.                                           	
terNO_AUTH               			Not authorized to hold IOUs.                                                 	
terNO_LINE               			No such line.                                                                	
terPRE_SEQ               			Missing/inapplicable prior transaction.                                      	
terOWNERS                			Non-zero owner count.                                                        	
terQUEUED                			Held until escalated fee drops.     
                                           	
tefTABLE_SAMENAME        			Table name and table new name is same or create exist table.                 	
tefTABLE_NOTEXIST        			Table is not exist. 	
tefTABLE_STATEERROR      			Table's state is error. 	
tefBAD_USER              			BAD User format.    	
tefTABLE_EXISTANDNOTDEL  			Table exist and not deleted. 	
tefTABLE_STORAGEERROR    			Table storage error. 	
tefTABLE_STORAGENORMALERROR   		Table storage normal error. 	
tefTABLE_TXDISPOSEERROR				Tx Dispose error. 	
tefTABLE_RULEDISSATISFIED			Operation rule not satisfied.	
tefINSUFFICIENT_RESERVE  			Insufficient reserve to complete requested operation. 	
tefINSU_RESERVE_TABLE				Insufficient reserve to create a table. 	
tefDBNOTCONFIGURED       			DB is not connected,please checkout 'sync_db'in config file. 	
tefBAD_DBNAME            			NameInDB does not match tableName. 	
tefBAD_STATEMENT					Statement is error. 	
                                     
tesSUCCESS               			The transaction was applied. Only final in a validated ledger.               	
==================================	======================================================================================