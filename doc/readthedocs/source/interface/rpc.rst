JSON-RPC接口
###########################

JSON-RPC，是一个无状态且轻量级的远程过程调用（RPC）传送协议，其传递内容透过 JSON 为主。

chainsqld沿用rippled的JSON-RPC，使用HTTP短连接，由“method”域指定调用的方法，“params”域指定调用的参数。
可以将RPC接口分为交易类和查询类。

.. _RPC返回值:

接口返回值
**************************

交易类接口
++++++++++++++++++++++++++

RPC交易类接口返回的JSON包含的各个域如下：

.. list-table::

    * - **域**
      - **类型**
      - **描述**
    * - result
      - 对象
      - 包含返回状态和具体结果，内容因命令而异。
    * - result.tx_json
      - 对象
      - 签名后的交易的JSON格式。
    * - result.tx_blob
      - 对象
      - 交易的16进制序列化。
    * - result.status
      - 字符串
      - 标识交易是否已被服务节点成功接收并且解析成功。
    * - result.engine_result
      - 字符串
      - 表明交易请求解析成功，并且能够被处理，现阶段的处理结果。
    * - result.engine_result_code
      - 整形
      - 与engine_result关联的整形值。
    * - result.engine_result_message
      - 字符串
      - 交易状态结果的描述。
    * - result.error
      - 字符串
      - 如果交易请求解析或者处理出错，返回错误类型码。
    * - result.error_code
      - 字符串
      - 与error关联的整形值。
    * - result.error_message
      - 字符串
      - 错误原因的描述。

成功示例
==========================================

.. code-block:: json

    {
        "result": {
            "engine_result": "tesSUCCESS",
            "engine_result_code": 0,
            "engine_result_message": "The transaction was applied. Only final in a validated ledger.",
            "status": "success",
            "tx_blob": "12000022800000002400000001201B0000010F614000000005F5E10068400000000000000A73210330E7FC9D56BB25D6893BA3F317AE5BCF33B3291BD63DB32654A313222F7FD0207446304402203A5E874FF57F41BEA70F3C1D0A839FA6307DC21049A1478DED7B18EBCA734D5002200685C792BCDCC6DA764F2EE2F1897100F914991A4A51E91D5CF72342FD38C0C58114B5F762798A53D543A014CAF8B297CFF8F2F937E88314934CD4FACC490E3DC5152F7C1BAD57EEEE3F9C77",
            "tx_json": {
                "Account": "zHb9CJAWyB4zj91VRWn96DkukG4bwdtyTh",
                "Amount": "100000000",
                "Destination": "zNRi42SAPegzJYzXYZfRFqPqUfGqKCaSbx",
                "Fee": "10",
                "Flags": 2147483648,
                "LastLedgerSequence": 271,
                "Sequence": 1,
                "SigningPubKey": "0330E7FC9D56BB25D6893BA3F317AE5BCF33B3291BD63DB32654A313222F7FD020",
                "TransactionType": "Payment",
                "TxnSignature": "304402203A5E874FF57F41BEA70F3C1D0A839FA6307DC21049A1478DED7B18EBCA734D5002200685C792BCDCC6DA764F2EE2F1897100F914991A4A51E91D5CF72342FD38C0C5",
                "hash": "2A5573C42CA73036A57AD823ACC4F0359D335FF067D6232EAB919AC2C130866E"
            }
        }
    }

出错示例
======================================

.. code-block:: json

    {
        "result": {
            "engine_result": "tefTABLE_EXISTANDNOTDEL",
            "engine_result_code": -176,
            "engine_result_message": "Table exist and not deleted.",
            "status": "success"
        },
        "tx_hash": "0D096D85F6F31E5BB93C44EB125D878BC704A8922AD27538B63676751D90D7FB"
    }

查询类接口
++++++++++++++++++++++++++

RPC查询类接口返回的JSON包含的各个域如下：

.. list-table::

    * - **域**
      - **类型**
      - **描述**
    * - result
      - 对象
      - 包含返回状态和具体结果。
    * - result.status
      - 字符串
      - 标识请求是否已被服务节点成功接收并且解析成功。
    * - result.error
      - 字符串
      - 如果请求解析或处理出错，返回错误类型码；处理成功，则被省略。
    * - result.error_code
      - 字符串
      - 与error关联的整形值。
    * - result.error_message
      - 字符串
      - 如果请求处理出错，描述错误原因；处理成功，则被省略。

成功示例
=============================================

.. code-block:: json

    {
        "result": {
            "account_data": {
                "Account": "zNRi42SAPegzJYzXYZfRFqPqUfGqKCaSbx",
                "Balance": "197899991",
                "Flags": 0,
                "LedgerEntryType": "AccountRoot",
                "OwnerCount": 3,
                "PreviousTxnID": "095CA8636351941D1AC9A9415D90F7A2AD73363198CDA0441999059C9A6B328B",
                "PreviousTxnLgrSeq": 359781,
                "Sequence": 13,
                "index": "68D7B391587F7FD814AE718F6BE298AACDB6662DFABF21A13FD163CF9E0C9C14"
            },
            "ledger_current_index": 363498,
            "status": "success",
            "validated": false
        }
    }

出错示例
============================================

.. code-block:: json

    {
        "result": {
            "account": "zcPMx2Zp4p9UnYaMtPLDwpSR5YFaa4E2SR",
            "error": "actNotFound",
            "error_code": 19,
            "error_message": "Account not found.",
            "ledger_current_index": 363481,
            "request": {
                "account": "zcPMx2Zp4p9UnYaMtPLDwpSR5YFaa4E2SR",
                "command": "account_info",
                "ledger_index": "current",
                "strict": true
            },
            "status": "error",
            "validated": false
        }
    }

.. _rpc-tx:

交易类接口
**************************

交易类接口包含rippled原生的交易类JSON-RPC接口、chainsqld新增的数据库表交易接口、智能合约交易类接口。

因为交易都需要在区块链上达成共识，所以交易类接口的应答结果是临时的，只代表此次交易是否已经进入本节点的临时账本。
最终的结果在共识后都有可能发生变化。

.. warning::

    交易类接口都需要将交易JSON进行签名或者向服务节点提供账户的私钥，
    本文中示例都是通过向服务节点提供账户私钥的方式。
    如果服务节点不可信任，或者请求通过公共网络发送，则存在风险。

Rippled交易
++++++++++++++++++++++++++++

rippled交易类JSON-RPC接口有很多，详情请参看XRP官方开发文档 `Transcation Formats <https://developers.ripple.com/transaction-formats.html>`_。 
下面展示一个示例。

转账
============================

请求格式：

.. code-block:: json

    {
        "method": "submit",
        "params": [{
            "offline": false,
            "secret": "xnoPBzXtMeMyMHUVTgbuqAfg1SUTb",
            "tx_json": {
                "Account": "zHb9CJAWyB4zj91VRWn96DkukG4bwdtyTh",
                "Amount": "100000000",
                "Destination": "zNRi42SAPegzJYzXYZfRFqPqUfGqKCaSbx",
                "TransactionType": "Payment"
            },
            "fee_mult_max": 1000
        }]
    }

参数说明：

.. list-table::

    * - **域**
      - **类型**
      - **描述**
    * - method
      - 字符串
      - | Rippled的交易类接口包含的Method有
        | sign、sign_for、submit、submit_multisigned、
        | transaction_entry、tx、tx_history。

        具体参看XRP官方开发文档 `Transcation Methods <https://developers.ripple.com/transaction-methods.html>`_。
    * - params
      - 数组
      - 包含请求的参数。
    * - secret
      - 字符串
      - 发起请求的账户的私钥，用来签名交易。
    * - tx_json
      - 对象
      - 最终账本中交易的json表现形式。还有一些必要的域，会自动填充进去。
    * - TransactionType
      - 字符串
      - 指定具体的交易类型，这里为Payment(转帐)。
    * - Account
      - 字符串
      - 发起交易的账户地址。
    * - Destination
      - 字符串
      - 接收转账的用户地址。
    * - Amount
      - 字符串
      - 此次转账的XRP数量，单位drop。

应答格式：

.. code-block:: json

    {
        "result": {
            "engine_result": "tesSUCCESS",
            "engine_result_code": 0,
            "engine_result_message": "The transaction was applied. Only final in a validated ledger.",
            "status": "success",
            "tx_blob": "12000022800000002400000002201B0002FA0E614000000005F5E10068400000000000000A73210330E7FC9D56BB25D6893BA3F317AE5BCF33B3291BD63DB32654A313222F7FD0207446304402207E88AA09F5C23A8E7AB29EC9BE5258B0C0A3F751AD8A8C26096FD6F022EC26FF0220112A2140F206679085B0015A2273BB4F802E23BFE64EF58F851F606BF6861ED68114B5F762798A53D543A014CAF8B297CFF8F2F937E88314934CD4FACC490E3DC5152F7C1BAD57EEEE3F9C77",
            "tx_json": {
                "Account": "zHb9CJAWyB4zj91VRWn96DkukG4bwdtyTh",
                "Amount": "100000000",
                "Destination": "zNRi42SAPegzJYzXYZfRFqPqUfGqKCaSbx",
                "Fee": "10",
                "Flags": 2147483648,
                "LastLedgerSequence": 195086,
                "Sequence": 2,
                "SigningPubKey": "0330E7FC9D56BB25D6893BA3F317AE5BCF33B3291BD63DB32654A313222F7FD020",
                "TransactionType": "Payment",
                "TxnSignature": "304402207E88AA09F5C23A8E7AB29EC9BE5258B0C0A3F751AD8A8C26096FD6F022EC26FF0220112A2140F206679085B0015A2273BB4F802E23BFE64EF58F851F606BF6861ED6",
                "hash": "1A4CA19291EED3A1F7D3FD8218B5FE1FF82D0A93368746A0188285E4CF60F6C1"
            }
        }
    }

.. _应答域说明:

应答域说明：

.. list-table::

    * - **域**
      - **类型**
      - **描述**
    * - engine_result
      - 字符串
      - 交易的初步处理结果码。
    * - engine_result_code
      - 整形
      - 与engine_result关联的整形值。
    * - engine_result_message
      - 字符串
      - 可读性较高的交易结果短语。
    * - tx_json
      - 对象
      - 最终形成的完整交易的json格式。
    * - tx_blob
      - 字符串
      - 用16进制序列化后的完整交易。

数据库表交易
+++++++++++++++++++++++++++++

数据库表交易类型接口可以分为三种，
TableListSet交易类型、SQLStatement交易类型、SQLTranscation交易类型。
每种类型的接口对应不同的数据库操作语句。

TableListSet
=============================

TableListSet交易类型主要对应SQL的数据定义语句（DDL）和数据控制语句（DCL）。
具体包含的操作有创建表、删除表、表重命名、表授权、表重建等操作，只有表的创建者可以删除及授权等其它操作。

TableListSet类型的交易的json格式（tx_json对象）各个域的描述如下：

.. list-table::

    * - **域**
      - **json类型**
      - **内部类型**
      - **描述**
    * - TransactionType
      - 字符串
      - String
      - 必填，交易类型为"TableListSet"。
    * - Account
      - 字符串
      - AccountID
      - 必填，发起交易的账户地址，用于操作鉴权。
    * - Tables
      - 数组
      - Array
      - 必填，指定本次操作所涉及的表。
    * - Table
      - 对象
      - Object
      - 必填，描述一张表。
    * - TableName
      - 字符串
      - Blob
      - 必填，指定用户层的表名。
    * - OpType
      - 整型
      - UInt32
      - 必填，具体操作类型有：1：创建表，2：删除表，3：改重命名，10：验证断言，11：表授权，12：表重建，13：多链整合。
    * - Raw
      - 数组
      - Array
      - 可选，用来指定列的属性，或是查询条件。也可以是SQL语句。
    * - NameInDB
      - 字符串
      - Hash160
      - 可选，数据库中对应的实际表名。
    * - TableNewName
      - 字符串
      - Blob
      - 可选，表重命名操作需要指定。
    * - User
      - 字符串
      - AccountID
      - 可选，表授权操作中，被授权的账户地址。
    * - Flags
      - 整型
      - UInt32
      - 可选，表授权操作中，表示被授予的权限。
    * - TxCheckHash
      - 字符串
      - Hash256
      - 可选，strict模式时设置的校验。
    * - Token
      - 字符串
      - Blob
      - 可选，创建表、授权表操作中用户公钥加密的密文。
    * - OperationRule
      - 对象
      - Blob
      - 可选，行级控制规则。

示例：

创建表
---------------------------

请求格式

.. code-block:: json

    {
        "method": "t_create",
        "params": [{
            "secret": "xx1zHjbA5yo7tF3cyMEP9odPKw4zD",
            "tx_json": {
                "TransactionType": "TableListSet",
                "Account": "zNRi42SAPegzJYzXYZfRFqPqUfGqKCaSbx",
                "Tables": [
                    {
                        "Table": { "TableName": "test_NULL1" }
                    }
                ],
                "OpType": 1,
                "Raw": [ 
                    {
                        "field": "id", 
                        "type": "int", 
                        "length": 11,
                        "PK": 1,
                        "NN": 1,
                        "UQ": 1,
                        "Index": 1
                    },
                    {
                        "field": "age",
                        "type": "int"
                    },
                    {
                        "field": "first_name",
                        "type": "varchar",
                        "length": 64
                    },
                    {
                        "field": "full_name",
                        "type": "varchar",
                        "length": 64
                    }
                ],
                "Confidential": false
            }
        }]
    }

.. note::

    Raw数组在创建表时用来描述表结构；在插入数据时用来表示插入的数据；
    在更新表时用来表示更新的列和条件；在查询列时用来表示查询条件。
    上例中，插入表时，Raw数组中每一个对象表示一列，包括列名，数据类型，数据长度，约束，索引等。
    具体见下表。

.. list-table::

    * - **域**
      - **类型**
      - **值**
    * - field
      - 字符串
      - 列名。
    * - type
      - 字符串
      - 列的数据类型，可选值有int/float/double/decimal/varchar/blob/text/datetime。
    * - length
      - 整数
      - 列的数据长度。
    * - PK
      - 整数
      - 值为1表示为列创建主键约束。
    * - NN
      - 整数
      - 值为1表示列的值不能为空（NULL）。
    * - UQ
      - 整数
      - 值为1表示为列创建唯一约束。
    * - Index
      - 整数
      - 值为1表示为列建立索引。
    * - FK
      - 整数
      - 值为1表示为列创建外键约束。必须配置REFERENCES使用。
    * - REFERENCES
      - 对象
      - 值的格式为{"table": "tablename", "field": "filedname"}。

一个成功应答格式：

.. code-block:: json

    {
        "result": {
            "engine_result": "tesSUCCESS",
            "engine_result_code": 0,
            "engine_result_message": "The transaction was applied. Only final in a validated ledger.",
            "status": "success"
        },
        "tx_hash": "1ED4E0F3CA238CE14145C38CCC06669376AC8B5F492E375D2658F721F07D288A"
    }

各个域的含义参考Rippled交易的\ `应答域说明`_\ 。

删除表
-------------------------

请求格式：

.. code-block:: json

    {
        "method": "t_drop",
        "params": [{
            "offline": false,
            "secret": "xx1zHjbA5yo7tF3cyMEP9odPKw4zD",
            "tx_json": {
                "TransactionType": "TableListSet",
                "Account": "zNRi42SAPegzJYzXYZfRFqPqUfGqKCaSbx",
                "Tables": [
                    {
                        "Table": { "TableName": "test_NULL1" }
                    }
                ],
                "OpType": 2
            }
        }]
    }

应答格式与其他请求基本一致。

重命名表
------------------------------

请求格式

.. code-block:: json

    {
        "method": "t_rename",
        "params": [{
            "offline": false,
            "secret": "xx1zHjbA5yo7tF3cyMEP9odPKw4zD",
            "tx_json": {
                "TransactionType": "TableListSet",
                "Account": "zNRi42SAPegzJYzXYZfRFqPqUfGqKCaSbx",
                "Tables": [
                    {
                        "Table": {
                            "TableName": "test_NULL1",
                            "TableNewName": "test_NULL2"
                        }
                    }
                ],
                "OpType": 3
            }
        }]
    }

应答格式与其他请求基本一致。

表授权
--------------------------------

请求格式

.. code-block:: json

    {
        "method": "t_grant",
        "params": [{
            "offline": false,
            "secret": "xx1zHjbA5yo7tF3cyMEP9odPKw4zD",
            "tx_json": {
                "TransactionType": "TableListSet",
                "Account": "zNRi42SAPegzJYzXYZfRFqPqUfGqKCaSbx",
                "Tables": [
                    {
                        "Table": { "TableName": "test_NULL2" }
                    }
                ],
                "OpType": 11,
                "User": "z9VF7yQPLcKgUoHwMbzmQBjvPsyMy19ubs",
                "PublicKey": "cBRmXRujuiBPuK46AGpMM5EcJuDtxpxJ8J2mCmgkZnPC1u8wqkUn",
                "Raw": [
                    {
                        "select": true,
                        "insert": true,
                        "update": true,
                        "delete": true
                    }
                ]
            }
        }]
    }

.. note::

    * PublicKey字段是可选的，加密方式创建的表在授权操作中需要提供用用户公钥加密过的密码的密文。
    * 如果要取消对某账户的权限，可重新做一次授权，在原有权限基础上对交易中Raw中的权限进行增删。
    * 对账户\ ``zzzzzzzzzzzzzzzzzzzzBZbvji``\ 授权表示对所有人进行授权，对所有人权限后还可单独对某一用户授权。已经被授权过的单一用户不受“所有人”再授权的影响。

应答格式与其他请求基本一致。

.. _SQLStatement接口:

SQLStatement
=============================================

SQLStatement交易类型主要对应SQL的数据操纵语句（DML）。
具体包含的操作有增、删、改操作，只有表的创建者和被授权的账户具有操作权限。

SQLStatement类型的交易的json格式（tx_json对象）各个域的描述如下：

.. list-table::

    * - **域**
      - **json类型**
      - **内部类型**
      - **描述**
    * - TransactionType
      - 字符串
      - String
      - 必填，交易类型为"SQLStatement"。
    * - Account
      - 字符串
      - AccountID
      - 必填，发起交易的账户地址，用于操作鉴权。
    * - Owner
      - 字符串
      - AccountID
      - 必填，用来指定表的创建者。
    * - Tables
      - 数组
      - Array
      - 必填，指定本次操作所涉及的表。
    * - Table
      - 对象
      - Object
      - 必填，描述一张表。
    * - TableName
      - 字符串
      - Blob
      - 必填，指定用户层的表名。
    * - OpType
      - 整型
      - UInt32
      - 必填，具体操作类型有：6:插入记录, 8:更新记录,9:删除记录。
    * - Raw
      - 数组
      - Array
      - 可选，用来指定列的属性，或是查询的列和条件。也可以是SQL语句。
    * - NameInDB
      - 字符串
      - Hash160
      - 可选，数据库中对应的实际表名。
    * - TxCheckHash
      - 字符串
      - Hash256
      - 可选，strict模式时设置的校验。
    * - AutoFillField
      - 字符串
      - String
      - 可选，用于插入操作，指定将此次交易的哈希插入到指定的列。
    * - OperationRule
      - 对象
      - Object
      - 可选，行级控制规则。

示例：

插入记录
---------------------------------

请求格式：

.. code-block:: json

    {
        "method": "r_insert",
        "params": [{
            "offline": false,
            "secret": "xnoPBzXtMeMyMHUVTgbuqAfg1SUTb",
            "tx_json": {
                "TransactionType": "SQLStatement",
                "Account": "zHb9CJAWyB4zj91VRWn96DkukG4bwdtyTh",
                "Owner": "zHb9CJAWyB4zj91VRWn96DkukG4bwdtyTh",
                "Tables":[
                    {
                        "Table": { "TableName": "aac" }
                    }
                ],
                "Raw": [
                    {
                        "id": 1,
                        "name": "AAA"
                        "age": 11
                    },
                    {
                        "id": 2,
                        "name": "BBB",
                        "age": 12
                    }
                ],
                "OpType": 6,
                "AutofillField": "txHash"
            }
        }]
    }

应答格式同上。

更新记录
----------------------------------

请求格式：

.. code-block:: json

    {
        "method": "r_update",
        "params": [{
            "offline": false,
            "secret": "xx1zHjbA5yo7tF3cyMEP9odPKw4zD",
            "tx_json": {
                "TransactionType": "SQLStatement",
                "Account": "zNRi42SAPegzJYzXYZfRFqPqUfGqKCaSbx",
                "Owner": "zNRi42SAPegzJYzXYZfRFqPqUfGqKCaSbx",
                "Tables": [
                    {
                        "Table": { "TableName": "test_NULL2" }
                    }
                ],
                "Raw": [
                    {
                        "full_name": "Chao Wang"
                    },
                    {
                        "age": 11,
                        "first_name": "Wang"
                    },
                    {
                        "id": 2
                    }
                ],
                "OpType": 8
            }
        }]
    }

.. note::

    Raw数组中第一个对象是要更新的列名和值，其后的所有对象均表示更新条件，
    对象内的多个条件为与（and）关系，对象之间为或（or）关系。

应答格式同上。

删除记录
---------------------------------

请求格式：

.. code-block:: json

    {
        "method": "r_delete",
        "params": [{
            "offline": false,
            "secret": "xx1zHjbA5yo7tF3cyMEP9odPKw4zD",
            "tx_json": {
                "TransactionType": "SQLStatement",
                "Account": "zNRi42SAPegzJYzXYZfRFqPqUfGqKCaSbx",
                "Owner": "zNRi42SAPegzJYzXYZfRFqPqUfGqKCaSbx",
                "Tables":[
                    {
                        "Table": { "TableName": "test_NULL2" }
                    }
                ],
                "Raw": [
                    { 
                        "id": 3
                    }
                ],
                "OpType": 9
            }
        }]
    }

.. note::

    Raw数组中所有对象表示条件，条件关系与更新操作一致。

应答格式同上。

SQLTranscation
=================================================

事务处理可以用来维护数据库的完整性，保证成批的SQL语句要么全部执行，要么全部不执行。

SQLTranscation RPC交易类型接口的请求方法名固定为\ ``t_sqlTxs``\ ，交易json格式（tx_json对象）各个域的描述如下：

.. list-table::

    * - **域**
      - **json类型**
      - **内部类型**
      - **描述**
    * - TransactionType
      - 字符串
      - String
      - 必填，交易类型为"SQLTranscation"。
    * - Account
      - 字符串
      - AccountID
      - 必填，发起交易的账户地址，用于操作鉴权。
    * - Statements
      - 数组
      - Array
      - 必填，数组中每一个对象表示事务中每一个具体的操作。
    * - Tables
      - 数组
      - Array
      - 必填，指定本次操作所涉及的表。
    * - Table
      - 对象
      - Object
      - 必填，描述一张表。
    * - TableName
      - 字符串
      - Blob
      - 必填，指定用户层的表名。
    * - OpType
      - 整型
      - UInt32
      - 必填，具体操作类型有：6:插入记录，8:更新记录，9:删除记录，10：验证断言，11:表授权。
    * - Raw
      - 数组
      - Array
      - 可选，用来指定列的属性，或是查询条件。也可以是SQL语句。
    * - NameInDB
      - 字符串
      - Hash160
      - 可选，数据库中对应的实际表名。
    * - TableNewName
      - 字符串
      - Blob
      - 可选，表重命名操作需要指定。
    * - User
      - 字符串
      - AccountID
      - 可选，表授权操作中，被授权的账户地址。
    * - Flags
      - 整型
      - UInt32
      - 可选，表授权操作中，表示被授予的权限。
    * - TxCheckHash
      - 字符串
      - Hash256
      - 可选，strict模式时设置的校验。

示例：

在一个事务中同时将数据插入两个不同的表，保证两次插入操作的原子性
-----------------------------------------------------------------------------------------

请求格式：

.. code-block:: json

    {
        "method": "t_sqlTxs",
        "params": [{
            "offline": false,
            "secret": "xx1zHjbA5yo7tF3cyMEP9odPKw4zD",
            "tx_json": {
                "TransactionType": "SQLTransaction",
                "Account": "zNRi42SAPegzJYzXYZfRFqPqUfGqKCaSbx",
                "Statements": [
                    {
                        "Owner": "zNRi42SAPegzJYzXYZfRFqPqUfGqKCaSbx",
                        "Tables": [
                            {
                                "Table": { "TableName": "Table1" }
                            }
                        ],
                        "OpType": 6,
                        "Raw": [
                            {
                                "id": 3,
                                "first_name": "Wang",
                                "full_name": "Chao Wang",
                                "age": 11
                            }
                        ]
                    },
                    {
                        "Owner": "zNRi42SAPegzJYzXYZfRFqPqUfGqKCaSbx",
                        "Tables": [
                            {
                                "Table": { "TableName": "Table2" }
                            }
                        ],
                        "OpType": 6,
                        "Raw":[
                            {
                                "id": 3,
                                "first_name": "Wang",
                                "full_name": "Chao Wang",
                                "age": 11
                            }
                        ]
                    }
                ]
            }
        }]
    }

应答格式同上。

.. _智能合约交易:

智能合约交易
++++++++++++++++++++++++++++++++++++++++

智能合约交易类型RPC接口包含部署合约接口和调用合约接口。
RPC接口方法名使用Rippled的交易方法名\ ``submit``\ ，交易json格式（tx_json对象）各个域的描述如下：

.. list-table::

    * - **域**
      - **json类型**
      - **内部类型**
      - **描述**
    * - TransactionType
      - 字符串
      - String
      - 必填，交易类型为"Contract"。
    * - ContractOpType
      - 整形
      - UInt32
      - 必填，表示智能合约的操作类型，1：部署合约，2：调用合约。
    * - Account
      - 字符串
      - AccountID
      - 必填，发起交易的账户地址，部署合约时也即是合约的所有者。
    * - ContractData
      - 字符串
      - String
      - 必填，部署合约时用来表示合约的二进制代码(ABI)，这里用16进制表示；调用合约时表示合约参数。
    * - ContractAddress
      - 字符串
      - ContractID
      - 调用合约时必填，表示合约账户的地址。
    * - ContractValue
      - 整形
      - STAMOUNT
      - 选填，本次交易要给合约地址转账的金额。
    * - Gas
      - 整形
      - UInt32
      - 必填，合约交易（部署和调用）消耗的Gas。

.. note::

    关于智能合约的编写和编译，参考solidity官方文档\ `introduction-smart-contracts <https://wizardforcel.gitbooks.io/solidity-zh/content/>`_\ 。

示例：

.. _rpc部署合约:

部署合约
==================================

请求格式：

.. code-block:: json

    {
        "method": "submit",
        "params": [
            {
                "offline": false,
                "secret": "xnoPBzXtMeMyMHUVTgbuqAfg1SUTb",
                "tx_json": {
                    "TransactionType": "Contract",
                    "ContractOpType": 1,
                    "Account": "zHb9CJAWyB4zj91VRWn96DkukG4bwdtyTh",
                    "ContractData": "60806040526103af806100136000396000f3006080604052600436106100565763ffffffff7c010000000000000000000000000000000000000000000000000000000060003504166306fdde03811461005b57806317d7de7c146100e5578063c47f0027146100fa575b600080fd5b34801561006757600080fd5b506100706101af565b6040805160208082528351818301528351919283929083019185019080838360005b838110156100aa578181015183820152602001610092565b50505050905090810190601f1680156100d75780820380516001836020036101000a031916815260200191505b509250505060405180910390f35b3480156100f157600080fd5b5061007061023d565b34801561010657600080fd5b506101ad6004803603602081101561011d57600080fd5b81019060208101813564010000000081111561013857600080fd5b82018360208201111561014a57600080fd5b8035906020019184600183028401116401000000008311171561016c57600080fd5b91908080601f0160208091040260200160405190810160405280939291908181526020018383808284376000920191909152509295506102d4945050505050565b005b6000805460408051602060026001851615610100026000190190941693909304601f810184900484028201840190925281815292918301828280156102355780601f1061020a57610100808354040283529160200191610235565b820191906000526020600020905b81548152906001019060200180831161021857829003601f168201915b505050505081565b60008054604080516020601f60026000196101006001881615020190951694909404938401819004810282018101909252828152606093909290918301828280156102c95780601f1061029e576101008083540402835291602001916102c9565b820191906000526020600020905b8154815290600101906020018083116102ac57829003601f168201915b505050505090505b90565b80516102e79060009060208401906102eb565b5050565b828054600181600116156101000203166002900490600052602060002090601f016020900481019282601f1061032c57805160ff1916838001178555610359565b82800160010185558215610359579182015b8281111561035957825182559160200191906001019061033e565b50610365929150610369565b5090565b6102d191905b80821115610365576000815560010161036f5600a165627a7a72305820f14ef6e7c6d9774a8d3cf6744bc9569884132a7abce3011ff2de4c69a4e7adba0029",
                    "ContractValue": 1,
                    "Gas": 3000000
                }
            }
        ]
    }

应答格式：

.. code-block:: json

    {
        "result": {
            "engine_result": "tesSUCCESS",
            "engine_result_code": 0,
            "engine_result_message": "The transaction was applied. Only final in a validated ledger.",
            "status": "success",
            "tx_blob": "1200181033000122800000002400000002201B000005F62037002DC6C068400000000000000A6013400000000000000173210330E7FC9D56BB25D6893BA3F317AE5BCF33B3291BD63DB32654A313222F7FD02074473045022100FBE361A9F921625D659871C3A7ECD616B7331F541742FAEACA0CC6C695530034022025EA828E9D158A528FF43BCF70C955726D9ACA9301D9EECC6E30DBDFDEAD62037041C40160806040526103AF806100136000396000F3006080604052600436106100565763FFFFFFFF7C010000000000000000000000000000000000000000000000000000000060003504166306FDDE03811461005B57806317D7DE7C146100E5578063C47F0027146100FA575B600080FD5B34801561006757600080FD5B506100706101AF565B6040805160208082528351818301528351919283929083019185019080838360005B838110156100AA578181015183820152602001610092565B50505050905090810190601F1680156100D75780820380516001836020036101000A031916815260200191505B509250505060405180910390F35B3480156100F157600080FD5B5061007061023D565B34801561010657600080FD5B506101AD6004803603602081101561011D57600080FD5B81019060208101813564010000000081111561013857600080FD5B82018360208201111561014A57600080FD5B8035906020019184600183028401116401000000008311171561016C57600080FD5B91908080601F0160208091040260200160405190810160405280939291908181526020018383808284376000920191909152509295506102D4945050505050565B005B6000805460408051602060026001851615610100026000190190941693909304601F810184900484028201840190925281815292918301828280156102355780601F1061020A57610100808354040283529160200191610235565B820191906000526020600020905B81548152906001019060200180831161021857829003601F168201915B505050505081565B60008054604080516020601F60026000196101006001881615020190951694909404938401819004810282018101909252828152606093909290918301828280156102C95780601F1061029E576101008083540402835291602001916102C9565B820191906000526020600020905B8154815290600101906020018083116102AC57829003601F168201915B505050505090505B90565B80516102E79060009060208401906102EB565B5050565B828054600181600116156101000203166002900490600052602060002090601F016020900481019282601F1061032C57805160FF1916838001178555610359565B82800160010185558215610359579182015B8281111561035957825182559160200191906001019061033E565B50610365929150610369565B5090565B6102D191905B80821115610365576000815560010161036F5600A165627A7A72305820F14EF6E7C6D9774A8D3CF6744BC9569884132A7ABCE3011FF2DE4C69A4E7ADBA00298114B5F762798A53D543A014CAF8B297CFF8F2F937E8",
            "tx_json": {
                "Account": "zHb9CJAWyB4zj91VRWn96DkukG4bwdtyTh",
                "ContractData": "60806040526103AF806100136000396000F3006080604052600436106100565763FFFFFFFF7C010000000000000000000000000000000000000000000000000000000060003504166306FDDE03811461005B57806317D7DE7C146100E5578063C47F0027146100FA575B600080FD5B34801561006757600080FD5B506100706101AF565B6040805160208082528351818301528351919283929083019185019080838360005B838110156100AA578181015183820152602001610092565B50505050905090810190601F1680156100D75780820380516001836020036101000A031916815260200191505B509250505060405180910390F35B3480156100F157600080FD5B5061007061023D565B34801561010657600080FD5B506101AD6004803603602081101561011D57600080FD5B81019060208101813564010000000081111561013857600080FD5B82018360208201111561014A57600080FD5B8035906020019184600183028401116401000000008311171561016C57600080FD5B91908080601F0160208091040260200160405190810160405280939291908181526020018383808284376000920191909152509295506102D4945050505050565B005B6000805460408051602060026001851615610100026000190190941693909304601F810184900484028201840190925281815292918301828280156102355780601F1061020A57610100808354040283529160200191610235565B820191906000526020600020905B81548152906001019060200180831161021857829003601F168201915B505050505081565B60008054604080516020601F60026000196101006001881615020190951694909404938401819004810282018101909252828152606093909290918301828280156102C95780601F1061029E576101008083540402835291602001916102C9565B820191906000526020600020905B8154815290600101906020018083116102AC57829003601F168201915B505050505090505B90565B80516102E79060009060208401906102EB565B5050565B828054600181600116156101000203166002900490600052602060002090601F016020900481019282601F1061032C57805160FF1916838001178555610359565B82800160010185558215610359579182015B8281111561035957825182559160200191906001019061033E565B50610365929150610369565B5090565B6102D191905B80821115610365576000815560010161036F5600A165627A7A72305820F14EF6E7C6D9774A8D3CF6744BC9569884132A7ABCE3011FF2DE4C69A4E7ADBA0029",
                "ContractOpType": 1,
                "ContractValue": "1",
                "Fee": "10",
                "Flags": 2147483648,
                "Gas": 3000000,
                "LastLedgerSequence": 1526,
                "Sequence": 2,
                "SigningPubKey": "0330E7FC9D56BB25D6893BA3F317AE5BCF33B3291BD63DB32654A313222F7FD020",
                "TransactionType": "Contract",
                "TxnSignature": "3045022100FBE361A9F921625D659871C3A7ECD616B7331F541742FAEACA0CC6C695530034022025EA828E9D158A528FF43BCF70C955726D9ACA9301D9EECC6E30DBDFDEAD6203",
                "hash": "80529639AC32BAEFEC24BC585AE794A80B5A6EEDD21708108D5334B43E9E3E04"
            }
        }
    }

.. note::

    应答域中没有直接返回合约地址，有返回部署合约交易的\ ``hash``\ ，
    可以使用 `tx <https://developers.ripple.com/tx.html>`_ 方法和交易哈希查询交易的详细信息，
    合约地址由返回的交易详细信息中的\ ``meta``\ .\ ``AffectedNodes``\ .\ ``CreatedNode``\ .\ ``NewFields``\ .\ ``Account``\ 域指定。
    建议使用Node.js或Java接口部署智能合约。

.. _rpc调用合约:

调用合约
==================================

.. code-block:: json

    {
        "method": "submit",
        "params": [
            {
                "secret": "xnoPBzXtMeMyMHUVTgbuqAfg1SUTb",
                "tx_json": {
                    "TransactionType": "Contract",
                    "Account": "zHb9CJAWyB4zj91VRWn96DkukG4bwdtyTh",
                    "ContractOpType": 2,
                    "ContractData": "0xc47f0027000000000000000000000000000000000000000000000000000000000000002000000000000000000000000000000000000000000000000000000000000000047761776100000000000000000000000000000000000000000000000000000000",
                    "Gas": 30000000,
                    "ContractAddress": "zpzebvchnEafz5DDYzovuUsJeqy6mEoC5Q"
                }
            }
        ]
    }

.. note::

    ``ContractData``\ 是编码过的合约接口和参数，可以使用\ `remix <http://remix.ethereum.org/#optimize=false&version=soljson-v0.5.1+commit.c8a2cb62.js>`_\ 部署合约，
    然后获取将要调用的合约接口和参数编码后的序列。
    建议使用Node.js或Java接口调用智能合约。

应答格式：

.. code-block:: json

    {
        "result": {
            "engine_result": "tesSUCCESS",
            "engine_result_code": 0,
            "engine_result_message": "The transaction was applied. Only final in a validated ledger.",
            "status": "success",
            "tx_blob": "1200181033000222800000002400000004201B00000974203701C9C38068400000000000000A73210330E7FC9D56BB25D6893BA3F317AE5BCF33B3291BD63DB32654A313222F7FD0207446304402204C494C054A861CA20D280B2713A1A7A3DC05FEC9F9ABA425D5D8BAD2E9A17568022028EC6E6F67A8EDF115CAE3F94EF1C6866DD07F850C24089E9115575DA787AD35704164C47F00270000000000000000000000000000000000000000000000000000000000000020000000000000000000000000000000000000000000000000000000000000000477617761000000000000000000000000000000000000000000000000000000008114B5F762798A53D543A014CAF8B297CFF8F2F937E88034140B1783ABF37EAEA5EAE35B73547B3C9F75E9294C",
            "tx_json": {
                "Account": "zHb9CJAWyB4zj91VRWn96DkukG4bwdtyTh",
                "ContractAddress": "zpzebvchnEafz5DDYzovuUsJeqy6mEoC5Q",
                "ContractData": "C47F0027000000000000000000000000000000000000000000000000000000000000002000000000000000000000000000000000000000000000000000000000000000047761776100000000000000000000000000000000000000000000000000000000",
                "ContractOpType": 2,
                "Fee": "10",
                "Flags": 2147483648,
                "Gas": 30000000,
                "LastLedgerSequence": 2420,
                "Sequence": 4,
                "SigningPubKey": "0330E7FC9D56BB25D6893BA3F317AE5BCF33B3291BD63DB32654A313222F7FD020",
                "TransactionType": "Contract",
                "TxnSignature": "304402204C494C054A861CA20D280B2713A1A7A3DC05FEC9F9ABA425D5D8BAD2E9A17568022028EC6E6F67A8EDF115CAE3F94EF1C6866DD07F850C24089E9115575DA787AD35",
                "hash": "0D0021A1EDCC1AD06374EC7FE9F37425E8C27E20044E673447957D0713BC01A2"
            }
        }
    }

查询类接口
************************************

查询类接口包含rippled原生的查询类JSON-RPC接口、chainsqld新增的数据库记录和信息查询接口、智能合约查询接口。

Rippled查询接口
+++++++++++++++++++++++++++++++++++++

rippled查询类JSON-RPC接口有很多，详情请参看XRP官方开发文档 `rippled API Reference <https://developers.ripple.com/rippled-api.html>`_。 
下面展示一个示例。

查询账户信息
======================================

请求格式：

.. code-block:: json

    {
        "method": "account_info",
        "params": [
            {
                "account": "zNRi42SAPegzJYzXYZfRFqPqUfGqKCaSbx",
                "strict": true,
                "ledger_index": "current"
            }
        ]
    }

参数说明：

.. list-table::

    * - **域**
      - **类型**
      - **描述**
    * - Account
      - 字符串
      - 必填，一个账户的唯一标识。
    * - strict
      - 布尔
      - 可选，如果设置为True，Account这个域只能填写账户的公钥或者账户地址。
    * - ledger_index
      - 字符串或整形
      - 可选，指定具体的账本进行查询，可选的值参考\ `Specifying Ledgers <https://developers.ripple.com/basic-data-types.html#specifying-ledgers>`_\ 。

应答格式：

.. code-block:: json

    {
        "result": {
            "account_data": {
                "Account": "zNRi42SAPegzJYzXYZfRFqPqUfGqKCaSbx",
                "Balance": "198003540",
                "Flags": 0,
                "LedgerEntryType": "AccountRoot",
                "OwnerCount": 3,
                "PreviousTxnID": "F6894AFA34C400CF36CCB345E044711753A5FAB9BF6BFAD6EFB2FA3C2F0A5525",
                "PreviousTxnLgrSeq": 251321,
                "Sequence": 12,
                "index": "68D7B391587F7FD814AE718F6BE298AACDB6662DFABF21A13FD163CF9E0C9C14"
            },
            "ledger_current_index": 254703,
            "status": "success",
            "validated": false
        }
    }

应答域说明：

.. list-table::

    * - **域**
      - **类型**
      - **描述**
    * - account_data
      - 对象
      - 指定账本中的账户信息。
    * - ledger_current_index
      - 整形
      - 接收到请求时的账本序列号。
    * - status
      - 字符串
      - 指示查询请求查询成功或者失败。
    * - validated
      - 布尔
      - 如果是True，表示返回的信息来自于已共识的账本，如果没有这个域或者值为False，则表示查询结果不是最终的结果。

数据库查询
++++++++++++++++++++++++++++++++++++

 | 数据库查询包含SQL数据操纵语句（DML）中的查操作和查询数据库表跟账户相关联的一些信息。
 | 查询指定表中的数据记录，直接从本节点查询。

r_get方法
======================================

下面是旧版本的请求JSON格式。这里有个问题，Account是用户自己填的，但是节点无法验证Account的真实身份，
用户A完全可以把Account的值填成账户B（已授权账户）的地址，或者直接填写表的所有者（Owner）的地址。
这样，不管用户A持有的账户是否真的有查询权限，都可以查询表的数据了。

.. code-block:: json

    {
        "method": "r_get",
        "params": [{
            "tx_json": {
                "Account": "zHb9CJAWyB4zj91VRWn96DkukG4bwdtyTh",
                "Owner": "zHb9CJAWyB4zj91VRWn96DkukG4bwdtyTh",
                "Tables": [ 
                    {
                        "Table": { "TableName": "test1" } 
                    }
                ],
                "Raw": [
                    [ ], 
                    { 
                        "id": 1
                    }
                ]
            }
        }]
    }


所有需要增加签名字段，用来验证用户的身份，修改后的API格式不变，请求的JSON格式变为如下。

.. code-block:: json

    {
        "method": "r_get",
        "params": [{
            "publicKey": "0253D5CE98521C9109791B69D2DABD135061BA2D4CA3742346D8DD56296D3F038F",
            "signature": "3045022100CF636FC5D0AE06AD097250D8344974315DE32BC6C1AF0B2AE87D600560FB571602203AC238419A2A7CB75556B24111ABD52764070AF62E5EBEC84A938148ADC151B6",
            "signingData": "{\"Account\":\"zHb9CJAWyB4zj91VRWn96DkukG4bwdtyTh\",\"Owner\":\"zHb9CJAWyB4zj91VRWn96DkukG4bwdtyTh\",\"Tables\":[{\"Table\":{\"TableName\":\"fasefa\"}}],\"Raw\":\"[[]]\",\"LedgerIndex\":47778}",
            "tx_json": {
                "LedgerIndex": 12345,
                "Account": "zHb9CJAWyB4zj91VRWn96DkukG4bwdtyTh",
                "Owner": "zHb9CJAWyB4zj91VRWn96DkukG4bwdtyTh",
                "Tables": [
                    {
                        "Table": { "TableName": "test1" }
                    }
                ],
                "Raw": [
                    [ ],
                    {
                        "id": 1
                    }
                ]
            }
        }]
    }

参数说明：

.. list-table::

    * - **域**
      - **json类型**
      - **内部类型**
      - **描述**
    * - publicKey
      - 字符串
      - String
      - 必填，签名者的公钥。
    * - signingData
      - 字符串
      - String
      - 必填，被签名的数据，即tx_json域的值。
    * - signature
      - 字符串
      - String
      - 必填，Account的私钥对签名数据signingData的签名结果。
    * - LedgerIndex
      - 字符串或整形
      - UInt64
      - 必填，当前区块号，用来对签名时效做限制。 
    * - Account
      - 字符串
      - AccountID
      - 必填，发起查询请求的账户
    * - Owner
      - 字符串
      - AccountID
      - 必填，表的所有者。
    * - tx_json
      - 对象
      - Object
      - 参看\ `SQLStatement接口`_\ 。

.. warning::

    因为必须对查询接口的JSON进行签名（跟交易类接口类似，但不支持提供私钥的形式），
    所以之后的版本不能用JSON-RPC接口直接查询表数据了。

SQL语句查询
==========================================

 | 通过SQL语句直接查询的接口支持复杂查询，多表联合查询等。
 | 为了区分权限，分为两个接口：
 | 管理员接口：需要在节点配置文件中配置admin IP，才可调用，不检查调用者身份。
 | 普通接口：节点不需要配置admin IP，通过签名验证调用者身份。

请求格式：

admin接口：

.. code-block:: json

    {
        "method": "r_get_sql_admin",
        "params": [{
            "sql": "select * from t_43ACD1FF143986210A44AB8B609371B392F45A86"
        }]
    }

.. note::

    SQL语句中的表名为数据库中的实际表名，需要先\ `查询实际表名`_\ 。

应答格式：

.. code-block:: json

    {
        "result": {
            "diff": 0,
            "lines": [
                {
                    "age": 11,
                    "first_name": "Peer",
                    "full_name": "null",
                    "id": 1
                },
                {
                    "age": "null",
                    "first_name": "Peer",
                    "full_name": "Peer safe",
                    "id": 2
                }
            ],
            "status": "success"
        }
    }

应答域说明：

.. list-table::

    * - **域**
      - **json类型**
      - **内部类型**
      - **描述**
    * - diff
      - 整形
      - UInt32
      - 当前区块序号与被查询表数据库同步到的区块序号的差值，如果有多个表，取最大差值。
    * - lines
      - 数组
      - Vector
      - 查询的结果行。

普通接口：

.. code-block:: json

    {
        "method": "r_get_sql_user",
        "params": [{
            "publicKey": "0253D5CE98521C9109791B69D2DABD135061BA2D4CA3742346D8DD56296D3F038F",
            "signature": "3045022100CF636FC5D0AE06AD097250D8344974315DE32BC6C1AF0B2AE87D600560FB571602203AC238419A2A7CB75556B24111ABD52764070AF62E5EBEC84A938148ADC151B6",
            "signingData": "{\"Account\":\"zHb9CJAWyB4zj91VRWn96DkukG4bwdtyTh\",\"Owner\":\"zHb9CJAWyB4zj91VRWn96DkukG4bwdtyTh\",\"Tables\":[{\"Table\":{\"TableName\":\"fasefa\"}}],\"Raw\":\"[[]]\",\"LedgerIndex\":47778}",
            "tx_json": {
                "LedgerIndex": 12345,
                "Account": "zHb9CJAWyB4zj91VRWn96DkukG4bwdtyTh",
                "Sql": "select * from t_xxxx where id=10"
            }
        }]
    }

.. warning::

    这里只是列一下JSON格式，因为需要签名，所以普通接口的RPC请求并没有实现。详情请参看Java和Node.js接口说明。

.. _查询实际表名:

查询实际表名
==========================================

请求格式：

.. code-block:: json

    {
        "method": "g_dbname",
        "params": [{
            "account": "zHb9CJAWyB4zj91VRWn96DkukG4bwdtyTh",
            "tablename": "Table1"
        }]
    }

参数说明：

.. list-table::

    * - **域**
      - **json类型**
      - **内部类型**
      - **描述**
    * - account
      - 字符串
      - AccountID
      - 必填，表的创建者账户地址。
    * - tablename
      - 字符串
      - String
      - 必填，创建表时，用户指定的表名。

应答格式：

.. code-block:: json

    {
        "result": {
            "nameInDB": "60C1540F58A8E608CF76DC2E12DB81842CB9591E",
            "status": "success"
        }
    }

应答域说明：

.. list-table::

    * - **域**
      - **json类型**
      - **内部类型**
      - **描述**
    * - nameInDB
      - 字符串
      - String
      - t\_\<nameInDB\>组成数据库中的实际表名。
    * - status
      - 字符串
      - String
      - 表示请求执行是否成功。

查询操作权限
===========================================

请求格式：

.. code-block:: json

    {
        "method": "table_auth",
        "params": [{
            "owner": "zNRi42SAPegzJYzXYZfRFqPqUfGqKCaSbx",
            "tablename": "Table1"
        }]
    }

参数说明：

.. list-table::

    * - **域**
      - **json类型**
      - **内部类型**
      - **描述**
    * - owner
      - 字符串
      - String
      - 必填，表的创建者账户地址。
    * - tablename
      - 字符串
      - String
      - 必填，创建表时，用户指定的表名。

应答格式：

.. code-block:: json

    {
        "result": {
            "owner": "zNRi42SAPegzJYzXYZfRFqPqUfGqKCaSbx",
            "status": "success",
            "tablename": "Table1",
            "users": [
                {
                    "account": "zNRi42SAPegzJYzXYZfRFqPqUfGqKCaSbx",
                    "authority": {
                        "delete": true,
                        "insert": true,
                        "select": true,
                        "update": true
                    }
                }
            ]
        }
    }

应答域说明：

.. list-table::

    * - **域**
      - **json类型**
      - **内部类型**
      - **描述**
    * - status
      - 字符串
      - String
      - 表示请求执行是否成功。
    * - authority
      - 对象
      - Object
      - 包含曾删改查等权限，true表示拥有权限，省略或false表示没有对应的操作权限。

查询拥有的表
=================================================

请求格式：

.. code-block:: json

    {
        "method": "g_accountTables",
        "params": [{
            "account": "zNRi42SAPegzJYzXYZfRFqPqUfGqKCaSbx",
            "detail": false
        }]
    }

参数说明：

.. list-table::

    * - **域**
      - **json类型**
      - **内部类型**
      - **描述**
    * - account
      - 字符串
      - AccountID
      - 必填，被查询的账户的地址。
    * - detail
      - 布尔
      - Bool
      - 可选，是否显示表的详细信息。

应答格式：

.. code-block:: json

    {
        "result": {
            "status": "success",
            "tables": [
                {
                    "ledger_index": 197647,
                    "nameInDB": "325251E2404DD9F66E9B5573963EE9D0D381319E",
                    "tablename": "test_NULL2",
                    "tx_hash": "1ED4E0F3CA238CE14145C38CCC06669376AC8B5F492E375D2658F721F07D288A"
                },
                {
                    "ledger_index": 251319,
                    "nameInDB": "52CE9409ECEF864A7FBEF80FD183EEB9D23F8EF3",
                    "tablename": "Table1",
                    "tx_hash": "CCB88EB4537755FAA8B1A31823DADC6346F5072843924ADE13BC425CE8218247"
                },
                {
                    "ledger_index": 251321,
                    "nameInDB": "60C1540F58A8E608CF76DC2E12DB81842CB9591E",
                    "tablename": "Table2",
                    "tx_hash": "F6894AFA34C400CF36CCB345E044711753A5FAB9BF6BFAD6EFB2FA3C2F0A5525"
                }
            ]
        }
    }

应答域说明：

.. list-table::

    * - **域**
      - **json类型**
      - **内部类型**
      - **描述**
    * - status
      - 字符串
      - String
      - 表示请求执行是否成功。
    * - tables
      - 数组
      - Array
      - 数组中每一个对象包含一个表的相关信息。

.. _rpc查询合约:

智能合约查询
++++++++++++++++++++++++++++++++

 | 智能合约的部署和调用参见\ `智能合约交易`_\ 。
 | 查询接口的方法名为\ ``contract_call``\ 。

查询示例
===============================

请求格式：

.. code-block:: json

    {
        "method": "contract_call",
        "params": [{
            "account": "zHb9CJAWyB4zj91VRWn96DkukG4bwdtyTh",
            "contract_data": "17d7de7c",
            "contract_address": "zpzebvchnEafz5DDYzovuUsJeqy6mEoC5Q"
        }]
    }

参数说明：

.. list-table::
    :align: left

    * - **域**
      - **json类型**
      - **内部类型**
      - **描述**
    * - account
      - 字符串
      - AccountID
      - 必填，请求发起者账户地址。
    * - contract_data
      - 字符串
      - String
      - 必填，查询参数，具体说明，见\ `rpc调用合约`_\ 。
    * - contract_address
      - 字符串
      - AccountID
      - 必填，合约地址。

应答格式：

.. code-block:: json

    {
        "result": {
            "contract_call_result": "0x000000000000000000000000000000000000000000000000000000000000002000000000000000000000000000000000000000000000000000000000000000047761776100000000000000000000000000000000000000000000000000000000",
            "request": {
                "account": "zHb9CJAWyB4zj91VRWn96DkukG4bwdtyTh",
                "command": "contract_call",
                "contract_address": "zpzebvchnEafz5DDYzovuUsJeqy6mEoC5Q",
                "contract_data": "17d7de7c"
            },
            "status": "success"
        }
    }

应答域说明：

.. list-table::
    :align: left

    * - **域**
      - **类型**
      - **内部类型**
      - **描述**
    * - contract_call_result
      - 字符串
      - String
      - 返回的查询结果，RLP编码序列化后的结果，需要反序列化。
    * - request
      - 对象
      - Object
      - 原始请求 。

.. note::

    查询结果需要反序列化，建议使用Node.js或Java接口进行智能合约的查询调用。