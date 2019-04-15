命令行接口
############################

chainsqld和rippled一样，可以通过指定一个API接口方法名来启动RPC客户端模式。
也可以查询服务节点状态和账本数据。
通过配置文件或者命令行参数指定服务节点的地址和端口。

rippled命令
*****************************

rippled原生的命令行接口很多，详情请参看XRP官方开发文档\ `rippled CommandLine Usage <https://developers.ripple.com/commandline-usage.html>`_\ 。 
下面展示几个常用的命令。

validation_create
+++++++++++++++++++++++++++++++

通过椭圆曲线加密算法，使用种子生成非对称加密秘钥对（可以做为验证节点的证书）。

语法：

::

    ./chainsqld --conf="./chainsqld-example.cfg"  validation_create [secret]

.. note::

    选项 --conf执行配置文件路径，如不指定，则使用当前路径下的\ ``chainsqld.cfg``\ 。

参数说明：

.. list-table::

    * - **参数**
      - **类型**
      - **描述**
    * - secret
      - 字符串
      - 秘钥种子，如果省略，则使用随机的种子，相同的种子生成相同的证书。

返回结果示例：

::

    {
        "status" : "success",
        "validation_key" : "TUCK NUDE CORD BERN LARD COCK ENDS ETC GLUM GALE CASK KEG",
        "validation_public_key" : "n9L9BaBQr3KwGuMoRWisBbqXfVoKfdJg3Nb3H1gjRSiM1arQ4vNg",
        "validation_seed" : "xxjX5VuTjQKvkTSw6EUyZnahbpgS1"
    }

结果说明：

.. list-table::

    * - **参数**
      - **类型**
      - **描述**
    * - status
      - 字符串
      - 标识命令是否执行成功。
    * - validation_key
      - 字符串
      - 证书私钥 RFC-1751格式。
    * - validation_public_key
      - 字符串
      - 证书公钥 Base58编码。
    * - validation_seed
      - 字符串
      - 证书私钥 Base58编码。

server_info
+++++++++++++++++++++++++++++++

用来查看节点的运行状态。

语法：

::

    ./chainsqld server_info

若返回结果中，字段\ ``complete_ledgers``\ 类似 "1-10"，则表示chainsqld服务启动成功。

peers
+++++++++++++++++++++++++++++++

查看已连接的其他节点的运行情况。

语法：

::

    ./chainsqld peers

返回结果中包含集群（如果配置了集群）信息和其他节点的信息。

wallet_propose
+++++++++++++++++++++++++++++++

生成一个账户地址和秘钥对，之后必须通过转账交易，发送足够的ZXC给该账户，才能使账户真正进入账本。

语法：

::

    ./chainsqld wallet_propose [passphrase]

参数说明：

.. list-table::

    * - **参数**
      - **类型**
      - **描述**
    * - passphrase
      - 字符串
      - 秘钥种子，如果省略，则使用随机的种子，相同的种子生成相同的账户地址和证书。

返回结果示例：

::

    {
        "result" : {
            "account_id" : "rHb9CJAWyB4rj91VRWn96DkukG4bwdtyTh",
            "key_type" : "secp256k1",
            "master_key" : "I IRE BOND BOW TRIO LAID SEAT GOAL HEN IBIS IBIS DARE",
            "master_seed" : "snoPBrXtMeMyMHUVTgbuqAfg1SUTb",
            "master_seed_hex" : "DEDCE9CE67B451D852FD4E846FCDE31C",
            "public_key" : "aBQG8RQAzjs1eTKFEAQXr2gS4utcDiEC9wmi7pfUPTi27VCahwgw",
            "public_key_hex" : "0330E7FC9D56BB25D6893BA3F317AE5BCF33B3291BD63DB32654A313222F7FD020",
            "status" : "success"
        }
    }

结果说明：

.. list-table::

    * - **参数**
      - **类型**
      - **描述**
    * - status
      - 字符串
      - 标识命令是否执行成功。
    * - account_id
      - 字符串
      - 生成的账户地址。
    * - master_seed
      - 字符串
      - 账户的种子（私钥）。
    * - public_key
      - 字符串
      - 账户的公钥。

chainsqld命令
*****************************

t_dump
+++++++++++++++++++++++++++++++

将数据库表的操作以文档的形式进行记录，可以分多次对同一张表进行dump。

语法：

::

    chainsqld t_dump <param> <out_file_path>

示例：

::

    ./chainsqld t_dump "zNRi42SAPegzJYzXYZfRFqPqUfGqKCaSbx Table1 262754" ./Table1.dump

参数说明：

.. list-table::

    * - **参数**
      - **类型**
      - **描述**
    * - param
      - 字符串
      - 与数据库表的同步设置保持一致。详情参见数据库表同步设置。
    * - out_file_path
      - 字符串
      - 输出文件路径。

返回结果：

::

    {
        "id" : 1,
        "result" : {
            "command" : "t_dump",
            "status" : "success",
            "tx_json" : [
                "zNRi42SAPegzJYzXYZfRFqPqUfGqKCaSbx Table1 262754",
                "./table1.dmp"
            ]
        }
    }

t_dumpstop
+++++++++++++++++++++++++++++++

停止dump一张表。

语法：

::

    chainsqld t_dump <owner_address> <table_name>

参数说明：

.. list-table::

    * - **参数**
      - **类型**
      - **描述**
    * - owner_address
      - 字符串
      - 表的创建者账户地址。
    * - table_name
      - 字符串
      - 表名。

返回结果示例：

::

    {
        "id" : 1,
        "result" : {
            "command" : "t_dumpstop",
            "status" : "success",
            "tx_json" : [ 
                "zNRi42SAPegzJYzXYZfRFqPqUfGqKCaSbx", 
                "Table1" 
            ]
        }
    }

t_audit
+++++++++++++++++++++++++++++++

对数据库表的指定记录（由SQL查询条件指定）的一列或多列进行追根溯源，将所有影响了指定记录的列的操作都记录下来。

语法：

::

    chainsqld t_audit <param> <sql_query_statement> <out_file_path>

示例：

::

    ./chainsqld t_audit "zNRi42SAPegzJYzXYZfRFqPqUfGqKCaSbx Table1 262754" "select * from Table1 where id=1" ./Table1.audit

参数说明：

.. list-table::

    * - **参数**
      - **类型**
      - **描述**
    * - param
      - 字符串
      - 与数据库表的同步设置保持一致。详情参见数据库表同步设置。
    * - sql_query_statement
      - 字符串
      - 由SQL语句指定审计的记录和列。
    * - out_file_path
      - 字符串
      - 输出文件路径。

返回结果：

::

    {
        "id" : 1,
        "result" : {
            "command" : "t_audit",
            "nickName" : "5C9DD983025F6F654EA23FAFC0ADFC1BD0CAF58E",
            "status" : "success",
            "tx_json" : [
                "zNRi42SAPegzJYzXYZfRFqPqUfGqKCaSbx Table1 263498",
                "select * from Table1 where id=1",
                "./Table1.audit"
            ]
        }
    }

结果说明：

.. list-table::

    * - **参数**
      - **类型**
      - **描述**
    * - nickName
      - 字符串
      - 审计任务名称，用来停止审计任务。

t_auditstop
+++++++++++++++++++++++++++++++

停止审计。

语法：

::

    chainsqld t_auditstop <nickname>

参数说明：

.. list-table::

    * - **参数**
      - **类型**
      - **描述**
    * - nickname
      - 字符串
      - 启动审计任务时，返回的审计任务名。

返回结果：

::

    {
        "id" : 1,
        "result" : {
            "command" : "t_auditstop",
            "status" : "success",
            "tx_json" : [ 
                "5C9DD983025F6F654EA23FAFC0ADFC1BD0CAF58E"
            ]
        }
    }

