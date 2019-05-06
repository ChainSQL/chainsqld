Websocket接口
###########################

Websocket接口与JSON-RPC接口的主要区别在于，
使用WebSocket API，客户端和节点只需要完成一次握手，
两者之间就直接可以创建持久性的连接，并进行双向数据传输。
并且节点可以主动向已连接的客户端推送数据，实现订阅/发布。

接口类型与JSON-RPC一样，可以分为交易类和查询类接口。本文只列举部分接口，其它接口请参考JSON-RPC接口。

.. _Websocket返回值:

接口返回值
**************************

交易类接口
++++++++++++++++++++++++++

Websocket交易类接口返回的JSON包含的各个域如下：

.. list-table::

    * - **域**
      - **类型**
      - **描述**
    * - id
      - 整数或者字符串
      - 与请求的id一致。
    * - type
      - 字符串
      - 标识数据类型，response表示对命令的响应，异步通知使用不同的值（例如：table、transaction）。
    * - status
      - 字符串
      - 标识交易是否已被服务节点成功接收并且解析成功
    * - result
      - 对象
      - 包含返回状态和具体结果，内容因命令而异。
    * - result.engine_result
      - 字符串
      - 表明交易请求解析成功，并且能够被处理，现阶段的处理结果。
    * - result.engine_result_code
      - 整形
      - 与engine_result关联的整形值。
    * - result.engine_result_message
      - 字符串
      - 交易状态结果的描述。
    * - reqeust
      - 对象
      - 请求处理出错时，展示原始请求的格式。
    * - error
      - 字符串
      - 如果交易请求解析或者处理出错，返回错误类型码。
    * - error_code
      - 字符串
      - 与error关联的整形值。
    * - error_message
      - 字符串
      - 错误原因的描述。

成功示例
============================================

.. code-block:: json

    {
        "id": 2,
        "result": {
            "engine_result": "tesSUCCESS",
            "engine_result_code": 0,
            "engine_result_message": "The transaction was applied. Only final in a validated ledger.",
            "tx_blob": "12000022800000002400000003201B00001F40614000000005F5E10068400000000000000A73210330E7FC9D56BB25D6893BA3F317AE5BCF33B3291BD63DB32654A313222F7FD02074473045022100C57C4430FDC9F43CD0EC5BFACFCF09582399D0414F7484DEA8D5AEA1D315605502200A9C569863A4654D073EDC273E7321652887B6610CE0C20DB3E35A38639F62DD8114B5F762798A53D543A014CAF8B297CFF8F2F937E88314934CD4FACC490E3DC5152F7C1BAD57EEEE3F9C77",
            "tx_json": {
                "Account": "zHb9CJAWyB4zj91VRWn96DkukG4bwdtyTh",
                "Amount": "100000000",
                "Destination": "zNRi42SAPegzJYzXYZfRFqPqUfGqKCaSbx",
                "Fee": "10",
                "Flags": 2147483648,
                "LastLedgerSequence": 8000,
                "Sequence": 3,
                "SigningPubKey": "0330E7FC9D56BB25D6893BA3F317AE5BCF33B3291BD63DB32654A313222F7FD020",
                "TransactionType": "Payment",
                "TxnSignature": "3045022100C57C4430FDC9F43CD0EC5BFACFCF09582399D0414F7484DEA8D5AEA1D315605502200A9C569863A4654D073EDC273E7321652887B6610CE0C20DB3E35A38639F62DD",
                "hash": "8D1CC127661FD004B6700AB60CEC8C0EB0A733CF894A074C635914FAE49C928F"
            }
        },
        "status": "success",
        "type": "response"
    }

出错示例
=============================================

.. code-block:: json

    {
        "error": "badSecret",
        "error_code": 42,
        "error_message": "Secret does not match account.",
        "id": 2,
        "request": {
            "command": "submit",
            "id": 2,
            "secret": "xnoPBzXtMeMyMHUVTgbuqAfg1SUTb",
            "tx_json": {
                "Account": "zNRi42SAPegzJYzXYZfRFqPqUfGqKCaSbx",
                "Amount": "100000000",
                "Destination": "zcPMx2Zp4p9UnYaMtPLDwpSR5YFaa4E2SR",
                "TransactionType": "Payment"
            }
        },
        "status": "error",
        "type": "response"
    }

查询类接口
++++++++++++++++++++++++++

Websocket查询类接口返回的JSON包含的各个域如下：

.. list-table::

    * - **域**
      - **类型**
      - **描述**
    * - id
      - 整数或者字符串
      - 与请求的id一致。
    * - type
      - 字符串
      - 标识数据类型，response表示对命令的响应，异步通知使用不同的值（例如：table、transaction）。
    * - status
      - 字符串
      - 标识请求是否已被服务节点成功接收并且解析成功。
    * - result
      - 对象
      - 包含返回状态和具体结果，内容因命令而异。
    * - reqeust
      - 对象
      - 请求处理出错时，展示原始请求的格式。
    * - error
      - 字符串
      - 如果请求解析或者处理出错，返回错误类型码。
    * - error_code
      - 字符串
      - 与error关联的整形值。
    * - error_message
      - 字符串
      - 错误原因的描述。

成功示例
===============================================

.. code-block:: json

    {
        "id": 5,
        "result": {
            "lines": [
            ]
        },
        "status": "success",
        "type": "response"
    }

出错示例
==============================================

.. code-block:: json

    {
        "error":"actNotFound",
        "error_code":19,
        "error_message":"Account not found.",
        "request":{
            "account":"zcPMx2Zp4p9UnYaMtPLDwpSR5YFaa4E2SR",
            "command":"account_lines",
            "ledger_index":"validated"
        },
        "status":"error",
        "type":"response"
    }

交易类接口
**************************

交易类接口包含rippled原生的交易类接口、chainsqld新增的数据库表交易接口、
智能合约交易类接口。chainsqld沿用rippled的Websocket接口。交易的通用格式如下：

.. list-table::

    * - **域**
      - **类型**
      - **描述**
    * - id
      - 整形
      - 请求ID，异步模型，用来配对请求和应答。
    * - command
      - 字符串
      - 必填，接口（命令）名。
    * - tx_blob
      - 字符串
      - 可选，具体交易jSON格式增加签名后的16进制格式。如果提供这个域，则下面的域都可以省略。
    * - tx_json
      - 对象
      - 具体交易的json格式。
    * - TransactionType
      - 字符串
      - 指定具体的交易类型。
    * - secret
      - 字符串
      - 账户私钥，发送给服务节点，节点代替用户对交易进行签名。

.. warning::

    交易类接口都需要将交易JSON进行签名或者向服务节点提供账户的私钥，
    本文中展示的示例主要是通过向服务节点提供账户私钥的方式。
    如果服务节点不可信任，或者请求通过公共网络发送，则存在风险。

Rippled交易
+++++++++++++++++++++++++++++

rippled Websocket交易类接口有很多，详情请参看XRP官方开发文档 `Transcation Formats <https://developers.ripple.com/transaction-formats.html>`_。 
下面展示一个示例。

转账
============================

请求格式：

.. code-block:: json

    {
        "id": 2,
        "command": "submit",
        "tx_json" : {
            "TransactionType" : "Payment",
            "Account": "zHb9CJAWyB4zj91VRWn96DkukG4bwdtyTh",
            "Amount": "100000000",
            "Destination": "zNRi42SAPegzJYzXYZfRFqPqUfGqKCaSbx"
        },
        "secret" : "xnoPBzXtMeMyMHUVTgbuqAfg1SUTb"
    }

应答格式：

.. code-block:: json

    {
        "id": 2,
        "result": {
            "engine_result": "tesSUCCESS",
            "engine_result_code": 0,
            "engine_result_message": "The transaction was applied. Only final in a validated ledger.",
            "tx_blob": "12000022800000002400000003201B00001F40614000000005F5E10068400000000000000A73210330E7FC9D56BB25D6893BA3F317AE5BCF33B3291BD63DB32654A313222F7FD02074473045022100C57C4430FDC9F43CD0EC5BFACFCF09582399D0414F7484DEA8D5AEA1D315605502200A9C569863A4654D073EDC273E7321652887B6610CE0C20DB3E35A38639F62DD8114B5F762798A53D543A014CAF8B297CFF8F2F937E88314934CD4FACC490E3DC5152F7C1BAD57EEEE3F9C77",
            "tx_json": {
                "Account": "zHb9CJAWyB4zj91VRWn96DkukG4bwdtyTh",
                "Amount": "100000000",
                "Destination": "zNRi42SAPegzJYzXYZfRFqPqUfGqKCaSbx",
                "Fee": "10",
                "Flags": 2147483648,
                "LastLedgerSequence": 8000,
                "Sequence": 3,
                "SigningPubKey": "0330E7FC9D56BB25D6893BA3F317AE5BCF33B3291BD63DB32654A313222F7FD020",
                "TransactionType": "Payment",
                "TxnSignature": "3045022100C57C4430FDC9F43CD0EC5BFACFCF09582399D0414F7484DEA8D5AEA1D315605502200A9C569863A4654D073EDC273E7321652887B6610CE0C20DB3E35A38639F62DD",
                "hash": "8D1CC127661FD004B6700AB60CEC8C0EB0A733CF894A074C635914FAE49C928F"
            }
        },
        "status": "success",
        "type": "response"
    }

应答域说明请参考JSON-RPC接口下的转账示例。

.. _转账:

另外一种使用签名后的交易16进制序列的格式如下：

.. code-block:: json

    {
        "id": 2,
        "command": "submit",
        "tx_blob": "12000022800000002400000003201B00001F40614000000005F5E10068400000000000000A73210330E7FC9D56BB25D6893BA3F317AE5BCF33B3291BD63DB32654A313222F7FD02074473045022100C57C4430FDC9F43CD0EC5BFACFCF09582399D0414F7484DEA8D5AEA1D315605502200A9C569863A4654D073EDC273E7321652887B6610CE0C20DB3E35A38639F62DD8114B5F762798A53D543A014CAF8B297CFF8F2F937E88314934CD4FACC490E3DC5152F7C1BAD57EEEE3F9C77"
    }

参数说明：

.. list-table::

    * - **域**
      - **类型**
      - **描述**
    * - id
      - 整形
      - 请求ID，异步模型，用来配对请求和应答。
    * - command
      - 字符串
      -  | 指定websocket接口（命令）名。
         | 交易请求的主要命令是submit，还有一些非交易类的辅助命令，比如签名交易，查询交易等。

         具体参看XRP官方开发文档 `Transcation Methods <https://developers.ripple.com/transaction-methods.html>`_。
    * - tx_blob
      - 字符串
      - 带\ `签名`_\ 字段的具体交易的16进制序列。

应答格式：

.. code-block:: json

    {
        "id": 2,
        "result": {
            "engine_result": "tesSUCCESS",
            "engine_result_code": 0,
            "engine_result_message": "The transaction was applied. Only final in a validated ledger.",
            "tx_blob": "12000022800000002400000003201B00001F40614000000005F5E10068400000000000000A73210330E7FC9D56BB25D6893BA3F317AE5BCF33B3291BD63DB32654A313222F7FD02074473045022100C57C4430FDC9F43CD0EC5BFACFCF09582399D0414F7484DEA8D5AEA1D315605502200A9C569863A4654D073EDC273E7321652887B6610CE0C20DB3E35A38639F62DD8114B5F762798A53D543A014CAF8B297CFF8F2F937E88314934CD4FACC490E3DC5152F7C1BAD57EEEE3F9C77",
            "tx_json": {
                "Account": "zHb9CJAWyB4zj91VRWn96DkukG4bwdtyTh",
                "Amount": "100000000",
                "Destination": "zNRi42SAPegzJYzXYZfRFqPqUfGqKCaSbx",
                "Fee": "10",
                "Flags": 2147483648,
                "LastLedgerSequence":8000,
                "Sequence": 3,
                "SigningPubKey": "0330E7FC9D56BB25D6893BA3F317AE5BCF33B3291BD63DB32654A313222F7FD020",
                "TransactionType": "Payment",
                "TxnSignature": "3045022100C57C4430FDC9F43CD0EC5BFACFCF09582399D0414F7484DEA8D5AEA1D315605502200A9C569863A4654D073EDC273E7321652887B6610CE0C20DB3E35A38639F62DD",
                "hash": "8D1CC127661FD004B6700AB60CEC8C0EB0A733CF894A074C635914FAE49C928F"
            }
        },
        "status": "success",
        "type": "response"
    }


.. _签名:

签名
============================

rippled提供了为交易签名接口，可以用来辅助做功能测试。
用户将具体交易的JSON格式和私钥传递个服务节点，节点帮助用户进行交易的签名后，返回交易的tx_blob，
之后用户可以调用submit命令提交交易。

.. warning::

    与submit命令一样，同样需要提供账户私钥给节点，如果服务节点不可信任，或者请求通过公共网络发送，
    账户私钥存在被窃取的风险，工程项目中应使用本地签名的方式，或使用本档中的Java和Node.js接口。

下面示例签名一个转账交易。

请求格式：

.. code-block:: json

    {
        "id": 2,
        "command": "sign",
        "tx_json" : {
            "TransactionType" : "Payment",
            "Account": "zHb9CJAWyB4zj91VRWn96DkukG4bwdtyTh",
            "Amount": "100000000",
            "Destination": "zNRi42SAPegzJYzXYZfRFqPqUfGqKCaSbx",
            "LastLedgerSequence": 8000
        },
        "secret" : "xnoPBzXtMeMyMHUVTgbuqAfg1SUTb",
        "offline": false,
        "fee_mult_max": 1000
    }

参数说明：

.. list-table::

    * - **域**
      - **类型**
      - **描述**
    * - tx_json
      - 对象
      - 具体交易的JSON格式，这里为转账交易。
    * - tx_json.LastLedgerSequence
      - 整数
      - 可选，允许交易进入的最大区块号。如不提供，节点会默认在当前区块号的基础上加上一个值，手动测试，建议使用一个较大的值。
    * - secret
      - 字符串
      - 必填，账户私钥。

应答格式：

.. code-block:: json

    {
        "id": 2,
        "result": {
            "tx_blob": "12000022800000002400000003201B00000FFC614000000005F5E10068400000000000000A73210330E7FC9D56BB25D6893BA3F317AE5BCF33B3291BD63DB32654A313222F7FD02074473045022100EF12099B281DCD9EB7D23B020CCEA8817E598C4A0EB0DD7365A52B318B2778CB02205AAAAD30B2C3BE33E20334BEBCAB30B4217A21BBD7A7B0C80F13F93D257BB0408114B5F762798A53D543A014CAF8B297CFF8F2F937E88314934CD4FACC490E3DC5152F7C1BAD57EEEE3F9C77",
            "tx_json": {
                "Account": "zHb9CJAWyB4zj91VRWn96DkukG4bwdtyTh",
                "Amount": "100000000",
                "Destination": "zNRi42SAPegzJYzXYZfRFqPqUfGqKCaSbx",
                "Fee": "10",
                "Flags": 2147483648,
                "LastLedgerSequence": 4092,
                "Sequence": 3,
                "SigningPubKey": "0330E7FC9D56BB25D6893BA3F317AE5BCF33B3291BD63DB32654A313222F7FD020", 
                "TransactionType": "Payment",
                "TxnSignature": "3045022100EF12099B281DCD9EB7D23B020CCEA8817E598C4A0EB0DD7365A52B318B2778CB02205AAAAD30B2C3BE33E20334BEBCAB30B4217A21BBD7A7B0C80F13F93D257BB040",
                "hash": "138436054D740225C562D194AC8DF045E440C8F93FE0A8027956B8FD4FD6D0CF"
            }
        },
        "status": "success",
        "type": "response"
    }

应答域说明：

.. list-table::

    * - **域**
      - **类型**
      - **描述**
    * - tx_json
      - 对象
      - 加工后的完整交易的json格式。
    * - tx_blob
      - 字符串
      - 签名后的交易的16进制格式，用于提交交易。查看第二种\ `转账`_\ 方法。

数据库表交易
+++++++++++++++++++++++++++++

数据库表交易类型接口可以分为三种，TableListSet接口、SQLStatement接口、SQLTranscation接口。每种类型的接口对应不同的数据库操作语句。

TableListSet
=============================

TableListSet交易接口主要对应SQL的数据定义语句（DDL）和数据控制语句（DCL）。
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

.. note::

    以下交易的示例均只展示具体交易的JSON格式（tx_json对象）部分。都可通过本地签名后（或者使用secret域携带账户私钥）
    调用submit接口将交易发往全网共识。

请求格式：

.. code-block:: json

    {
        "TransactionType": "TableListSet",
        "Account": "zNRi42SAPegzJYzXYZfRFqPqUfGqKCaSbx",
        "Tables": [
            {
                "Table": { "TableName": "Table1" }
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

应答格式：

.. code-block:: json

    { 
        "resultCode": "tesSUCCESS",
        "resultMessage": "The transaction was applied. Only final in a validated ledger." 
    }

删除表
------------------------------

请求格式：

.. code-block:: json

    {
        "TransactionType": "TableListSet",
        "Account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
        "Tables": [
            {
                "Table": {
                    "TableName": "ExampleName",
                    "NameInDB": "48C80D2CF136054DB6F0116D4833D4DAD1D4CED5"
                }
            }
        ],
        "OpType": 2
    }

应答格式同上。

重命名表
------------------------------

.. code-block:: json

    {
        "TransactionType": "TableListSet",
        "Account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
        "Tables": [
            {
                "Table": {
                   "TableName": "ExampleName",
                    "TableNewName": "ExampleNameNew"
                }
            }
        ],
        "OpType": 3
    }

应答格式同上。

重建表
------------------------------

.. code-block:: json

    {
        "OpType": 12,
        "Tables": [
            {
                "Table": {
                    "TableName": "ExampleName",
                    "NameInDB": "48C80D2CF136054DB6F0116D4833D4DAD1D4CED5"
                }
            }
        ],
        "Fee": 12,
        "Sequence": 6
    }

应答格式同上。

表授权
------------------------------

.. code-block:: json

    {
        "TransactionType": "TableListSet",
        "Account ": "zf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
        "Tables": [
            {
                "Table": { "TableName": "ExampleName" }
            }
        ],
        "OpType": 11,
        "User": "zBGagHrWQ44SX3YE7eYehiDA8iNPdBssFY",
        "Raw": [
            {
                "select": true,
                "insert": true,
                "update": true,
                "delete": true
            }
        ]
    }

应答格式同上。

SQLStatement
=============================================

SQLStatement交易接口主要对应SQL的数据操纵语句（DML）。
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

.. code-block:: json

    {
        "TransactionType": "SQLStatement",
        "Account": "zf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
        "Owner": "zf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
        "Tables": [
            {
                "Table": {
                    "TableName": "ExampleName",
                    "NameInDB": "48C80D2CF136054DB6F0116D4833D4DAD1D4CED5"
                }
            }
        ],
        "Raw": [
            {
                "id": 1,
                "name": "test"
            },
            {
                "id": 2,
                "name": "hello"
            }
        ],
        "OpType": 6,
        "AutofillField": "txHash"
    }

应答格式同上。

更新记录
----------------------------------

.. code-block:: json

    {
        "TransactionType": "SQLStatement",
        "Account": "zf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
        "Owner": "zf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
        "Tables": [
            {
                "Table": {
                    "TableName": "ExampleName",
                    "NameInDB": "48C80D2CF136054DB6F0116D4833D4DAD1D4CED5"
                }
            }
        ],
        "Raw": [
      	    { "age": "11", "name": "abc" },
            { "id": 1 }
        ],
        "OpType": 8
    }
    
应答格式同上。

删除记录
---------------------------------

.. code-block:: json

    {
        "TransactionType": "SQLStatement",
        "Account": "zf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
        "Owner": "zf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
        "Tables": [
            {
                "Table": {
                    "TableName": "ExampleName",
                    "NameInDB": "48C80D2CF136054DB6F0116D4833D4DAD1D4CED5"
                }
            }
        ],
        "Raw": [
            { "id": 1 }
        ],
        "OpType": 9
    }

应答格式同上。

SQLTranscation
=================================================

事务处理可以用来维护数据库的完整性，保证成批的SQL语句要么全部执行，要么全部不执行。

SQLTranscation websocket交易接口的json格式（tx_json对象）各个域的描述如下：

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
      - 必填，支持的操作类型有：6:插入记录，8:更新记录，9:删除记录，10：验证断言，11:表授权。
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

请求示例：

.. code-block:: json

    {
        "TransactionType": "SQLTransaction",
        "Account": "zHb9CJAWyB4zj91VRWn96DkukG4bwdtyTh",
        "Statements": [
            {
                "Owner": "zHb9CJAWyB4zj91VRWn96DkukG4bwdtyTh",
                "Tables": [
                    {
                        "Table": {
                            "TableName": "EName"
                        }
                    }
                ],
                "OpType": 10,
                "Raw": [
                    {
                        "$IsExisted": 1
                    }
                ]
            },
            {
                "Owner": "zHb9CJAWyB4zj91VRWn96DkukG4bwdtyTh",
                "Tables": [
                    {
                        "Table": {
                            "TableName": "EName"
                        }
                    }
                ],
                "Raw": [
                    {
                        "id": 1,
                        "name": "PingGuo",
                        "age": 20
                    }
                ],
                "OpType": 6
            },
            {
                "Owner": "zHb9CJAWyB4zj91VRWn96DkukG4bwdtyTh",
                "Tables": [
                    {
                        "Table": {
                            "TableName": "Salarys"
                        }
                    }
                ],
                "Raw": [
                    {
                        "id": 1,
                        "salary": 20
                    }
                ],
                "OpType": 6
            },
            {
                "Owner": "zHb9CJAWyB4zj91VRWn96DkukG4bwdtyTh",
                "Tables": [
                    {
                        "Table": {
                            "TableName": "Salarys"
                        }
                    }
                ],
                "Flags": 65536,
                "OpType": 10,
                "Raw": [
                    {
                        "$RowCount": 1
                    },
                    {
                        "id": 1,
                        "salary": 20
                    }
                ]
            }
        ]
    }

\ 

 | 上例中，Statements数组中一共4个对象，每个对象分别对应一条语句，4个语句组成一个事务。下面分别说明：
 | 第一条为断言操作，Raw字段为 "$IsExisted": 1 判断EName表是否存在。
 | 第二条为插入操作，在表EName中增加一行。
 | 第三条也时插入操作，插入另一个表，在表Salary中插入一行。
 | 第四条为断言操作，Raw字段有 "$RowCount": 1 判断Salary表的总行数是否为1，或者存在id为1并且salary为20的一行。
 | 以上四条语句中只要某条语句执行失败，或者断言错误。整个事务就不会执行，并且回退到初始状态。

应答格式同上。

智能合约交易
++++++++++++++++++++++++++++++++++++++++

智能合约交易类型websocket接口包含部署合约接口和调用合约接口。
接口名使用\ ``submit``\ ，交易json格式（tx_json对象）各个域的描述如下：

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
      - UInt32
      - 可选，部署合约时，由部署者账户转账给合约账户的金额。
    * - Gas
      - 整形
      - UInt32
      - 必填，合约交易（部署和调用）消耗的Gas。

.. note::

    关于智能合约的编写和编译，参考solidity官方文档\ `introduction-smart-contracts <https://solidity.readthedocs.io/en/latest/index.html>`_\ 。

.. _websocket部署合约:

部署合约
==================================

请求格式：

.. code-block:: json

    {
        "TransactionType": "Contract",
        "ContractOpType": 1,
        "Account": "zNRi42SAPegzJYzXYZfRFqPqUfGqKCaSbx",
        "ContractData": "60806040526103af806100136000396000f3006080604052600436106100565763ffffffff7c010000000000000000000000000000000000000000000000000000000060003504166306fdde03811461005b57806317d7de7c146100e5578063c47f0027146100fa575b600080fd5b34801561006757600080fd5b506100706101af565b6040805160208082528351818301528351919283929083019185019080838360005b838110156100aa578181015183820152602001610092565b50505050905090810190601f1680156100d75780820380516001836020036101000a031916815260200191505b509250505060405180910390f35b3480156100f157600080fd5b5061007061023d565b34801561010657600080fd5b506101ad6004803603602081101561011d57600080fd5b81019060208101813564010000000081111561013857600080fd5b82018360208201111561014a57600080fd5b8035906020019184600183028401116401000000008311171561016c57600080fd5b91908080601f0160208091040260200160405190810160405280939291908181526020018383808284376000920191909152509295506102d4945050505050565b005b6000805460408051602060026001851615610100026000190190941693909304601f810184900484028201840190925281815292918301828280156102355780601f1061020a57610100808354040283529160200191610235565b820191906000526020600020905b81548152906001019060200180831161021857829003601f168201915b505050505081565b60008054604080516020601f60026000196101006001881615020190951694909404938401819004810282018101909252828152606093909290918301828280156102c95780601f1061029e576101008083540402835291602001916102c9565b820191906000526020600020905b8154815290600101906020018083116102ac57829003601f168201915b505050505090505b90565b80516102e79060009060208401906102eb565b5050565b828054600181600116156101000203166002900490600052602060002090601f016020900481019282601f1061032c57805160ff1916838001178555610359565b82800160010185558215610359579182015b8281111561035957825182559160200191906001019061033e565b50610365929150610369565b5090565b6102d191905b80821115610365576000815560010161036f5600a165627a7a72305820f14ef6e7c6d9774a8d3cf6744bc9569884132a7abce3011ff2de4c69a4e7adba0029",
        "ContractValue": 1,
        "Gas": 3000000
    }

应答与RPC部署合约一致，具体参考\ :ref:`RPC部署合约 <rpc部署合约>`\ 。

.. _websocket调用合约:

调用合约
==================================

请求格式：

.. code-block:: json

    {
        "TransactionType": "Contract",
        "Account": "zNRi42SAPegzJYzXYZfRFqPqUfGqKCaSbx",
        "ContractOpType": 2,
        "ContractData": "0xc47f0027000000000000000000000000000000000000000000000000000000000000002000000000000000000000000000000000000000000000000000000000000000047761776100000000000000000000000000000000000000000000000000000000",
        "Gas": 30000000,
        "ContractAddress": "zpzebvchnEafz5DDYzovuUsJeqy6mEoC5Q"
    }

具体参考\ :ref:`RPC调用合约 <rpc调用合约>`\ 。

查询类接口
************************************

查询类接口包含rippled原生的查询类接口、chainsqld新增的数据库记录和信息查询接口、
智能合约查询接口。

Rippled查询接口
+++++++++++++++++++++++++++++++++++++

rippled Websocket查询类接口有很多，详情请参看XRP官方开发文档 `rippled API Reference <https://developers.ripple.com/rippled-api.html>`_。 
下面展示一个示例。

查询账户信息
======================================

请求格式：

.. code-block:: json

    {
        "id": 2,
        "command": "account_info",
        "account": "zNRi42SAPegzJYzXYZfRFqPqUfGqKCaSbx",
        "strict": true,
        "ledger_index": "current",
        "queue": true
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
        "id": 2,
        "result": {
            "account_data": {
                "Account": "zNRi42SAPegzJYzXYZfRFqPqUfGqKCaSbx",
                "Balance": "100000000",
                "Flags": 0,
                "LedgerEntryType": "AccountRoot",
                "OwnerCount": 0,
                "PreviousTxnID": "2A5573C42CA73036A57AD823ACC4F0359D335FF067D6232EAB919AC2C130866E",
                "PreviousTxnLgrSeq": 266,
                "Sequence": 1,
                "index": "68D7B391587F7FD814AE718F6BE298AACDB6662DFABF21A13FD163CF9E0C9C14"
            },
            "ledger_current_index": 614,
            "queue_data": {
                "txn_count": 0
            },
            "validated": false
        },
        "status": "success",
        "type": "response"
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

SQL语句查询
==========================================

 | 通过SQL语句直接查询的接口支持复杂查询，多表联合查询等。
 | 为了区分权限，分为两个接口：
 | 管理员接口：需要在节点配置文件中配置ip为admin，才可调用，不检查调用者身份。
 | 普通用户接口：不需要配置admin，通过签名验证调用者身份。

请求格式：

admin接口：

.. code-block:: json

    {
        "command": "r_get_sql_admin",
        "sql": "select * from t_43ACD1FF143986210A44AB8B609371B392F45A86"
    }

.. note::

    SQL语句中的表名为数据库中的实际表名，需要先\ `获取实际表名`_\ 。

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
            "status": "success",
            "type":"response"
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

普通接口请参看Java和Node.js接口说明。

.. _获取实际表名:

查询实际表名
==========================================

请求格式：

.. code-block:: json

    {
        "command": "g_dbname",
        "account": "zNRi42SAPegzJYzXYZfRFqPqUfGqKCaSbx",
        "tablename": "Table2"
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
            "nameInDB": "43ACD1FF143986210A44AB8B609371B392F45A86"
        },
        "status": "success",
        "type": "response"
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

查询表的Token
===========================================

获取账户对应表的Token。

请求格式：

.. code-block:: json

    {
        "command": "g_userToken",
        "tx_json": {
            "Owner": "zNRi42SAPegzJYzXYZfRFqPqUfGqKCaSbx",
            "User": "zNRi42SAPegzJYzXYZfRFqPqUfGqKCaSbx",
            "TableName": "Table2"
        }
    }

参数说明：

.. list-table::

    * - **域**
      - **json类型**
      - **内部类型**
      - **描述**
    * - Owner
      - 字符串
      - String
      - 必填，表的创建者账户地址。
    * - User
      - 字符串
      - String
      - 必填，请求发起者账户地址。
    * - TableName
      - 字符串
      - String
      - 必填，创建表时，用户指定的表名。

应答格式：

.. code-block:: json

    {
        "result": {
            "token": ""
        },
        "status": "success", 
        "type": "response"
    }

应答域说明：

.. list-table::

    * - **域**
      - **json类型**
      - **内部类型**
      - **描述**
    * - token
      - 字符串
      - String
      - 16进制的密码的密文。
    * - status
      - 字符串
      - String
      - 请求处理成功或者失败。成功可以返回空的Token，表示表未加密，或者返回非空的Token。
        失败表示表加密了，但未对当前账户授权。
    * - error_message
      - 字符串
      - String
      - 错误信息。如果处理成功了，则被省略。

查询操作权限
===========================================

请求格式：

.. code-block:: json

    {
        "command": "table_auth",
        "owner": "zNRi42SAPegzJYzXYZfRFqPqUfGqKCaSbx",
        "tablename": "Table2"
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
        "status": "success",
        "type":"response"
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

.. _websocket查询合约:

智能合约查询
++++++++++++++++++++++++++++++++

查询示例
===============================

请求格式：

.. code-block:: json

    {
        "command": "contract_call",
        "account": "zHb9CJAWyB4zj91VRWn96DkukG4bwdtyTh",
        "contract_data": "17d7de7c",
        "contract_address": "zpzebvchnEafz5DDYzovuUsJeqy6mEoC5Q"
    }

具体参考\ :ref:`RPC查询合约 <rpc查询合约>`\ 。

订阅类接口
**************************

订阅需要客户端与Chainsqld服务节点之间维持一个长连接，客户端与服务器建立连接后，
可以订阅表，也可以订阅交易。

订阅表
++++++++++++++++++++++++++++++++

每当有修改已订阅表数据的交易，节点就会通过连接通道将交易发送给客户端。

请求格式：

.. code-block:: json

    {
        "id": "Example Subscribe a table",
        "command": "subscribe",
        "owner": "zNRi42SAPegzJYzXYZfRFqPqUfGqKCaSbx",
        "tablename": "Table2"
    }

参数说明：

.. list-table::

    * - **域**
      - **类型**
      - **描述**
    * - id
      - 字符串 
      - 对订阅的简要描述。
    * - command
      - 字符串
      - 订阅请求（命令）名为 subscribe。
    * - owner
      - 字符串
      - 必填，订阅表的所有者账户地址。
    * - tablename
      - 字符串
      - 必填，建表时，用户指定的表名。

应答格式：

.. code-block:: json

    {
        "id": "Example Subscribe a table",
        "result": {},
        "status": "success",
        "type": "response"
    }

订阅成功后每当有修改表数据的交易，节点就会通过通道将交易和交易状态的变化发送过来。

例如在已订阅的表中发送删除行请求，删除某一个行数据后，将收到如下消息：

.. code-block:: json

    {
        "owner": "zNRi42SAPegzJYzXYZfRFqPqUfGqKCaSbx",
        "status": "validate_success", 
        "tablename": "Table2",
        "transaction": {
            "Account": "zNRi42SAPegzJYzXYZfRFqPqUfGqKCaSbx", 
            "Fee": "29330",
            "Flags": 2147483648, 
            "LastLedgerSequence": 19579,
            "OpType": 9, 
            "Owner": "zNRi42SAPegzJYzXYZfRFqPqUfGqKCaSbx",
            "Raw": "5B0A2020207B0A20202020202022696422203A20330A2020207D0A5D0A",
            "Sequence": 3,
            "SigningPubKey": "027F92009A387B539099B0953DA21979CB002A9F142AA656E9AE5E263000040A43",
            "Tables": [
                {
                    "Table": {
                        "NameInDB": "43ACD1FF143986210A44AB8B609371B392F45A86",
                        "TableName": "5461626C6532"
                    }
                }
            ],
            "TransactionType": "SQLStatement",
            "TxnSignature": "3045022100DC1B1EB950A8EFC319296E2C6ABBF3D5E0D59369ADA05AAB176B8997E0939994022026850D33A21E64FD576108B33935B55348A153482210FA8E2157409CEBBB5FE8",
            "hash":"48DC88FE2A5293BB50286FB888BB84D7785E44557A37554292DE7B8F50E3F77C"
        },
        "type": "table"
    }

.. code-block:: json

    {
        "owner": "zNRi42SAPegzJYzXYZfRFqPqUfGqKCaSbx",
        "status": "db_success",
        "tablename": "Table2",
        "transaction": {
            "Account": "zNRi42SAPegzJYzXYZfRFqPqUfGqKCaSbx",
            "Fee": "29330",
            "Flags": 2147483648,
            "LastLedgerSequence": 19579,
            "OpType": 9,
            "Owner": "zNRi42SAPegzJYzXYZfRFqPqUfGqKCaSbx",
            "Raw": "5B0A2020207B0A20202020202022696422203A20330A2020207D0A5D0A",
            "Sequence": 3,
            "SigningPubKey": "027F92009A387B539099B0953DA21979CB002A9F142AA656E9AE5E263000040A43",
            "Tables": [
                {
                    "Table": {
                        "NameInDB": "43ACD1FF143986210A44AB8B609371B392F45A86",
                        "TableName": "5461626C6532"
                    }
                }
            ],
            "TransactionType": "SQLStatement",
            "TxnSignature": "3045022100DC1B1EB950A8EFC319296E2C6ABBF3D5E0D59369ADA05AAB176B8997E0939994022026850D33A21E64FD576108B33935B55348A153482210FA8E2157409CEBBB5FE8",
            "hash": "48DC88FE2A5293BB50286FB888BB84D7785E44557A37554292DE7B8F50E3F77C"
        },
        "type": "table"
    }

状态validate_success标识交易已共识成功， db_success标志数据库操作已成功。

订阅交易
++++++++++++++++++++++++++++++++

订阅一个具体的交易，当交易的状态发生改变时，节点就会通过通道将交易的状态发送给客户端。

请求格式：

.. code-block:: json

    {
        "id": "Example Subscribe a Transaction",
        "command": "subscribe",
        "transaction": "C317EF68120210BC9035D4B7DEBC885F0210C12955621F700BB3C40C2EC5B651"
    }

参数说明：

.. list-table::

    * - **域**
      - **类型**
      - **描述**
    * - id
      - 字符串 
      - 对订阅的简要描述。
    * - command
      - 字符串
      - 订阅请求（命令）名为 subscribe。
    * - transaction
      - 字符串
      - 交易的哈希，256位。

一般是在发送交易之前，先订阅这个即将要发送的交易。
这样能够监控到交易从提交到进入已验证账本过程中状态的变化。
需要事先计算交易的哈希，普通websocket客户端无法演示。
可以具体参考本文档中Java和Node.js接口部分。

应答格式同上。
