======================
Java接口
======================

ChainSQL提供JAVA-API与节点进行交互。实现ChainSQL区块链的基础交易发送、链数据查询、数据库操作、智能合约操作等交互操作。

环境准备
*****************

获取依赖的jar包
=====================


* 如果本地有maven环境，将以下代码加入本地开发环境pom.xml文件进行jar包下载。

.. code-block:: xml

  <dependency>
    <groupId>com.peersafe</groupId>
    <artifactId>chainsql</artifactId>
    <version>1.5.1</version>
  </dependency>


- 如果本地没有maven环境，可以直接下载项目依赖的jar包。项目依赖的jar包下载: `下载地址 <http://www.chainsql.net/libs.zip>`_

  *-* Eclipse。在"buildPath"中选择"libraries"，再Add External Jars 添加相应的jar包。

  *-* IDEA。在"Project Structure"-"Modules"-"Dependencies"中添加相应的jar包。



------------------------------------------------------------------------------

引入
=====================

使用下面的代码引入ChainSQL JAVA-API

.. code-block:: java

    import com.peersafe.chainsql.core.Chainsql;
    import com.peersafe.chainsql.core.Submit.SyncCond;
    import com.peersafe.chainsql.util.Util;

------------------------------------------------------------------------------

接口说明
*****************

版本变化
=====================

    - 1.5.1版本之前的版本对多线程的支持不好,新版本支持多线程中调用。
    - 1.5.1版本之前 ``pay`` 方法直接调会返回交易提交结果，而新版本需要在方法后接``.submit`` 指定是否共识成功返回。具体示例见 :ref:`示例 <my-reference-pay-sample>`.
    - 1.5.1版本之前对象可以使用Chainsql静态对象： ``Chainsql c = Chainsql.c`` ,现在删除了静态对象，需要用户自己调用 ``new`` ，例如
      ``Chainsql c = new Chainsql();``

------------------------------------------------------------------------------

接口返回格式
=====================

------------
交易类接口
------------

交易类的接口包括网关交易以及表交易等相关的一系列接口。

.. _json-return:

交易类接口返回的JSON包含的各个域如下：

.. list-table::

    * - **域**
      - **类型**
      - **描述**
    * - tx_hash
      - 字符串
      - 交易的哈希值
    * - status
      - 字符串
      - 标识交易是否已被服务节点成功接收并且解析成功。
    * - tx_json
      - 对象
      - 签名后的交易的JSON格式。
    * - engine_result_code
      - 整形
      - 与engine_result关联的整形值。
    * - engine_result_message
      - 字符串
      - 交易状态结果的描述。
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

.. code-block:: json

    {
        "tx_hash":"CC552C401D7E0393A240D7441C4C4870DD5723F5D9306F89BA9848BDA6ED4816",
        "status":"db_success"
    }

失败示例

.. code-block:: json

      {
          "error_message":"Insufficient ZXC balance to send.",
          "tx_json":{
              "Account":"z3VGAJo59RWZ23CeM7zwMGVvsbWF8HabHf",
              "Destination":"zHb9CJAWyB4zj91VRWn96DkukG4bwdtyTh",
              "TransactionType":"Payment",
              "TxnSignature":"3045022100E3D6BE3070FBF7B7F891DD6A724F0EFA77A9BF0E440C739CF898F0E44FB25273022001D82BE41146FBA21F20145A14132943623632C33FB0844CD035FAE38086DB83",
              "SigningPubKey":"032B50FB18E894BB74B56A18A482C113219598650AE465F61161663D15C0EC4215",
              "Amount":"2000000000",
              "Fee":"11",
              "Flags":2147483648,
              "Sequence":2,
              "LastLedgerSequence":5116,
              "hash":"8441BD3D6777A647B0D63DEC5D37C8AF67DE38248D7045E3900E14B3AE56DC8B"
          },
          "error_code":104,
          "error":"tecUNFUNDED_PAYMENT",
          "status":"error"
      }

------------------------------------------------------------------------------

------------
查询类接口
------------

查询类的接口包括账户相关的信息查询以及表查询等相关的一系列接口。

查询类接口返回的JSON包含的各个域如下：

.. list-table::

    * - **域**
      - **类型**
      - **描述**
    * - status
      - 字符串
      - 标识交易是否已被服务节点成功接收并且解析成功。
    * - request
      - 对象
      - 查询接口的JSON格式。
    * - final_result
      - 整形
      - 与final_result关联的整形值。
    * - error
      - 字符串
      - 如果交易请求解析或者处理出错，返回错误类型码。
    * - error_code
      - 字符串
      - 与error关联的整形值。
    * - type
      - 字符串
      - 返回类型      
    * - error_message
      - 字符串
      - 错误原因的描述。

成功示例

.. code-block:: json

    {
        "ledger_current_index":5397,
        "validated":false,
        "account_data":{
              "Account":"zHb9CJAWyB4zj91VRWn96DkukG4bwdtyTh",
              "OwnerCount":0,
              "PreviousTxnLgrSeq":4988,
              "LedgerEntryType":"AccountRoot",
              "index":"2B6AC232AA4C4BE41BF49D2459FA4A0347E1B543A4C92FCEE0821C0201E2E9A8",
              "PreviousTxnID":"E796A7B6A53900928F8016880A1593CD627B1FD4909532BBDDEBE274B815A4B2",
              "Flags":0,
              "Sequence":44,
              "Balance":"99999984779956509"
        }
    }

失败示例

.. code-block:: json

      {
            "error_message":"Table does not exist.",
            "request":{
            "signature":"3045022100A72D868B70FE66FDCD78DE2F411C751549B2A3F5D438AA14ABA251E8FFCE90B2022061C52B78311E6F2669E0293225602C3597942751160E82B4C852920AB7DDC76C",
            "tx_json":{
                  "LedgerIndex":7432,
                  "Account":"zHb9CJAWyB4zj91VRWn96DkukG4bwdtyTh",
                  "Owner":"zHb9CJAWyB4zj91VRWn96DkukG4bwdtyTh",
                  "Raw":"[[], {\"name\":\"hello\"}]",
                  "Tables":[
                    {
                      "Table":{
                      "TableName":"tTable2"
                      }
                    }
                  ]
                  },
                  "id":2,
                  "publicKey":"0330E7FC9D56BB25D6893BA3F317AE5BCF33B3291BD63DB32654A313222F7FD020",
                  "signingData":"{\"LedgerIndex\":7432,\"Account\":\"zHb9CJAWyB4zj91VRWn96DkukG4bwdtyTh\",\"Owner\":\"zHb9CJAWyB4zj91VRWn96DkukG4bwdtyTh\",\"Tables\":[{\"Table\":{\"TableName\":\"tTable2\"}}],\"Raw\":\"[[], {\\\"name\\\":\\\"hello\\\"}]\"}",
                  "command":"r_get"
            },
            "final_result":true,
            "error_code":76,
            "id":2,
            "error":"tabNotExist",
            "type":"response",
            "status":"error"
      }

------------------------------------------------------------------------------

基础接口
*****************

as
=====================

.. code-block:: java

  public void as(String address, String secret);

提供操作账户信息，指明一个全局的操作账户。

.. warning::
    部分接口与节点进行交互操作前，需要指明一个全局的操作账户，这样避免在每次接口的操作中频繁的提供账户。再次调用该接口即可修改全局操作账户。

------------
参数
------------

1. ``address``  - ``String``: 账户地址.
2. ``secret ``  - ``String``: 账户私钥

-------
返回值
-------

-------
示例
-------

.. code-block:: java

    //提供操作账户信息
    c.as("zP8Mum8xaGSkypRgDHKRbN8otJSzwgiJ9M", "xcUd996waZzyaPEmeFVp4q5S3FZYB");

------------------------------------------------------------------------------

use
=====================

.. code-block:: java

  public void use(String address);

use接口主要使用场景是针对ChainSQL的表操作，为其提供表的 **拥有者账户地址** 。use接口和as接口的区别如下：

- as接口的目的是设置全局操作账户GU，这个GU在表操作接口中扮演表的操作者，并且默认情况下也是表的拥有者（即表的创建者）。
- 当as接口设置的账户不是即将操作的表的拥有者时，即可能通过表授权获得表的操作权限时，需要使用use接口设置表的拥有者地址。
- use接口只需要提供账户地址即可，设置完之后，表的拥有者地址就确定了，再次使用use接口可以修改表拥有者地址。

------------
参数
------------

1. ``address`` - ``String``: 表的拥有者的账户地址

-------
返回值
-------

-------
示例
-------

.. code-block:: java

    c.use("zP8Mum8xaGSkypRgDHKRbN8otJSzwgiJ9M");

------------------------------------------------------------------------------

.. note:: 进行表操作前必须调用use,来指定表的所有者。因为不同用户可能存在同名的表名

connect
=====================

.. code-block:: java

    public Connection connect(String url);
    public Connection connect(String url,String serverCertPath,String storePass);
    public Connection connect(String url,final Callback<Client> connectCb);
    public Connection connect(String url,final Callback<Client> connectCb,final Callback<Client> disconnectCb);
    public Connection connect(String url,String serverCertPath,String storePass,final Callback<Client> connectCb);
    public Connection connect(String url,String serverCertPath,String storePass,final Callback<Client> connectCb,
                       final Callback<Client> disconnectCb);

连接一个 ``websocket`` 地址.如果需要与节点进行交互，必须设置该节点的websocket地址。

------------
参数
------------

1. ``url`` - ``String``: 节点的websocket访问地址,格式为:"ws://127.0.0.1:5006".
2. ``serverCertPath`` - ``String``: 认证路径.
3. ``storePass`` - ``String``: 认证密码
4. ``connectCb`` - ``Callback<Client>``: 已连接后的回调
5. ``disconnectCb`` - ``Callback<Client>``: 断开连接后的回调

-------
返回值
-------

``Connection`` - 连接后的对象

-------
示例
-------

.. code-block:: java

    // 同步连接
    // 如果无法建立连接,会抛出java.net.ConnectException;
    String url = "ws://192.168.0.162:6006";
    c.connect(url);

.. code-block:: java

  // 异步连接
  c.connect("ws://127.0.0.1:6006", new Callback<Client>() {
    @Override
    public void called(Client args) {

      System.out.println("Connected");

    }
  }, new Callback<Client>() {
    @Override
    public void called(Client args) {

      System.out.println("Disconnected  ");

    }
  });

------------------------------------------------------------------------------

submit
=====================

.. code-block:: java

  public JSONObject submit();
  public JSONObject submit(Callback cb)
  public JSONObject submit(SyncCond cond);

submit有3个重载函数，对应异步和同步，客户可以根据需要填写参数。返回值均为JSON对象，指示成功或失败;

针对ChainSQL的交易类型的操作，需要使用submit接口执行提交上链操作，交易类型的操作是指需要进行区块链共识的操作。
还有一类ChainSQL的查询类操作，不需要使用submit接口，不需要进行区块链共识。
submit接口有使用前提，需要事先调用其他操作接口将交易主体构造，比如创建数据库表，需要调用createTable接口，然后调用submit接口，详细使用方法在具体接口处介绍。

------------
参数
------------

1. ``cb``   - ``Callback``: 异步接口，参数为一回调函数
2. ``cond`` - ``SyncCond``: 同步接口，参数为一枚举类型;

.. code-block:: java

  enum SyncCond {
      send_success,    // 交易提交成功
      validate_success,// 交易共识通过
      db_success       // 交易成功同步到数据库
  };

-------
返回值
-------

``JSONObject`` - JSON对象.可参考 :ref:`接口返回值 <json-return>`

1. 执行成功，则 ``JsonObject`` 中包含两个字段：

  * ``status`` - ``String`` : 为提交时的设定值，如果没有，则默认为**send_success**
  * ``tx_hash`` - ``String`` : 交易哈希值，通过该值可以在链上查询交易

2. 执行失败，有以下几种情况:

  * 第一种交易共识前的字段信息有效性检测出错，``JsonObject`` 中主要包含以下字段：

    - ``error`` - ``String`` : 错误类型；
    - ``error_message`` - ``String`` : 错误具体描述。

  * 第二种交易提交之后共识出错，``JsonObject`` 中包含以下字段：

  - ``error`` - ``String`` : 错误类型或者说错误码，可参考 :ref:`交易类错误码 <tx-errcode>`；
  - ``error_message`` - ``String`` : 错误具体描述。

  * 第三种交易提交共识后出错，主要是数据库入库操作中的错误，``JsonObject`` 中包含以下字段：

    - ``status`` - ``String`` : 错误类型，有以下可能字段：

      - db_error
      - validate_timeout
      - db_noTableExistInDB
      - db_noDbConfig
      - db_noSyncConfig
      - db_noAutoSync
      - db_noTableExistInDB
    - ``tx_hash`` - ``String`` : 交易哈希值。
    - ``error_message`` - ``String`` : [**可选**]在错误类型为"db_error"的时候，会额外附加错误信息。

示例

.. code-block:: java

  // 1、
  c.table("marvel")
  .insert(c.array("{'name': 'peera','age': 22}","{'name': 'peerb','age': 21}"))
  .submit();

  // 2、
  c.table("marvel").insert(c.array("{'name': 'peera','age': 22}", "{'name': 'peerb','age': 21}"))
  .submit(new Callback () {
  public void called(JSONObject data) {
    System.out.println(data);
  }));

  // 3、
  c.table("marvel").insert(c.array("{'name': 'peera','age': 22}", "{'name': 'peerb','age': 21}"))
  .submit(SyncCond.db_success);
  pay(转账系统币)

------------------------------------------------------------------------------

.. _pay-系统币:

pay(转账系统币)
=====================

.. code-block:: java

  public Ripple pay(String accountId, String value);

给用户转账,新创建的用户在转账成功之后才能正常使用(激活)。

.. note::
    这个函数有重载，除了可以转账系统币外，还可以转账发行的代币：:ref:`pay(转账代币) <pay-代币>`

.. warning::
    1.5.0版本之前 pay 方法直接调完就行，现在需要.submit

------------
参数
------------

1. ``accountId``   - ``String``: 接收转账方地址
2. ``value``       - ``String``: 转账金额（单位:ZXC）,默认情况下，最小需要5个ZXC才能激活一个账户;

-------
返回值
-------

``Ripple`` - Ripple对象,后面一般接submit进行连续操作,如示例。


.. _my-reference-pay-sample:

-------
示例
-------

.. code-block:: java

  // 给账户地址等于 z9VF7yQPLcKgUoHwMbzmQBjvPsyMy19ubs 的用户转账5ZXC.
  c.pay("z9VF7yQPLcKgUoHwMbzmQBjvPsyMy19ubs", "5").submit(SyncCond.validate_success);

------------------------------------------------------------------------------

generateAddress
=====================

.. code-block:: java

  public JSONObject generateAddress();

生成一个ChainSQL账户，但是此账户未在链上有效，需要链上有效账户对新账户发起pay操作，新账户才有效。

-------
返回值
-------

``JSONObject`` - JSON对象 

    * ``address`` - ``String`` : 新账户地址，是原始十六进制的base58编码；
    * ``publicKey`` - ``String`` : 新账户公钥，是原始十六进制的base58编码；
    * ``secret`` - ``String`` : 新账户私钥，是原始十六进制的base58编码。

-------
示例
-------

.. code-block:: java

    JSONObject json = c.generateAddress();

输出:

.. code-block:: json    

     {
       "secret":"xcUd996waZzyaPEmeFVp4q5S3FZYB",
       "address":"zP8Mum8xaGSkypRgDHKRbN8otJSzwgiJ9M",
       "publicKey":"02B2F836C47A36DE57C2AF2116B8E812B7C70E7F0FEB0906493B8476FC58692EBE"
     }

------------------------------------------------------------------------------

validationCreate
=====================

.. code-block:: java

  public JSONObject validationCreate();
  public JSONArray  validationCreate(int count);

生成验证key

------------
参数
------------

1. ``count`` - ``int``: 生成的key的个数

-------
返回值
-------

``JSONObject`` -  一个有效的key,结构为{"seed":xxx,"publickey":xxx}
``JSONArray``  -  一个或多个有效的key，每个key的结构为{"seed":xxx,"publickey":xxx}

-------
示例
-------

.. code-block:: java

    JSONObject json = c.validationCreate();
  
输出:

.. code-block:: json

    {
      "seed"     :"xnaKLBqkwZxCxCNk1LokjAekUQaWT",
      "publickey":"n9KrLAkaHZk3kns6TfZS9mRJmPrNJLjARxM8qUtM2CXpBpUcyTdD"
    }

.. code-block:: java

    JSONArray jsonArr = c.validationCreate(2);

输出:

.. code-block:: json

    [
     {"seed":"xxuvaugPX5ZTCcFvKdd9vzhAHFd27","publickey":"n94U13Uap8LQaDJQtbV9HGcgWH8qzWPscpZdqMv6SPz6U5Zazcdq"},
     {"seed":"xxqE8bBLKrKMMEpjqS4gwLwmRAGm6","publickey":"n9MdENDVAaQSDnmFdv3BzRbuuNH1AvUmpy8D7LMfN3evEx82us4Z"}
    ]

------------------------------------------------------------------------------

getServerInfo
=====================

.. code-block:: java

  public JSONObject getServerInfo();

获取区块链信息.

-------
返回值
-------

``JSONObject`` - 区块链信息.

1. ``JsonObject`` : 包含区块链基础信息，详细字段可在 **命令行接口 server_info** 中查看， 主要字段介绍如下：

	* ``buildVersion`` - ``String`` : 节点程序版本
	* ``complete_ledgers`` - ``String`` : 当前区块范围
	* ``peers`` - ``Number`` : peer节点数量
	* ``validation_quorum`` - ``Number`` : 完成共识最少验证节点个数

-------
示例
-------

.. code-block:: java

    JSONObject json = c.getServerInfo();

输出:

.. code-block:: json

    {
        "info":{
          "build_version":"0.30.3+DEBUG",
          "peers":0,
          "hostid":"DESKTOP-M5MSU6I",
          "last_close":{
              "proposers":0,
              "converge_time_s":1.985
          },



          "validation_quorum":1,



          "complete_ledgers":"1-7888",
          "pubkey_validator":"n9KigtPo6tPTNSuyaz7AtHk7XijPZwEUuF8LfaQQhjmSwFBenk6Q",
          "server_state":"proposing",
          "validator_list_expires":"unknown"
        }
    }

------------------------------------------------------------------------------

getChainInfo
=====================

.. code-block:: java

    public JSONObject getChainInfo();

获取链信息

-------
返回值
-------

1. ``JsonObject`` : 包含区块链基础信息，详细字段可在 **命令行接口 server_info** 中查看， 主要字段介绍如下：

	* ``chain_time`` - ``int`` : 区块链运行时间
	* ``tx_count`` - ``JSONObject`` : 见 **getTransactionCount 返回值**

-------
示例
-------

.. code-block:: java

    System.out.println(c.getChainInfo());

输出:

.. code-block:: json

    {
        "chain_time":517500,
        "tx_count":{
              "all":562,
              "chainsql":502
        }
    }

------------------------------------------------------------------------------

getUnlList
=====================

.. code-block:: java

    public JSONObject getUnlList();

获取信任公钥列表

-------
返回值
-------

``JSONObject`` - 信任公钥列表

-------
示例
-------

.. code-block:: java

    JSONObject json = c.getUnlList();

输出:

.. code-block:: json

    {
        "unl":[
          {
            "trusted":true,
            "pubkey_validator":"n9KigtPo6tPTNSuyaz7AtHk7XijPZwEUuF8LfaQQhjmSwFBenk6Q"
          }
        ]
    }

------------------------------------------------------------------------------

getAccountInfo
=====================

.. code-block:: java

    public JSONObject getAccountInfo(address);

从链上请求查询账户信息。

------------
参数
------------

1. ``address`` - ``String``: 账户地址

-------
返回值
-------

1. ``JsonObject`` : 包含账户基本信息。正常返回主要字段如下：

	* ``sequence`` - ``Number`` : 该账户交易次数；
	* ``zxcBalance`` - ``String`` : 账户ZXC系统币的余额。


-------
示例
-------

.. code-block:: java

    System.out.println(c.getAccountInfo(testAccountAddress));

输出:

.. code-block:: json    


      {
          "ledger_current_index":6363,
          "validated":false,
          "account_data":{
                "Account":"z3VGAJo59RWZ23CeM7zwMGVvsbWF8HabHf",
                "OwnerCount":1,
                "PreviousTxnLgrSeq":5113,
                "LedgerEntryType":"AccountRoot",
                "index":"6AFC4F3E9B190B8B6711BCC33EF03BF2D3FA1D374368BB3F8E80E5B744E8AACD",
                "PreviousTxnID":"8441BD3D6777A647B0D63DEC5D37C8AF67DE38248D7045E3900E14B3AE56DC8B",
                "Flags":0,
                "Sequence":3,
                "Balance":"399848588"
          }
      }

------------------------------------------------------------------------------

getTransactionCount
=====================

.. code-block:: java

    private JSONObject getTransactionCount();

获取交易数量，getServerInfo中调用，存于返回的tx_count字段中

-------
返回值
-------

1. ``JsonObject`` : 获取交易数量

	* ``all``      - ``int`` : 所有交易数量
	* ``chainsql`` - ``int`` : chainsql交易数量

------------------------------------------------------------------------------

getLedger
=====================

.. code-block:: java

    // 同步接口
    public JSONObject getLedger();                    // 获取最新账本信息
    public JSONObject getLedger(Integer ledger_index);// 获取指定索引账本信息
    public JSONObject getLedger(JSONObject option) ;

    // 异步接口
    public void getLedger(Callback<JSONObject> cb);
    public void getLedger(Integer ledger_index,Callback<JSONObject> cb);


获取账本信息

------------
参数
------------

1. ``ledger_index`` - ``Integer``: 账本索引

2. ``option`` - ``JSONObject``:

  * ``ledger_index`` - ``Integer`` : 账本索引；

3. ``cb``      - ``Callback`` : 异步接口，参数为一回调函数

-------
返回值
-------

.. _区块信息字段说明: https://developers.ripple.com/rippleapi-reference.html#getledger

1. ``JsonObject`` : 区块信息，可参考 `区块信息字段说明`_

-------
示例
-------

.. code-block:: java

    JSONObject json = c.getLedger();

输出:

.. code-block:: json

  {
    "ledger":{
      "close_flags":0,
      "ledger_index":"1654",
      "seqNum":"1654",
      "account_hash":"B5FAE3E051FDBC120E57A89EEEC54A59FA5974F7FC433236909A24BE09D31B8F",
      "close_time_resolution":10,
      "accepted":true,
      "close_time":609909971,
      "transactions":[
      ],
      "close_time_human":"2019-Apr-30 03:26:11",
      "ledger_hash":"3B2931B3F2D5FD589322B5EBA704B2249BCCB4E04E6BB660C5405660DAAE7D5B",
      "total_coins":"99999999999848599",
      "closed":true,
      "totalCoins":"99999999999848599",
      "parent_close_time":609909970,
      "hash":"3B2931B3F2D5FD589322B5EBA704B2249BCCB4E04E6BB660C5405660DAAE7D5B",
      "parent_hash":"939FE60220A6016CA0847790B4319C0FFA1AA06A58A3BA30DA37A664FEBD24E5",
      "transaction_hash":"0000000000000000000000000000000000000000000000000000000000000000"
    },
    "validated":true,
    "ledger_index":1654,
    "ledger_hash":"3B2931B3F2D5FD589322B5EBA704B2249BCCB4E04E6BB660C5405660DAAE7D5B"
  }

------------------------------------------------------------------------------

getLedgerVersion
=====================

.. code-block:: java

    public void       getLedgerVersion(Callback<JSONObject> cb);
    public JSONObject getLedgerVersion();
    

获取最新区块高度（区块号）

-------
返回值
-------

``JSONObject`` - 最新区块高度

-------
示例
-------

.. code-block:: java

    System.out.println(c.getLedgerVersion);
    c.getLedgerVersion( new Callback<JSONObject>() {
			@Override
			public void called(JSONObject args) {
				System.out.println(args);
			}
		});

输出:

.. code-block:: json    

  {
    "ledger_current_index":13796
  }

------------------------------------------------------------------------------

getAccountTransactions
==================================

.. code-block:: java

  public void       getAccountTransactions(String address,Callback cb);
  public void       getAccountTransactions(String address,int limit,Callback<JSONObject> cb);
  public JSONObject getAccountTransactions(String address,int limit);
  public JSONObject getAccountTransactions(String address);     

查询某账户的交易

------------
参数
------------

1. ``address`` - ``String``: 查询交易的账户地址;
2. ``limit``   - ``int``: 获取的最大的交易数量;
3. ``cb``      - ``Callback`` : 异步接口，参数为一回调函数

-------
返回值
-------

1. ``JsonObject`` : 包含账户基本信息。正常返回主要字段如下：

    * ``ledger_index_max`` - ``int`` : 最大区块号 
    * ``limit``            - ``int`` : 获取的最大的交易数量，默认为20
    * ``ledger_index_min`` - ``int`` : 最小区块号 
    * ``transactions``     - ``JSONArray`` : 交易数组，详见示例 
    * ``account``          - ``String`` : 查询交易的账户地址


-------
示例
-------

.. code-block:: java

		String rootAddress   = "zHb9CJAWyB4zj91VRWn96DkukG4bwdtyTh";
		JSONObject  obj      =   c.getAccountTransactions(rootAddress,30);
		System.out.println(obj);

		// Example 2
		c.getAccountTransactions(rootAddress, 30, new Callback<JSONObject>() {

			public void called(JSONObject args) {
				System.out.println(args);
			}
		});


输出

.. code-block:: json

    {
        "ledger_index_max":911,
        "limit":30,
        "ledger_index_min":1,
        "transactions":[
                  {
                  "tx":{
                      "date":609906292,
                      "Account":"zHb9CJAWyB4zj91VRWn96DkukG4bwdtyTh",
                      "TransactionType":"TableListSet",
                      "ledger_index":431,
                      "SigningPubKey":"0330E7FC9D56BB25D6893BA3F317AE5BCF33B3291BD63DB32654A313222F7FD020",
                      "Fee":"151401",
                      "Raw":"[{\"NN\":1,\"field\":\"id\",\"length\":11,\"PK\":1,\"type\":\"int\",\"UQ\":1}, {\"default\":null,\"field\":\"name\",\"length\":50,\"type\":\"varchar\"}, {\"field\":\"age\",\"type\":\"int\"}]",
                      "Flags":2147483648,
                      "Sequence":1,
                      "LastLedgerSequence":434,
                      "TxnSignature":"304402207E31292196C8004021A7A8D021E1EC39E2E997149DE886AF8AC3DFBFF17EAADA02200467DA6734FA000A3915806C4DC951F7307D3DFEEC0A6D75E715D1E5E51C54DC",
                      "Tables":[
                      {
                      "Table":{
                      "TableName":"c1235",
                      "NameInDB":"79D9C64B0297611ED6A642B1B5980C9C05E8ECBD"
                      }
                      }
                      ],
                      "inLedger":431,
                      "OpType":1,
                      "hash":"7A836046F485A7F94A205476AFC4D4BB12EFE9E2C0EFA31402406F774DC86094"
                      },
                      "validated":true                      
                  }                       
        ],
        "account":"zHb9CJAWyB4zj91VRWn96DkukG4bwdtyTh"
    }


------------------------------------------------------------------------------

getTransaction
=====================

.. code-block:: java

   public JSONObject getTransaction (String hash);     //同步
   public void getTransaction(String hash,Callback cb);//异步

查询某个hash下的交易信息

------------
参数
------------

1. ``hash``    - ``String``:  交易哈希值;
2. ``cb``   - ``Callback`` : 异步接口，参数为一回调函数

-------
返回值
-------

``JSONObject`` - 成功为交易信息，失败为null

------------------------------------------------------------------------------

sign
=====================

.. code-block:: java

    public JSONObject sign(JSONObject tx,String secret);  //对交易签名
    public byte[]     sign(byte[] message,String secret); //对普通字符串进行签名

签名接口。

------------
参数
------------

1. ``tx``      - ``JSONObject``:  交易对象，不同交易类型，结构不同
2. ``message`` - ``byte[]``    : 要签名的字符串
3. ``secret``  - ``String``    :  签名私钥

-------
返回值
-------

1. ``JsonObject`` : 

	* ``tx_blob``  -  签名后的交易
	* ``hash``     -  哈希值

2.  ``byte[]`` - 签名后的字符串

-------
示例
-------

.. code-block:: java

  JSONObject obj = new JSONObject();
  JSONObject tx_json = new JSONObject();
  tx_json.put("Account", "rHb9CJAWyB4rj91VRWn96DkukG4bwdtyTh");
  tx_json.put("Amount", "10000000000");
  tx_json.put("Destination", "rBuLBiHmssAMHWQMnEN7nXQXaVj7vhAv6Q");
  tx_json.put("TransactionType", "Payment");
  tx_json.put("Sequence", 2);
  obj.put("tx_json", tx_json);

  JSONObject res = c.sign(obj, "snoPBrXtMeMyMHUVTgbuqAfg1SUTb");
  System.out.println(res);


输出

.. code-block:: json

  {
      "tx_blob":"120000228000000024000000026140000002540BE40068400000000000000B73210330E7FC9D56BB25D6893BA3F317AE5BCF33B3291BD63DB32654A313222F7FD0207446304402203C7734114C1F9EEB16C775D49B8392636361CC844839483979BCBF0EE39F4B390220233BE3C714E1374415E2B4F2EF155579DB2E97BA627A3B16A0342A78975E06BF8114B5F762798A53D543A014CAF8B297CFF8F2F937E883140EDBA2E944E1751BDA1FC4ACC7116C9B19394996",
      "hash":"706BF05DD43730C312A3B3F2F9BB7B16B04DEC827E29A38AD2C5C390ED0E75D2"
  }

.. code-block:: java

    String hello = "helloworld";
		byte[] signature = c.sign(hello.getBytes(), rootSecret);
		System.out.println(signature);

输出

.. code-block:: java

  "[B@56cbfb61"


------------------------------------------------------------------------------

signFor
=====================

.. code-block:: java

    public JSONObject signFor(JSONObject tx,String secret);

用于交易多方签名，多方签名的详细介绍请参考 `Ripple开发者官网 <https://developers.ripple.com/multi-signing.html>`_ 

------------
参数
------------

1. ``tx``     - ``JSONObject``: 交易JSON
2. ``secret`` - ``String``    : 签名私钥

-------
返回值
-------

1. ``JSONObject`` :主要参数说明(格式见示例输出)

      * ``Account``        -  签名账户
      * ``TxnSignature``   -  签名后的交易
      * ``SigningPubKey``  -  签名公钥    
      * ``hash``           -  哈希值

-------
示例
-------

.. code-block:: java

      JSONObject obj = new JSONObject();
      obj.put("secret", "xnoPBzXtMeMyMHUVTgbuqAfg1SUTb");
      obj.put("account","zHb9CJAWyB4zj91VRWn96DkukG4bwdtyTh");
      JSONObject tx_json = new JSONObject();
      tx_json.put("Account", "zKvHeBUtEoNRW1wtvA42tfJx1bh7pqxZmT");
      tx_json.put("Amount", "1000000000");
      tx_json.put("Destination", "zMcXHEkD78T1pwAgG2pf6QWALyBKF1YvD1");
      tx_json.put("TransactionType", "Payment");
      tx_json.put("Sequence", 2);
      obj.put("tx_json", tx_json);

      JSONObject res = c.signFor(obj, "xnoPBzXtMeMyMHUVTgbuqAfg1SUTb");
      System.out.println("sign_for payment signer:");
      System.out.println(res);

输出

.. code-block:: json

    {
      "Signer":{
        "Account":"zHb9CJAWyB4zj91VRWn96DkukG4bwdtyTh",
        "TxnSignature":"3045022100CF059AC2B07D3CAC1F3E877F0E81675731BDA21F6BF51000FC3DE3EE579A343002207320DB8614F45925D1AA392E0E61D468D1F3BA269574ED24F3789A887CC89F12",
        "SigningPubKey":"0330E7FC9D56BB25D6893BA3F317AE5BCF33B3291BD63DB32654A313222F7FD020"
      },
      "hash":"EB11BE3944E183EBD89A5CDB1EE55664DD2289D05E84BF99F47147108A5A728B"
    }


------------------------------------------------------------------------------

getTableNameInDB
=====================

.. code-block:: java

    public JSONObject getTableNameInDB(String owner,String tableName);

查询表在数据库中的记录的名字，在外定义的表名，经过ChainSQL会进行格式转换，此接口查询转换之后的名字。

------------
参数
------------

1. ``owner`` - ``String``: 账户地址,表的拥有者；
2. ``tableName`` - ``String``:  原始表名

-------
返回值
-------

 ``JSONObject`` : 

      * ``status``         -  正确 success;错误 error
      * ``nameInDB``       -  数据库中的表名
      * ``error_message``  -  错误返回时，显示错误信息
      
-------
示例
-------

.. code-block:: java

  System.out.println(c.getTableNameInDB(rootSecret,"test1"));

输出:

.. code-block:: json

  	{
	    "status":"success"
	    "nameInDB":"xxx"
	  }

------------------------------------------------------------------------------

getTableAuth
=====================

.. code-block:: java

    public JSONObject getTableAuth(String owner,String tableName);
    public JSONObject getTableAuth(String owner,String tableName,List<String> accounts);
    public void getTableAuth(String owner,String tableName,Callback<JSONObject> cb);
    public  void getTableAuth(String owner,String tableName,List<String> accounts,Callback<JSONObject> cb)


获取某张表的授权列表

------------
参数
------------

1. ``owner`` - ``String``: 表的拥有者地址
2. ``tableName`` - ``String``: 表名
3. ``accounts`` - ``List<String>``: 指定账户地址列表，只查这个列表中相关地址的授权
4. ``cb`` - ``Callback<JSONObject>``:回调函数

-------
返回值
-------

``JSONObject`` - 授权列表

-------
示例
-------

.. code-block:: java

  System.out.println(c.getTableAuth(testAccountAddress,sTableName2));

.. code-block:: Json

    {
          "owner":"z3VGAJo59RWZ23CeM7zwMGVvsbWF8HabHf",
          "tablename":"tTable2",
          "users":[
            {
              "authority":{
              "select":true,
              "insert":true,
              "update":true,
              "delete":true
              },
              "account":"z3VGAJo59RWZ23CeM7zwMGVvsbWF8HabHf"
            }
          ]
    }

------------------------------------------------------------------------------

getAccountTables
=====================

.. code-block:: java

    public JSONObject getAccountTables(String address,boolean bGetDetail);
    public void getAccountTables(String address,boolean bGetDetail,Callback<JSONObject> cb);

获取某个账户建的表

------------
参数
------------

1. ``address`` - ``String``: 账户地址
2. ``bGetDetail`` - ``boolean``: 是否获取详细信息（建表的raw字段）
3. ``cb`` - ``Callback<JSONObject>``:回调函数

-------
返回值
-------

``JSONObject`` - 用户建的表（数组）

-------
示例
-------

.. code-block:: java

  System.out.println(c.getAccountTables(rootAddress,true));

输出:

.. code-block:: json

    {
      "tables":[
        {
        "create_time":608208080,
        "ledger_index":6222,
        "ledger_hash":"8443D58F41A6B391B2833E7A58F8E4587A361441EC1B66E182F5CAD0F02EFE7F",
        "raw":"AB8B11AE95B9DD145AF82052FE87FADBF23DA6336095848D1C2918699AEC772E853E4CD9107E2AAB0C304DB62C03A85373249680DC6D56B55852C572A279EFBE307417EA81868DCAA422A5E6C22404803CE37FF48EC1FC95F40522C7763E1DC8EDA9D9BA10E4D4E51910FBF7852ACF7455F5FFFC07AEBFB3F7BBBFEF8096B559872BB64D3E0E91B6332018088CC76D83A5E85A189717B9262067F66F2A89FDE6",
        "tx_hash":"EFE119F4EA1481400D050C54981D20E5AE40C293A1C9FD0D80C3F73AF3FF1673",
        "tablename":"b1",
        "nameInDB":"D06DC2D2A7A8CB40B637B95BF2A9BF8EF62C89CA",
        "table_exist_inDB":false,
        "confidential":true
        }
      ]
    }

------------------------------------------------------------------------------

网关交易
*****************

accountSet 
=====================

.. code-block:: java

  public Ripple accountSet(int nFlag, boolean bSet);
  public Ripple accountSet(String transferRate, String transferFeeMin, String transferFeeMax);

账户属性设置

------------
参数
------------

1. ``flag`` - ``int``:                 一般情况下为8，表示asfDefaultRipple，详见 `AccountSet Flags <https://developers.ripple.com/accountset.html>`_
2. ``bSet`` - ``boolean``:             true:SetFlag; false:ClearFlag
3. ``transferRate`` - ``String``:      通过网关进行转账，网关要收取的手续费费率，1.0 - 2.0 string,，例如 "1.005","1.008"，设置为"1.0"表示取消
4. ``transferFeeMin`` - ``String``:    通过网关进行转账，网关要收取的最小手续费，10进制字符串数字，例如"10"，设置为"0"表示取消
5. ``transferFeeMax`` - ``String``:    通过网关进行转账，网关要收取的最大手续费，10进制字符串数字，例如"10"，设置为"0"表示取消

-------
返回值
-------

``Ripple`` - Ripple对象,后面一般接submit进行连续操作,如示例。

-------
示例
-------

.. code-block:: java

    //开启Rippling功能
    JSONObject jsonObj = c.accountSet(8, true).submit(SyncCond.validate_success);
    System.out.print("set gateWay:" + jsonObj + "\ntrust gateWay ...\n");

    //设置网关手续费
    jsonObj = c.accountSet("1.005", "2", "3").submit(SyncCond.validate_success);
    System.out.print("set gateWay:" + jsonObj + "\ntrust gateWay ...\n");

------------------------------------------------------------------------------

trustSet
=====================

.. code-block:: java

  public Ripple trustSet(String value, String sCurrency, String sIssuer)

信任网关，参数指定信任某个网关的某货币数量。从而可以交易该货币。为交易类型，需要调用submit提交交易。

------------
参数
------------

1. ``value`` - ``String``:        转账数额
2. ``sCurrency`` - ``String``:    货币名称 ，例如"RMB"
3. ``sIssuer`` - ``String``:      该货币的发行网关地址。


-------
返回值
-------

``Ripple`` - Ripple对象,后面一般接submit进行连续操作,如示例。

-------
示例
-------

.. code-block:: java

    JSONObject jsonObj = c.trustSet("1000000000", "RMB", "zHb9CJAWyB4zj91VRWn96DkukG4bwdtyTh").submit(SyncCond.validate_success);

------------------------------------------------------------------------------

.. _pay-代币:

pay(转账网关代币)
=====================

.. code-block:: java

   public  Ripple pay(String accountId, String value, String sCurrency, String sIssuer);

转发代币

.. note::
    这个函数有重载，除了可以转账系统币外，还可以转账发行的代币：:ref:`pay(转账系统币) <pay-系统币>`

------------
参数
------------

1. ``accountId``   - ``String``:  转账接受地址
2. ``value``       - ``String``:  转账货币的数量 最大值为:1e11.
3. ``sCurrency``   - ``String``:  货币名称 ，例如"RMB"
4. ``sIssuer``     - ``String``:  网关地址

-------
返回值
-------

``Ripple`` - Ripple对象,后面一般接submit进行连续操作,如示例。

-------
备注
-------

网关要收取的手续费的计算公式如下

``transferRate``   - 费率，    为 ``accountSet`` 函数中的参数

``transferFeeMin`` - 最小花费， 为 ``accountSet`` 函数中的参数

``transferFeeMax`` - 最大花费， 为 ``accountSet`` 函数中的参数

.. math::
    \begin{gather}
    fee   = 转账金额*费率 \\
    交易花费=\begin{cases}
    最小花费, \quad fee<最小花费 \\
    fee,\quad 最小花费\leq fee\leq 最大花费 \\ 
    最大花费，\quad fee>最大花费
    \end{cases}
    \end{gather}

-------
示例
-------

.. code-block:: java

  // 给账户地址等于 z9VF7yQPLcKgUoHwMbzmQBjvPsyMy19ubs 的用户转账5RMB.
  c.pay("z9VF7yQPLcKgUoHwMbzmQBjvPsyMy19ubs", "5", "RMB", "zHb9CJAWyB4zj91VRWn96DkukG4bwdtyTh").submit(SyncCond.validate_success);

------------------------------------------------------------------------------

表交易
*****************

createTable
=====================

.. code-block:: java

  public Chainsql createTable(String name, List raw);
  public Chainsql createTable(String name, List rawList ,boolean confidential);
  public Chainsql createTable(String name, List raw,JSONObject operationRule);


建表。

------------
参数
------------

1. ``tableName``   - ``String``: 所创建表名，创建表不支持自增型;
2. ``raw`` - ``List``: 创建表的字段名称必须为Json格式数据,详细格式及内容可参看  :ref:`建表raw字段说明 <create-table>`;例如：

.. code-block:: javascript

  {'field':'id','type':'int','length':11,'PK':1,'NN':1,'UQ':1}
  {'field':'name','type':'varchar','length':50,'default':null}
  {'field':'age','type':'int'}


3. ``confidential``  - ``boolean``:    表示创建的表是否为加密的表,true:创建加密表;如果不写,默认为false;
4. ``operationRule`` - ``JSONObject``: 行级控制规则，不能与confidential一起使用

.. _my-reference-chainsql:

-------
返回值
-------

1. ``Chainsql`` - Chainsql对象，后面一般接submit函数进行连续操作,如示例。

-------
示例
-------

.. code-block:: java

  // 创建表 "dc_universe"
  JSONObject ret = c.createTable("dc_universe", c.array(
  "{'field':'id','type':'int','length':11,'PK':1,'NN':1,'UQ':1}",
  "{'field':'name','type':'varchar','length':50,'default':null}",
  "{'field':'age','type':'int'}"),
  false)
  .submit(SyncCond.db_success);

------------------------------------------------------------------------------

renameTable
=====================

.. code-block:: java

  public Chainsql renameTable(String oldName, String newName);

修改数据库中表的名字

------------
参数
------------

1. ``tableName``    - ``String``:  旧的表名
2. ``NewTableName`` - ``String``: 新的表名;两个名字都不能为空；

-------
返回值
-------

``Chainsql`` -  :ref:`Chainsql <my-reference-chainsql>`.

-------
示例
-------

.. code-block:: java

  // 将表"dc_universe"改为"dc_name"
  c.renameTable("dc_universe", "dc_name").submit();

------------------------------------------------------------------------------

dropTable
=====================

.. code-block:: java

  public Chainsql dropTable(String tableName);

从数据库删除一个表。表和它的所有数据将被删除;

------------
参数
------------

1. ``tableName``    - ``String``:  表名

-------
返回值
-------

``Chainsql`` -  :ref:`Chainsql <my-reference-chainsql>`.

-------
示例
-------

.. code-block:: java

  c.dropTable("dc_universe").submit();

------------------------------------------------------------------------------

table
=====================

.. code-block:: java

  public Table table(String name);

创建一个table对象

------------
参数
------------

1. ``tableName``    - ``String``:  表名

-------
返回值
-------

.. _my-reference-table:

``Table`` - Table对象，一般后面接insert，get，submit等函数进行连续操作，如示例。

-------
示例
-------

.. code-block:: java

  c.table("posts").insert(c.array("{id: 1, 'name': 'peera','age': 22}", "{id: 2, 'name': 'peerb','age': 21}"))
  .submit(SyncCond.db_success);

  c.table("posts")
  .get(c.array("{'id': 1}"))
  .update("{'age':52,'name':'lisi'}")
  .submit(SyncCond.db_success);

------------------------------------------------------------------------------

insert
=====================

.. code-block:: java

  public Table insert(List<String> raw);

向数据库中插入数据。

------------
参数
------------

1. ``raw``    - ``List``:  raw类型必须都是示例中的json格式的数据类型，详细格式和内容可参看 :ref:`插入raw字段说明 <insert-table>` ；;

-------
返回值
-------

``Table`` - 见 :ref:`Table  <my-reference-table>`.

-------
示例
-------

.. code-block:: java

  // 向表"posts"中插入一条记录.
  c.table("posts").insert(c.array("{id: 1, 'name': 'peera','age': 22}", "{id: 2, 'name': 'peerb','age': 21}"))
  .submit(SyncCond.db_success);
  

------------------------------------------------------------------------------

update
=====================

.. code-block:: java

  public Table update(String raw);

更新表中数据。如果get添加为空，则更新表中所有记录；其中raw为json格式字符串;

------------
参数
------------

1. ``raw``    - ``List``:  raw类型必须都是示例中的json格式的数据类型，详细格式和内容可参看 :ref:`更新raw字段说明 <update-table>`;

-------
返回值
-------

``Table`` - 见 :ref:`Table  <my-reference-table>`.

-------
示例
-------

.. code-block:: java

  // 更新 id 等于 1 的记录
  c.table("posts")
  .get(c.array("{'id': 1}"))
  .update("{'age':52,'name':'lisi'}")
  .submit(SyncCond.db_success);

------------------------------------------------------------------------------

delete
=====================

.. code-block:: java

  public Table delete();

从表中删除对应条件的数据，需要与get配置使用。如果get条件为空，则删除所有数据。

------------
参数
------------

-------
返回值
-------

``Table`` - 见 :ref:`Table  <my-reference-table>`.

-------
示例
-------

.. code-block:: java

  // 删除 id 等于 1 的记录.
  c.table("comments")
  .get(c.array("{'id': 1}"))
  .delete()
  .submit(SyncCond.db_success);

------------------------------------------------------------------------------

beginTran
=====================

.. code-block:: java

  public void beginTran();

开启事务.

commit
=====================

.. code-block:: java

  public JSONObject commit();
  public JSONObject commit(SyncCond cond);
  public JSONObject commit(Callback<?> cb);

提交事务;本次事务期间的所有操作都会打包提交到区块链网络。
commit有3个重载函数，对应异步和同步，客户可以根据需要填写参数。返回值均为JSON对象，指示成功或失败;

------------
参数
------------

1. ``cond`` - ``SyncCond``: 同步接口，参数为 枚举类型;
2. ``cb``   - ``Callback``: 异步接口，参数为 回调函数

-------
返回值
-------

-------
示例
-------

::

  c.beginTran();

  c.table("posts").insert({name: 'peera',age: 22}, {name: 'peerb',age: 21});
  c.table("posts").get({id: 1}).assert({age:22,name:'peera'});
  c.table("posts").get({id: 1}).update({age:52,name:'lisi'});
  c.table("comments").delete({id: 1});

  // 1、
  c.commit();

  // 2、
  //c.commit(new Callback () {
  //  public void called(JSONObject data) {
  //    System.out.println(data);
  // }));

  // 3、
  // c.commit(SyncCond.db_success);

------------------------------------------------------------------------------

.. note:: 在事务开始和结束之间的insert，update，delete，assert语句会包装在一个原子操作中执行，与数据库的事务类似，事务中执行的语句要么全部成功，要么全部失败。执行事务类型交易主要涉及两个api：beginTran,commit.beginTran开启事务，commit提交事务，事务中的操作全部执行成功事务才成功，有一个执行失败，则事务会自动回滚。在事务上下文中，不再支持单个语句的submit。

grant
=====================

.. code-block:: java

  public Chainsql grant(String name, String user,String flag);

授权user用户操作表name的各项权限

------------
参数
------------

1. ``name``    - ``String``:  表名
2. ``user``    - ``String``:  被授权账户地址 ,以字母 'z' 开头.
3. ``flag``    - ``String``:  表操作规则.例如:"{insert:true,delete:false}" 表示user 账户可以执行插入操作，但是不能执行删除操作

-------
返回值
-------

``Chainsql`` -  :ref:`Chainsql <my-reference-chainsql>`.

-------
示例
-------

.. code-block:: java

  JSONObject obj = new JSONObject();
  obj = c.grant(sTableName, sNewAccountId, "{insert:true,update:true}")
          .submit(SyncCond.validate_success);
  System.out.println("grant result:" + obj.toString());

------------------------------------------------------------------------------

setRestrict
=====================

.. code-block:: java

  public void setRestrict(boolean falg);

设置是否使用严格模式，默认为非严格模式；在严格模式下，语句共识通过的条件是期望的快照HASH与预期一致

------------
参数
------------

1. ``flag`` - ``boolean``: true:严格模式     false: 非严格模式

-------
返回值
-------

-------
示例
-------

.. code-block:: java

    c.setRestrict(false);

------------------------------------------------------------------------------

setNeedVerify
=====================

.. code-block:: java

  public void setNeedVerify(boolean flag);

设置是否需要认证

------------
参数
------------

1. ``flag`` - ``boolean``: true: 需要认证  false: 不需要认证

-------
返回值
-------

-------
示例
-------

.. code-block:: java

    c.setNeedVerify(false);

------------------------------------------------------------------------------

表查询
*****************

.. note::
	现在查询无论数据库有多少内容，get接口一次最多返回200条结果，如果数据较多，可以结合 ``limit`` 接口做分页查询。

get
=====================

.. code-block:: java

   public  Table get(List<String> args);

从数据库查询数据,后面可以进行其他操作，例如update、delete等;
通过指定查询的内容作为raw参数传入，raw的详细格式及内容可参看 :ref:`Raw字段详解 <查询Raw详解>`

------------
参数
------------

1. ``args``    - ``List<String>``:  List 元素的格式是JSON类型，例如"{'name': 'aa'}";

-------
返回值
-------

``Table`` - 见 :ref:`Table  <my-reference-table>`.

-------
示例
-------

.. code-block:: java

  //查询 name 等于 aa 的记录.
  c.table("posts")
  .get(c.array("{'name': 'aa'}"))
  .submit();

  //或

  c.table("posts")
  .get(c.array("{'name': 'aa'}"))
  .withFields([])
  .submit();

------------------------------------------------------------------------------

limit
=====================

.. code-block:: java

   public Table limit(String raw);

对数据库进行分页查询.返回对应条件的数据；必须与get配合使用;

------------
参数
------------

1. ``raw``    - ``List``:  raw类型必须都是示例中的json格式的数据类型;

-------
返回值
-------

``Table`` - 见 :ref:`Table  <my-reference-table>`.

-------
示例
-------

.. code-block:: java

  // 查询 name 等于 aa 的前10条记录
  c.table("posts")
  .get(c.array("{'name': 'aa'}"))
  .limit("{index:0,total:10}")
  .withFields([])
  .submit();

------------------------------------------------------------------------------

order
=====================

.. code-block:: java

   public Table order(String raw);

------------
参数
------------

1. ``raw``    - ``List<String>``:  raw类型必须都是示例中的json格式的数据类型;

-------
返回值
-------

``Table`` - 见 :ref:`Table  <my-reference-table>`.

-------
示例
-------

.. code-block:: java

  // 按 id 升序，name 的降序排序
  c.table("posts")
  .get(c.array("{'name': 'aa'}"))
  .order(c.array("{id:1}", "{name:-1}"))
  .withFields([])
  .submit();

------------------------------------------------------------------------------

withFields
=====================

.. code-block:: java

  public Table withFields(String  orgs);

从数据库查询数据,并返回指定字段,必须与get配合使用;

------------
参数
------------

1. ``orgs``    - ``String``:  orgs类型必须都是示例中的json格式的数据类型;

-------
返回值
-------

``Table`` - 见 :ref:`Table  <my-reference-table>`.

-------
示例
-------

.. code-block:: java

  // 查询 name 等于 aa 的记录.取name以及age字段
  c.table("posts")
  .get(c.array("{'name': 'aa'}"))
  .withFields("['name','age']")
  .submit();

------------------------------------------------------------------------------

getBySqlAdmin
=====================

.. code-block:: java

   // 根据sql语句查询，admin接口，同步调用
   public JSONObject getBySqlAdmin(String sql);

   // 根据sql语句查询，admin接口，异步调用
   public void       getBySqlAdmin(String sql,Callback<JSONObject> cb);

直接传入SQL语句进行数据库查询操作。

.. note::

  * 因为直接操作数据库中的表，所以需要配合getTableNameInDB接口获取表在数据库中的真实表名。详见示例。
	* 本接口不做表权限判断，但是只能由节点配置文件中 **[port_ws_admin_local]** 的admin里所配置的ip才可以发起此接口的调用，否则调用失败。
	* 即nodejs接口运行所在的主机ip必须是配置在配置文件的 **[port_ws_admin_local]** 的admin里。
	* 因为在配置文件中配置为admin的ip认为是该节点的管理员。拥有对本节点已经同步表的查询权限。


------------
参数
------------

1. ``sql``   - ``String``              :  标准sql语句
2. ``cb``    - ``Callback<JSONObject>``:  回调函数

-------
返回值
-------

1. ``JSONObject`` :主要参数说明(格式见示例输出)   

      * ``final_result``   -  true，查询成功;false,查询失败。
      * ``diff``           -  当前区块序号与被查询表数据库同步到的区块序号的差值，如果有多个表，取最大差值。
      * ``lines``          -  返回查询的sql结果。
      * ``error_message``  -  错误返回时，显示错误信息。

-------
示例
-------

.. code-block:: java

  // select * from t_xxxxxxx
  c.getTableNameInDB(rootAddress, sTableName, new Callback<JSONObject>(){

  @Override
  public void called(JSONObject args) {
      System.out.println(args);
      if(args.has("nameInDB")) {
        String sql = "select * from t_" + args.getString("nameInDB");
        c.getBySqlAdmin(sql,new Callback<JSONObject>() {

          @Override
          public void called(JSONObject args) {
            System.out.println( args);
          }
          
        });
        
      }
    }
    
  });

成功

.. code-block:: json

    {
      "final_result":true,
      "lines":[
          {
          "name":"hello",
          "id":1,
          "age":333
          },
          {
          "name":"sss",
          "id":2,
          "age":444
          },
          {
          "name":"rrr",
          "id":3,
          "age":555
          }
      ]
    }

失败

.. code-block:: json

    {
        "error_message":"Bad credentials.",
        "request":{
          "id":2,
          "command":"r_get_sql_admin",
          "sql":"select * from t_27C10F5E2128840470021D82A40933CEC689FEC5"
        },
        "error_code":3,
        "id":2,
        "error":"forbidden",
        "type":"response",
        "status":"error"
    }

------------------------------------------------------------------------------

getBySqlUser
=====================

.. code-block:: java

    public JSONObject getBySqlUser(String sql);//同步接口
    public void       getBySqlUser(String sql,Callback<JSONObject> cb);// 异步接口

直接传入SQL语句进行数据库查询操作。

.. note::
  * 因为直接操作数据库中的表，所以需要配合getTableNameInDB接口获取表在数据库中的真实表名。详见示例。
  * 调用此接口前，as接口中设置的用户需要对SQL语句中的表有查询权限，才可以调用此接口。


------------
参数
------------

1. ``sql``    - ``String``:  标准sql语句
2. ``cb``    - ``Callback<JSONObject>``:  回调函数

-------
返回值
-------

1. ``JSONObject`` :主要参数说明(格式见示例输出)   

      * ``final_result``   -  true，查询成功;false,查询失败。
      * ``diff``           -  当前区块序号与被查询表数据库同步到的区块序号的差值，如果有多个表，取最大差值。
      * ``lines``          -  返回查询的sql结果。
      * ``error_message``  -  错误返回时，显示错误信息。

-------
示例
-------

.. code-block:: java

  JSONObject ret = c.getTableNameInDB(rootAddress, sTableName);
  if(ret.has("nameInDB")) {
    JSONObject obj = c.getBySqlUser("select * from t_" + ret.getString("nameInDB"));
    System.out.println( obj );
  }

成功

.. code-block:: json

    {
      "final_result":true,
      "lines":[
          {
          "name":"hello",
          "id":1,
          "age":333
          },
          {
          "name":"sss",
          "id":2,
          "age":444
          },
          {
          "name":"rrr",
          "id":3,
          "age":555
          }
      ]
    }

失败

.. code-block:: json

  {
    "error_message": "The user is unauthorized to the table.",
    "request": {
      "signature": "304402200BFB2A48BCBDCA39E10334ABE926CAB3AAF668DC34C0EC6FC16AB6D30F3906520220025A911F16530C60842CEBF6EF70BDFA053287427A007B6E8D5751D49BE7B36F",
      "tx_json": {
        "LedgerIndex": 33020,
        "Account": "zpMZ2H58HFPB5QTycMGWSXUeF47eA8jyd4",
        "Sql": "select * from t_A07D12AF1616383806669B128675E8C32A894265"
      },
      "id": 3,
      "publicKey": "02080EF3E62711D21A502DC6B6290E2819474AA7701271D82CDFF73F65AFEE7276",
      "signingData": "{\"LedgerIndex\":33020,\"Account\":\"zpMZ2H58HFPB5QTycMGWSXUeF47eA8jyd4\",\"Sql\":\"select * from t_A07D12AF1616383806669B128675E8C32A894265\"}",
      "command": "r_get_sql_user"
    },
    "final_result": true,
    "error_code": 77,
    "id": 3,
    "error": "tabUnauthorized",
    "type": "response",
    "status": "error"
  }


------------------------------------------------------------------------------

订阅
*****************

subscribeTable
=====================

.. code-block:: java

  public void subscribeTable(String name, String owner ,Callback<JSONObject> cb);

订阅某张表。该表相关的信息发生改变时，会通过回调函数返回改变内容。

------------
参数
------------

1. ``name``    - ``String``:    表名;
2. ``owner``   - ``String``:    为表的所有者地址;
3. ``cb``      - ``Callback`` : 回调函数

-------
返回值
-------

-------
示例
-------

.. code-block:: java

  // 用户订阅TestName表信息，表的创建者为rootAddress
  c.event.subscribeTable("TestName", rootAddress,new Callback<JSONObject>() {
    @Override
    public void called(JSONObject args) {
      System.out.println(" table info :" + args);
    }
  });

------------------------------------------------------------------------------

unsubcribeTable
=====================

.. code-block:: java

  public void unsubscribeTable(String name, String owner,Callback<JSONObject> cb);

取消对表的订阅。

------------
参数
------------

1. ``name``  - ``String``:  被订阅的数据库表名；
2. ``owner``      - ``String``:  被订阅的表拥有者地址；
3. ``cb``         - ``Callback``: 异步接口回调

-------
返回值
-------

-------
示例
-------

.. code-block:: java

  // 用户取消订阅TestName表
  c.event.unsubscribeTable("TestName", rootAddress, new Callback<JSONObject>() {
      @Override
      public void called(JSONObject args) {

        System.out.println(" unsubscribeTable info :" + args);
      }
  });

------------------------------------------------------------------------------

subscribeTx
=====================

.. code-block:: java

   public void subscribeTx(String txid,Callback<?> cb);

订阅交易事件，提供交易的哈希值，就可以订阅此交易。

------------
参数
------------

1. ``txid`` - ``String``:          被订阅的交易哈希值；
2. ``cb``   - ``callback``:  回调函数，为必填项，需用通过此函数接收后续订阅结果。

-------
返回值
-------

-------
示例
-------

.. code-block:: java

  // 用户订阅交易Hash信息.
  c.event.subscribeTx("601781B50D7936370276287EAC3737D7A1D20281E2E73FCA31FE7563426C93B0", new Callback<JSONObject>() {

    @Override
    public void called(JSONObject args) {
      System.out.println("subscribeTx Info:" + args);
    }
  });

------------------------------------------------------------------------------

unsubscribeTx
=====================

.. code-block:: java

   public void unsubscribeTx(String txid,Callback<JSONObject> cb)

取消对交易的订阅

------------
参数
------------

1. ``txid`` - ``String``:          被订阅的交易哈希值

-------
返回值
-------

-------
示例
-------

.. code-block:: java

  // 取消订阅交易Hash信息.
  c.event.unsubscribeTx("601781B50D7936370276287EAC3737D7A1D20281E2E73FCA31FE7563426C93B0", new Callback<JSONObject>() {
    @Override
    public void called(JSONObject args) {
      System.out.println("unsubscribeTx Info:" + args);
    }
  });

------------------------------------------------------------------------------


智能合约接口
*****************

.. toctree::
   :maxdepth: 2

   javaSmartContract
