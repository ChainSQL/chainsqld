======================
Java接口
======================

基本操作
*****************

获取依赖的jar包

- 如果本地有maven环境，将以下代码加入本地开发环境pom.xml文件进行jar包下载。

::

  <dependency>
    <groupId>com.peersafe</groupId>
    <artifactId>chainsql</artifactId>
    <version>1.4.2</version>
  </dependency>


- 如果本地没有maven环境，直接下载项目依赖的jar包，在“buildPath”中选择“libraries”，再Add External Jars 添加相应的jar包。项目依赖的jar包下载: `下载地址 <http://www.chainsql.net/libs.zip>`_

------------------------------------------------------------------------------

引入
=====================

使用下面的代码引入ChainSQL JAVA API

.. code-block:: java

    import com.peersafe.chainsql.core.Chainsql;

    import com.peersafe.chainsql.core.Table;

    public static final Chainsql c= Chainsql.c;

------------------------------------------------------------------------------

基本函数
*****************

connect
=====================

.. code-block:: java

    c.connect(url);

Connect to a websocket url.

------------
参数
------------

1. ``url`` - ``String``: Websocket url to connect,e.g.:"ws://127.0.0.1:5006".

-------
返回值
-------

``Connection`` - Connection object after connected.

-------
例子
-------

.. code-block:: java

    // 如果无法建立连接,会抛出java.net.ConnectException;
    String url = "ws://192.168.0.162:6006";
    c.connect(url);

------------------------------------------------------------------------------

as
=====================

.. code-block:: java

  as(String address, String secret);

提供操作者的身份

------------
参数
------------

1. ``address`` - ``String``: Account address,start with a lower case 'z'.
2. ``secret``  - ``String``: Account secret,start with a lower case 'x'.

-------
返回值
-------

-------
例子
-------

.. code-block:: java

    //提供操作者的身份;
    c.as("zP8Mum8xaGSkypRgDHKRbN8otJSzwgiJ9M", "xcUd996waZzyaPEmeFVp4q5S3FZYB");

------------------------------------------------------------------------------

use
=====================

.. code-block:: java

  use(String address);

切换表的所有者（即后续操作的上下文），默认表的所有者为操作者

------------
参数
------------

1. ``address`` - ``String``: AAddress of table owner..

-------
返回值
-------

-------
例子
-------

.. code-block:: java

    c.use("zP8Mum8xaGSkypRgDHKRbN8otJSzwgiJ9M");

------------------------------------------------------------------------------

setRestrict
=====================

.. code-block:: java

  c.setRestrict(flag)

设置是否使用严格模式，默认为非严格模式；在严格模式下，语句共识通过的条件是期望的快照HASH与预期一致

------------
参数
------------

1. ``flag`` - ``boolean``: true:严格模式     false: 非严格模式

-------
返回值
-------

-------
例子
-------

.. code-block:: java

    c.setRestrict(false);

------------------------------------------------------------------------------

submmit
=====================

.. code-block:: java

  JSONObject submit();
  JSONObject submit(Callback cb)
  JSONObject submit(SyncCond cond);

submit有3个重载函数，对应异步和同步，客户可以根据需要填写参数。返回值均为JSON对象，指示成功或失败;

------------
参数
------------

1. ``cb``   - ``Callback``: 异步接口，参数为一回调函数
2. ``cond`` - ``SyncCond``: 同步接口，参数为一枚举类型;

.. code-block:: java

  enum SyncCond {
      validate_success,// 交易共识通过
      db_success       //交易成功同步到数据库
  };

-------
返回值
-------

``JSONObject`` - JSON对象.

-------
例子
-------

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

------------------------------------------------------------------------------

用户管理
*****************

generateAddress
=====================

.. code-block:: java

    c.generateAddress();

创建用户,返回JSON数据类型.

-------
返回值
-------

``JSONObject`` - JSON对象 secret:账户秘钥 address:账户地址 publicKey:公钥

-------
例子
-------

.. code-block:: java

    JSONObject json = c.generateAddress();

    // 输出:

    // {
    //   "secret":"xcUd996waZzyaPEmeFVp4q5S3FZYB",
    //   "address":"zP8Mum8xaGSkypRgDHKRbN8otJSzwgiJ9M",
    //   "publicKey":"02B2F836C47A36DE57C2AF2116B8E812B7C70E7F0FEB0906493B8476FC58692EBE"
    // }

------------------------------------------------------------------------------

pay
=====================

.. code-block:: java

    c.pay(accountId,count);

给用户转账,新创建的用户在转账成功之后才能正常使用。

------------
参数
------------

1. ``accountId``   - ``String``: 转账的用户地址
2. ``count`` - ``String``: 转账金额,最小值5;

-------
返回值
-------

``Ripple`` - Ripple对象

-------
例子
-------

.. code-block:: java

  // 给账户地址等于 z9VF7yQPLcKgUoHwMbzmQBjvPsyMy19ubs 的用户转账5ZXC.
  c.pay("z9VF7yQPLcKgUoHwMbzmQBjvPsyMy19ubs", "5")

------------------------------------------------------------------------------

表操作
*****************

createTable
=====================

.. code-block:: java

  Chainsql createTable(String name, List raw);
  Chainsql createTable(String name, List rawList ,boolean confidential);
  Chainsql createTable(String name, List raw,JSONObject operationRule);


建表。

------------
参数
------------

1. ``tableName``   - ``String``: 所创建表名，创建表不支持自增型;
2. ``raw`` - ``List``: 创建表的字段名称必须为Json格式数据;例如：

.. code-block:: javascript

  {'field':'id','type':'int','length':11,'PK':1,'NN':1,'UQ':1}
  {'field':'name','type':'varchar','length':50,'default':null}
  {'field':'age','type':'int'}

创建表参数说明:


==========   =========================================
field        表字段名
==========   =========================================
type         字段名类型，
             支持int/float/double/decimal/varchar/blob/text/datetime
length       字段值的字节长度
PK           值为1表示字段为主键
NN           值为1表示值不能为空,NOT NULL
UQ           值为1表示值唯一
index        值为1表示字段为索引
FK           值为1表示字段为某表的外键，必须配合REFERENCES使用
REFERENCES   值的格式为 {'table':'user','field':'id'}
==========   =========================================


3. ``confidential``  - ``boolean``: 表示创建的表是否为加密的表,true:创建加密表;如果不写,默认为false;
4. ``operationRule`` - ``JSONObject``: 行级控制规则，不能与confidential一起使用

-------
返回值
-------

``Chainsql`` - Chainsql对象

-------
例子
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

  c.renameTable(tableName, NewTableName).submit();

修改数据库中表的名字

------------
参数
------------

1. ``tableName``    - ``String``:  旧的表名
2. ``NewTableName`` - ``String``: 新的表名;两个名字都不能为空；

-------
返回值
-------

``Chainsql`` - Chainsql对象

-------
例子
-------

.. code-block:: java

  // 将表"dc_universe"改为"dc_name"
  c.renameTable("dc_universe", "dc_name").submit();

------------------------------------------------------------------------------

dropTable
=====================

.. code-block:: java

  c.dropTable(tableName);

从数据库删除一个表。表和它的所有数据将被删除;

------------
参数
------------

1. ``tableName``    - ``String``:  表名

-------
返回值
-------

``Chainsql`` - Chainsql对象

-------
例子
-------

.. code-block:: java

  c.dropTable("dc_universe").submit();

------------------------------------------------------------------------------

数据操作
*****************

insert
=====================

.. code-block:: java

  c.table(tableName).insert(raw).submit(SyncCond.db_success);

向数据库中插入数据。

------------
参数
------------

1. ``raw``    - ``List``:  raw类型必须都是例子中的json格式的数据类型;

-------
返回值
-------

``Table`` - Table对象

-------
例子
-------

.. code-block:: java

  // 向表"posts"中插入一条记录.
  c.table("posts").insert(c.array("{id: 1, 'name': 'peera','age': 22}", "{id: 2, 'name': 'peerb','age': 21}"))
  .submit(SyncCond.db_success);

------------------------------------------------------------------------------

update
=====================

.. code-block:: java

  c.table(tableName).get(raw).update(raw).submit();

更新表中数据。如果get添加为空，则更新表中所有记录；其中raw为json格式字符串;

------------
参数
------------

1. ``raw``    - ``List``:  raw类型必须都是例子中的json格式的数据类型;

-------
返回值
-------

``Table`` - Table对象

-------
例子
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

  c.table(tableName).get(raw).delete().submit(SyncCond.db_success);

从表中删除对应条件的数据，如果get条件为空，则删除所有数据

------------
参数
------------

-------
返回值
-------

``Table`` - Table对象

-------
例子
-------

.. code-block:: java

  // 删除 id 等于 1 的记录.
  c.table("comments")
  .get(c.array("{'id': 1}"))
  .delete()
  .submit(SyncCond.db_success);

------------------------------------------------------------------------------

数据查询
*****************

get
=====================

.. code-block:: java

  c.table(tableName).get(raw).submit();

从数据库查询数据,后面可以进行其他操作，例如update、delete等;

------------
参数
------------

1. ``raw``    - ``List``:  raw类型必须都是例子中的json格式的数据类型;

-------
返回值
-------

``Table`` - Table对象

-------
例子
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

  c.table(tableName).get(raw).limit("{index:0,total:10}").withFields([]).submit();;

对数据库进行分页查询.返回对应条件的数据;

------------
参数
------------

1. ``raw``    - ``List``:  raw类型必须都是例子中的json格式的数据类型;

-------
返回值
-------

``Table`` - Table对象

-------
例子
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

  c.table(tableName).get(raw).order(orgs);

对查询的数据按指定字段进行排序

------------
参数
------------

1. ``orgs``    - ``List<String>``:  orgs类型必须都是例子中的json格式的数据类型;

-------
返回值
-------

``Table`` - Table对象

-------
例子
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

  c.table(tableName).get(raw).withFields(orgs);

从数据库查询数据,并返回指定字段,必须个get配合使用;

------------
参数
------------

1. ``orgs``    - ``String``:  orgs类型必须都是例子中的json格式的数据类型;

-------
返回值
-------

``Table`` - Table对象

-------
例子
-------

.. code-block:: java

  // 查询 name 等于 aa 的记录.取name以及age字段
  c.table("posts")
  .get(c.array("{'name': 'aa'}"))
  .withFields("['name','age']")
  .submit();

------------------------------------------------------------------------------

权限管理
*****************

grant
=====================

.. code-block:: java

  c.grant(tableName,user,rightInfo).submit();
  c.grant(tableName,user,rightInfo,publicKey).submit();

向用户授予数据表操作权限(Select、Insert、Update、Delete)，授权为非数据库操作，validate_success即可

------------
参数
------------

1. ``tableName``    - ``String``:  授权的表名;
2. ``user``  - ``String``:  所要授权的账户地址;
3. ``rightInfo`` - ``String``:  为授权字符串，例如:{select: true, insert: true, update: true,delete:false};
4. ``publicKey``  - ``String``:  被授权账户的公钥（选填字段，用于加密表

-------
返回值
-------

``Chainsql`` - Chainsql对象

-------
例子
-------

.. code-block:: java

  // 向用户授予Insert、Delete操作权限.
  c.grant("Table", "User.address", "{insert:true, delete:true}")
  .submit(SyncCond.validate_success);


  // 取消用户Insert、Delete操作权限.
  c.grant("Table", "User.address", "{insert:false, delete:false}")
  .submit(SyncCond.validate_success);

------------------------------------------------------------------------------

事务相关
*****************

在事务开始和结束之间的insert，update，delete，assert语句会包装在一个原子操作中执行

与数据库的事务类似，事务中执行的语句要么全部成功，要么全部失败

执行事务类型交易主要涉及两个api：beginTran,commit.

beginTran开启事务，commit提交事务，事务中的操作全部执行成功事务才成功，有一个执行失败，则事务会自动回滚

在事务上下文中，不在支持单个语句的submit


beginTran
=====================

.. code-block:: java

  c.beginTran();

开启事务

commit
=====================

.. code-block:: java

  c.commit();

提交事务;本次事务期间的所有操作都会打包提交到区块链网络

-------
例子
-------
.. code-block:: java

  c.beginTran();

  c.table("posts").insert("{name: 'peera',age: 22}, {name: 'peerb',age: 21}");
  c.table("posts").get("{id: 1}").assert"({age:22,name:'peera'}");
  c.table("posts").get("{id: 1}").update("{age:52,name:'lisi'}");
  c.table("comments").delete("{id: 1}");

  c.commit();

------------------------------------------------------------------------------

区块信息
*****************

getLedger
=====================

.. code-block:: java

    getLedger();

-------
返回值
-------

``JSONObject`` - 成功返回最新区块信息，失败返回null;

.. code-block:: java

    getLedger(ledger_index);

------------
参数
------------

1. ``ledger_index``    - ``Integer``:  区块号;

-------
返回值
-------

``JSONObject`` - 成功返回最新区块信息，失败返回null;


getLedgerVersion
=====================

.. code-block:: java

    c.getLedgerVersion();

-------
返回值
-------

``Integer`` - 整数，当前的区块高度


getTransaction
=====================

.. code-block:: java

   JSONObject getTransaction (String hash);     //同步
   void getTransaction(String hash,Callback cb);//异步

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


getTransactions
=====================

.. code-block:: java

   JSONObject getTransactions (String address);     //同步
   void getTransactions(String address,Callback cb);//异步

查询某账户提交的最新20笔交易

------------
参数
------------

1. ``address``    - ``String``:  要查询的账户地址;
2. ``cb``   - ``Callback`` : 异步接口，参数为一回调函数

-------
返回值
-------

``JSONObject`` - 成功为交易信息数组，失败为null

------------------------------------------------------------------------------

事件订阅
*****************

subscribeTable
=====================

.. code-block:: java

  c.event.subscribeTable(tablename ,owner, callback);

订阅某张表

------------
参数
------------

1. ``tablename`` - ``String``:  表名;
2. ``owner``     - ``String``:  为表的所有者地址;
3. ``cb``        - ``Callback`` : 回调函数

.. code-block:: json

  {
    "owner":"zP8Mum8xaGSkypRgDHKRbN8otJSzwgiJ9M",
    "status":"validate_success",
    "tablename":"sub_message1", 
    "transaction": {
     
    },
    "type":"table"
  }

-------
返回值
-------

-------
例子
-------

.. code-block:: java

  // 用户订阅TestName表信息，表的创建者为zP8Mum8xaGSkypRgDHKRbN8otJSzwgiJ9M
  c.event.subscribeTable("TestName", "zP8Mum8xaGSkypRgDHKRbN8otJSzwgiJ9M",new Callback(){

    @Override
    public void called(Object args) {
      //do something here
    }
  });

------------------------------------------------------------------------------

unsubcribeTable
=====================

.. code-block:: java

  c.event.unsubcribeTable(owner, tablename);

取消订阅某张表

------------
参数
------------

1. ``owner`` - ``String``:          为表的所有者地址;
2. ``tablename``     - ``String``:  表名;

-------
返回值
-------

-------
例子
-------

.. code-block:: java

  // 用户取消订阅TestName表信息.
  c.event.unsubcribeTable("zP8Mum8xaGSkypRgDHKRbN8otJSzwgiJ9M");

------------------------------------------------------------------------------

subscribeTx
=====================

.. code-block:: java

  c.event.subscribeTx(txid, callback);

订阅某个交易

------------
参数
------------

1. ``txid`` - ``String``:          交易Hash;
2. ``callback``     - ``callback``:  回调函数;

-------
返回值
-------

-------
例子
-------

.. code-block:: java

  // 用户订阅交易Hash信息.
  c.event.subscribeTx(txid, (data) -> {
    //do something here
  }));

------------------------------------------------------------------------------

unsubscribeTx
=====================

.. code-block:: java

  c.event.unsubscribeTx(txid);

取消订阅某个交易

------------
参数
------------

1. ``txid`` - ``String``:          交易Hash;

-------
返回值
-------

-------
例子
-------

.. code-block:: java

  // 取消订阅交易Hash信息.
  c.event.unsubscribeTx(txid);

------------------------------------------------------------------------------

运算符
*****************
比较运算符(comparison operators)
==============================================


============   ===========================     =========================================
运算符	        说明	                           语法
============   ===========================     =========================================
$ne             不等于                           {field:{$ne:value}}
$eq	            等于                            	{field:{$eq:value}} or {field:value}
$lt	           小于	                            {field:{$lt:value}}
$le	           小于等于	                             {field:{$le:value}}
$gt	           大于	                            {field:{$gt:value}}
$ge	           大于等于	                           {field:{$ge:value}}
$in	            字段值在指定的数组中	                 {field:{$in:[v1, v2, ..., vn]}}
$nin	         字段值不在指定的数组中	                 {field:{$nin:[v1, v2, ..., vn]}}
============   ===========================     =========================================


逻辑运算符(logical operators)
==============================================


============   ===========     =========================================
逻辑符	        说明	                           语法
============   ===========     =========================================
$and	          逻辑与	             {$and:[{express1},{express2},...,{expressN}]}
$or           	逻辑或                {$or:[{express1},{express2},...,{expressN}]}
============   ===========     =========================================


模糊匹配(fuzzy matching)
==============================================


=========================================     =========================================
语法	                                           说明	                           
=========================================     =========================================
{"field":{"$regex":"/value/"}}	                like "%value%"
{"field":{"$regex":"/^value/"}}	                like "%value"
=========================================     =========================================


-------
例子
-------

.. code-block:: javascript

  where id > 10 and id < 100
  对应 json 对象
    {
      $and: [
        {
          id: {$gt: 10}
        },
        {
          id: {$lt: 100}
        }
      ]
    }

.. code-block:: javascript

  where id > 10 and id < 100
  对应 json 对象
  {
    $and: [
      {
        id: {$gt:10}
      },
      {
        id: {$lt:100}
      }
    ]
  }

.. code-block:: javascript

  where name = 'peersafe' or name = 'zongxiang'
  对应 json 对象
  {
    $or: [
      {
        name: {$eq:'peersafe'}
      },
      {
        name: {$eq:zongxiang}
      }
    ]
  }

.. code-block:: javascript

  where (id > 10 and name = 'peersafe') or name = 'zongxiang'
  对应 json 对象
  {
    $or: [
      {
        $and: [
          {
            id: {$gt:10}
          },
          {
            name:'peersafe'
          }]
      },
      {
        name:'zongxiang'
      }
    ]
  }

.. code-block:: javascript

  where name like '%peersafe%'
  对应 json 对象
  {
    name: {
      $regex:'/peersafe/'
    }
  }

.. code-block:: javascript

  where name like '%peersafe'

  对应 json 对象

  {
    name: {
      $regex:'/^peersafe/'
    }
  }

------------------------------------------------------------------------------

智能合约操作
================

.. toctree::
   :maxdepth: 2

   javaSmartContract