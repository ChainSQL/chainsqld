配置文件详解
===============

.. _配置文件:

简介
---------
- ``.cfg`` 是chainsqld节点的配置文件，文件名默认为 chainsqld.cfg
- 文档中以'#'开头的内容是注释。
- 文档中以 ``[]`` 修饰一个配置项单元的名称
- 文档中需要空格的地方只允许一个空格，多于一个空格会识别不了

配置文件示例
----------------
.. code-block:: bash

    #端口配置列表
    [server]
    port_rpc_admin_local
    port_peer
    port_ws_admin_local

    #http端口配置
    [port_rpc_admin_local]
    port = 5005
    ip = 0.0.0.0
    admin = 127.0.0.1
    protocol = http

    #peer端口配置，用于p2p节点发现
    [port_peer]
    port = 5126
    ip = 0.0.0.0
    protocol = peer

    #websocket端口配置
    [port_ws_admin_local]
    port = 6006
    ip = 0.0.0.0
    admin = 127.0.0.1
    protocol = ws


    #-------------------------------------------------------------------------------
    # 缓存级别
    [node_size]
    medium

    # 区块数据存储配置，windows下用NuDB,Linux/Mac下用RocksDB
    [node_db]
    type=RocksDB
    path=./rocksdb
    open_files=2000
    filter_bits=12
    cache_mb=256
    file_size_mb=8
    file_size_mult=2

    #是否全节点
    [ledger_history]
    full

    #sqlite数据库（存储区块头数据，交易概要数据）
    [database_path]
    ./db

    # This needs to be an absolute directory reference, not a relative one.
    # Modify this value as required.
    [debug_logfile]
    ./debug.log

    #时间服务器，用于不同节点单时间同步
    [sntp_servers]
    time.windows.com
    time.apple.com
    time.nist.gov
    pool.ntp.org

    # 要连接的其它节点的Ip及端口
    [ips]
    127.0.0.1 5127
    127.0.0.1 5128

    # 信任节点列表（信任节点的公钥列表）
    [validators]
    n94ngNasveyfF2KttLuNni6nHPcUtw1Se3969nUginy8cf2Kzb4Z
    n9LacsFGc9VrpAXZidEeAipLchuC2r7wPV243ugR2KDA4En818sM

    # 本节点私钥（如不配置，不参与共识）
    [validation_seed]
    xpvEo9rKgjc6uabELVRymX9scieGC

    # 本节点公钥
    [validation_public_key]
    n9Ko6z3Ua9ShbTaTdqr1x457vaHzAnsrQFLH5uo1Mtu6pgE6

    #日志级别，一般设置为warning级别
    [rpc_startup]
    { "command": "log_level", "severity": "warning" }

    #禁用某些支持但未不需要启用的特性
    [veto_amendments]
    42EEA5E28A97824821D4EF97081FE36A54E9593C6E4F20CBAE098C69D2E072DC fix1373
    740352F2412A9909880C23A559FCECEDA3BE2126FED62FC7660D628A06927F11 Flow
    E2E6F2866106419B88C50045ACE96368558C345566AC8F2BDF5A5B5587F0E6FA fix1368
    C6970A8B603D8778783B61C0D445C23D1633CCFAEF0D43E7DBCD1521D34BD7C3 SHAMapV2
    C1B8D934087225F509BEB5A8EC24447854713EE447D277F69545ABFA0E0FD490 Tickets
    86E83A7D2ECE3AD5FA87AB2195AE015C950469ABF0B72EAACED318F74886AE90 CryptoConditionsSuite
    1562511F573A19AE9BD103B5D6B9E01B3B46805AEC5D3C4805C902B514399146 CryptoConditions
    3012E8230864E95A58C60FD61430D7E1B4D3353195F2981DC12B0C7C0950FFAC FlowCross


    #chainsql数据库配置，根据自己的机子
    [sync_db]
    type=mysql
    host=localhost
    port=3306
    user=root
    pass=123456
    db=chainsql
    first_storage=0
    charset=utf8

    # 开户自动同步后，节点运行情况下会去自动同步新建的表，开启这个开关，或者使用sync_tables标签的配置，否则无法同步表
    [auto_sync]
    1

配置项说明
----------------
[server]
************

- 端口列表，chainsqld 会查找文件中具有与列表项相同名称的配置项，并用这些配置荐创建监听端口。
- 列表中配置项的名称不会影响功配置功能

单个配置项示例如下：

.. code-block:: bash

    [port_rpc_admin_local]
    port = 5005
    ip = 0.0.0.0
    admin = 127.0.0.1
    protocol = http

每个配置项包含如下内容：

    - ``port`` 配置端口
    - ``ip`` 哪些ip可以连接这一端口，如果有多个，以逗号（,）进行分隔， ``0.0.0.0`` 代表任意ip可以连接这一端口
    - ``admin`` chainsql中有一些命令（如peers,t_dump,t_audit）只有拥有admin权限的ip才能调用，配置方法与 ip 相同
    - ``protocol`` 协议名称，chainsql中支持协议有 http,https,ws,wss,peer

[node_size]
**************
    | 缓存大小，可设置的值有 "tiny", "small", "medium", "large","huge"，我们建议一开始设置一个默认值，如果运行一段时间发现还有内存空余，则将缓存增大。
    | 默认值为"tiny"。

[node_db]
**************
    Chainsql中创建了4个sqlite数据库来存储交易、区块等信息的索引，完整的区块内容存储到node_db配置项中

示例：

.. code-block:: bash

    [node_db]
    type=RocksDB
    path=./rocksdb
    online_delete=2000
    advisory_delete=0

配置项的具体内容：

- ``type`` node_db可选类型有两种，一个是 NuDB（平台兼容，可运行在linux/windows)，一个是 RocksDB（只支持linux）
- ``path`` node_db 可配置绝对路径也可以配置相对路径（相对于当前配置文件）

可选配置项（用于开启非全节点）：

- ``online_delete`` 最小值为256，节点最小维持的区块数量，这个值不能小于 ``ledger_history`` 配置项的值
- ``advisory_delete`` 0为禁用，1为启用。如果启用了，需要调用admin权限接口 ``can_delete`` 来开启区块的在线删除功能。

[ledger_history]
*****************
- 节点要维护的最小历史区块数量
- 若不想维护历史区块，则设置为 ``none`` ,若想维护全部历史区块（全节点），则设置为 ``full`` 还可以设置为一个数字
- 默认值为256，如果 [node_db]中有 ``online_delete`` 配置项，[ledger_history] 的值必须 <= ``online_delete`` 的值

**配置非全节点的示例** :

.. code-block:: bash

    [node_db]
    type=RocksDB
    path=/data/chainsql/db1/rocksdb
    open_files=2000
    filter_bits=12
    cache_mb=256
    file_size_mb=8
    file_size_mult=2
    online_delete=2000
    advisory_delete=0

    #[ledger_history]
    #full


[database_path]
******************
    sqlite 数据库的存储路径，可以是全路径 ，也可以是相对路径（相对于当前配置文件路径）

[debug_logfile]
******************
    日志文件路径

[sntp_servers]
******************
    时间服务器，用于p2p节点间时间同步

.. important:: 

    在内网环境中，公网的时间服务器连不上，这时必须配置内网的时间服务器或手动将节点的时间调节一致，不然会出现节点发现不了，或者达不成共识等各种 问题

[ips]
******************
    | 要连接的其它节点的Ip及端口，一行只允许出现一个ipv4地址及端口
    | 配置的端口为要连接节点的peer协议端口

[validators]
******************
    信任节点列表（信任节点的公钥列表）

[validation_seed]
********************
    本节点私钥（如不配置，不参与共识，为非共识节点）

[validation_public_key]
***************************
    本节点公钥（可选），如果配置了validation_seed，会由 validation_seed 生成节点公钥

[rpc_startup]
*****************
    节点启动时要执行的JSON-RPC命令，一般用于设置日志级别：

.. code-block:: json

    {
        "command":"log_level",
        "severity":"warning"
    }

日志级别包括：trace, debug, info, warning, error, fatal

[sync_db]
*****************
    配置同步表相关交易用的数据库，原生支持mysql,sqlite，可通过 mycat 支持其它数据库，示例如下:

.. code-block:: bash
    
    [sync_db]
    type=mysql
    host=localhost
    port=3306
    user=root
    pass=root
    db=chainsql
    first_storage=0 #可选
    charset=utf8 #可选
    unix_socket=/var/lib/mysql/mysql.sock #可选

    [sync_db]
    type=sqlite
    db=chainsql

配置项说明：
    - ``type`` 数据库类型，这里支持sqlite,mysql,mycat，配置mysql等同于mycat
    - ``host`` 连接数据库用的主机,localhost或127.0.0.1表示本机
    - ``port`` 数据库端口
    - ``user`` 登录数据库的用户名
    - ``pass`` 登录数据库的密码
    - ``db``   数据库名称
    - ``first_storage`` 是否开启先入库，0为不开启，1为开启，默认为0
    - ``charset`` 数据库编码
    - ``unix_socket`` 
        | 使用localhost连接时，会默认使用 sock 方式连接，默认sock路径是 /var/run/mysqld/mysqld.sock。
        | 在非ubuntu系统中，这个路径是不对的，会导致连接数据库失败，需要用 unix_socket 选项来指定 sock 路径
        | 如果host写为ip（如127.0.0.1）去连接，会使用 tcp 方式连接，就不会有这个问题

[auto_sync]
********************
是否开启表自动同步，可设置为0或1，默认为0

.. important:: 

    auto_sync只影响表的创建，如auto_sync为0，在创建新表的交易共识过后，不会在数据库中建表。但是如果表已经存在，这时向表中插入数据，是不受auto_sync值影响的。

[sync_tables]
*********************
    配置要同步的表，详细配置方法参考 :ref:`sync_tables <表同步设置>`，这里与auto_sync的不同在于：

    - auto_sync 配置为1，只能同步新建的表，而 sync_tables 还可以同步之前区块上建的表
    - sync_tables 可中配置同步加密表所用的解密私钥，加密表只有通过sync_tables的配置才可以同步下来
    - sync_tables 可配置各种同步条件，如同步到某个区块，同步到某个时间，跳过某个区块等

[voting]
***************
    配置账户预留费用或对象增加费用，示例如下:

.. code-block:: bash

    [voting]
    account_reserve = 10000000
    owner_reserve = 1000000

配置项说明：

    - account_reserve 账户预留费用，指的是激活一个账户所需要的最小系统币（ZXC）数量，也是一个账户的余额要保留的最小值，单位为drop，上面的配置表示账户预留费用为10ZXC。
    - owner_reserve 增加一个对象，要增加的预留费用，这里的对象指的是要占用链上存储的对象，如账户与网关之间的trustline,账户新建的表等，上面的值表示每增加一个对象，账户的预留费用要增加1ZXC。

[features]
**************
    要在节点启动时就在本节点启用的特性，特性的具体介绍参考 :ref:`features <amendments>` ,这里不再赘述。

[veto_amendments]
********************
    要禁用的特性，在特性启用前禁用，会给特性的开启投反对票，赞成票低于80%，会导致特性无法开启，详情参考：:ref:`features <amendments>`