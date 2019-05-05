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

validation_create属于管理命令，通过椭圆曲线加密算法，使用种子生成非对称加密秘钥对（可以做为验证节点的证书）。

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

示例：

::

    ./chainsqld validation_create

返回结果：

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

server_info属于公共命令，用来查看节点的运行状态。

语法：

::

    ./chainsqld server_info

返回结果示例：

.. code-block:: json

    {
        "id" : 1,
        "result" : {
            "info" : {
                "build_version" : "0.30.3+DEBUG",
                "complete_ledgers" : "1-555",
                "hostid" : "a-virtual-machine",
                "io_latency_ms" : 1,
                "last_close" : {
                    "converge_time_s" : 2,
                    "proposers" : 0
                },
                "load" : {
                    "job_types" : [
                        {
                            "in_progress" : 1,
                            "job_type" : "clientCommand"
                        },
                        {
                            "avg_time" : 1,
                            "job_type" : "acceptLedger",
                            "peak_time" : 3
                        },
                        {
                            "job_type" : "peerCommand",
                            "per_second" : 1
                        }
                    ],
                    "threads" : 6
                },
                "load_factor" : 1,
                "peers" : 1,
                "pubkey_node" : "n9M6KKeKxpP61t63EW6cKKACyhGJyQSokDbA8ipHsZJWCv1dJ3Cq",
                "pubkey_validator" : "n9M15Yj6Jdao2Tnpn8pQe8CeDkFYXid1jJLV9cmHMZngpVCdcPkk",
                "server_state" : "proposing",
                "state_accounting" : {
                    "connected" : {
                        "duration_us" : "72050340",
                        "transitions" : 1
                    },
                    "disconnected" : {
                        "duration_us" : "1191980",
                        "transitions" : 1
                    },
                    "full" : {
                        "duration_us" : "2442353290",
                        "transitions" : 1
                    },
                    "syncing" : {
                        "duration_us" : "0",
                        "transitions" : 0
                    },
                    "tracking" : {
                        "duration_us" : "3",
                        "transitions" : 1
                    }
                },
                "validated_ledger" : {
                    "base_fee_zxc" : 1e-05,
                    "close_time_offset" : 18753,
                    "hash" : "2D1E46FAD9EC8AAD34E8B472F1556A56407528A8F8218081B1F7BB2E0CC4CC5C",
                    "reserve_base_zxc" : 5,
                    "reserve_inc_zxc" : 1,
                    "seq" : 555
                },
                "uptime" : 2428,
                "validation_quorum" : 2,
                "validator_list_expires" : "never"
            },
            "status" : "success"
        }
    }


.. _serverInfo-return:

结果说明：

.. list-table::

    * - **参数**
      - **类型**
      - **描述**
    * - build_version
      - 字符串
      - 节点运行的chainsqld版本。
    * - closed_ledger
      - 对象
      - 本节点中最近一个已关闭，并且还没有完成共识区块信息，
        如果最近一个关闭的区块已经完成了共识，那这个域将被省略，用validated_ledger代替。
    * - complete_ledgers
      - 字符串
      - 本节点上完整的区块序列，如果本节点上没有任何完整的区块
        （可能刚接入网络，正在于网络同步），则值为empty。
    * - load
      - 对象
      - 节点当前的负载详情。
    * - peers
      - 整形
      - 与本节点直接连接的其他chainsqld节点的数量。
    * - pubkey_node
      - 字符串
      - 节点与节点通信时，用来验证这个节点的公钥。节点在启动时自动生成的。
    * - pubkey_validator
      - 字符串
      - 该验证节点的公钥，有上面的validation_create命令生成。
    * - server_state
      - 字符串
      - 节点当前状态，可能的状态参考\ `节点状态<https://developers.ripple.com/rippled-server-states.html>`\ 。
    * - state_accounting
      - 对象
      - 节点在每个状态下的运行时长。
    * - validated_ledger
      - 对象
      - 最近完成共识的区块的信息。
        如果不存在，则会替换为closed_ledger域，表示最近关闭但还没有完成共识的区块信息。
    * - validated_ledger.base_fee_zxc
      - 整形
      - 账本的基本费用，交易、记账以这个数额为基础，单位：zxc。
    * - validated_ledger.reserve_base_zxc
      - 整形
      - 账户必须预留的费用。
    * - validated_ledger.reserve_inc_zxc
      - 整形
      - 账户每增加一个对象（比如一个表）需要额外预留的费用增加这个数值。
    * - validated_ledger.close_time_offset
      - 整形
      - 表示账本关闭多长时间了。
    * - validated_ledger.hash
      - 字符串
      - 区块的哈希。
    * - validated_ledger.seq
      - 整形
      - 区块的序号。
    * - uptime
      - 整形
      - 节点已运行时长。
    * - validation_quorum
      - 整形
      - 账本达成共识需要的验证数。
    * - validator_list_expires
      - 字符串
      - 新特性，验证节点列表相关的。

.. note::

    若返回结果中，字段\ ``complete_ledgers``\ 类似 "1-10"，则表示chainsqld服务启动成功。

peers
+++++++++++++++++++++++++++++++

peers属于管理命令，查看已连接的其他节点的连接状态和同步状态。

语法：

::

    ./chainsqld peers

返回结果示例：

.. code-block:: json

    {
        "id" : 1,
        "result" : {
            "cluster" : {},
            "peers" : [
                {
                    "address" : "127.0.0.1:5115",
                    "complete_ledgers" : "18850253 - 18851277",
                    "latency" : 0,
                    "ledger" : "5724E7C9B0E7B9E6D7F359A15B260216D896968C0BD782B94F423B10AE0B59FB",
                    "load" : 152,
                    "public_key" : "n9M6KKeKxpP61t63EW6cKKACyhGJyQSokDbA8ipHsZJWCv1dJ3Cq",
                    "uptime" : 4195,
                    "version" : "chainsqld-0.30.3+DEBUG"
                }
            ],
            "status" : "success"
        }
    }

结果说明：

.. list-table::

    * - **参数**
      - **类型**
      - **描述**
    * - cluster
      - 对象
      - 如果配置了集群，则返回集群中其他节点的信息。
    * - peers
      - 数组
      - 已连接的其他节点的连接状态和同步状态。
    * - address
      - 字符串
      - 对端节点与本节点连接使用的IP地址和端口号。
    * - complete_ledgers
      - 字符串
      - 对端节点中有哪些完整的账本。
    * - latency
      - 整数
      - 与对端节点的网络延迟。单位：毫秒。
    * - ledger
      - 字符串
      - 对端节点最后一个关闭的账本的哈希。
    * - load
      - 整数
      - 衡量对等服务器在本地服务器上加载的负载量。数字越大表示负载越大。（测量负载的单位未正式定义。）
    * - public_key
      - 字符串
      - 用来验真对端节点消息完整性的公钥。
    * - uptime
      - 整数
      - 对端节点自启动以来，连续运行的时长。单位：秒。
    * - version
      - 字符串
      - 对端节点运行的chainsqld版本。

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

.. code-block:: json

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

.. code-block:: json

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

.. code-block:: json

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

.. code-block:: json

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

.. code-block:: json

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

