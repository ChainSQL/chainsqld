===========
Node.js接口
===========

ChainSQL提供nodejs-API与节点进行交互，实现基础交易发送、链数据查询、数据库操作、智能合约操作等ChainSQL区块链交互操作。

环境准备
===========

Nodejs接口的使用，需要以下准备：

1. 安装nodejs环境
2. 安装chainsql的nodejs模块

---------------
安装nodejs环境
---------------
.. _nodejs下载: https://nodejs.org/zh-cn/download/

 | 初次安装nodejs，可以到官方 `nodejs下载`_ 网址下载对应平台的最新版本安装包。
 | ChainSQL推荐使用nodejs的版本为 **v8.5.0及以上** 。可以通过命令 ``node -v`` 查看本机nodejs版本。

-------------------------
安装chainsql的nodejs模块
-------------------------
在nodejs项目文件中使用 ``npm i chainsql`` 命令即可以将chainsql的nodejs模块安装到项目中，然后按照以下方式引入：

.. code-block:: javascript

	// chainsql模块0.6.56及之前的版本引入方式
	const ChainsqlAPI = require('chainsql').ChainsqlAPI;
	// chainsql模块0.6.57及之后的版本引入方式
	const ChainsqlAPI = require('chainsql');
	
	// 引入之后使用new创建全局chainsql对象，之后使用chainsql对象进行接口操作
	const chainsql = new ChainsqlAPI();

.. _Node.js返回值:

返回值格式
===========

ChainSQL的接口类型可以分为交易类和查询类，每种接口类型返回数据格式不同，下面就交易类和查询类进行说明。

----------------
交易类接口返回值
----------------

交易类接口是指提交内容需要经过ChainSQL区块链共识的。在nodejs的接口里主要通过submit接口去提交交易，返回值格式可以参见 :ref:`submit接口返回值 <submit-return>`。

----------------
查询类接口返回值
----------------

查询类接口返回值正常返回格式在各个接口有说明，错误返回值为一个 ``JsonObject`` 对象，具体包含以下字段：

	* ``name`` - ``String`` : 错误类型或者说错误码；
	* ``message`` - ``String`` : 详细错误说明。

基础接口
===========

-----------
as
-----------
.. code-block:: javascript

	chainsql.as(user)

部分接口与节点进行交互操作前，需要指明一个全局的操作账户，这样避免在每次接口的操作中频繁的提供账户。再次调用该接口即可修改全局操作账户。

参数说明
-----------
.. _user-Format:

1. ``user`` - ``JsonObject`` : chainsql账户，包含账户地址、账户私钥和账户公钥，其中账户公钥为可选字段。

.. code-block:: javascript

	const user = {
		secret: "xhFyeP6kFEtm......Rx7Zqu2xFvm",
		address: "zLtH4NFSqDFioq......Lf8xcyyw7VCf",
		publicKey: "cBPjhaYGqKMAShw......7Sd3JHKv5L3Uj2yVJfdV"
	};

返回值
-----------

None

示例
-----------
.. code-block:: javascript

	const user = {
		secret: "xhFyeP6kFEtm7KSyACRx7Zqu2xFvm",
		address: "zLtH4NFSqDFioq5zifriKKLf8xcyyw7VCf",
		publicKey: "cBPjhaYGqKMAShwbL7rnudzndBXWSKV57Sd3JHKv5L3Uj2yVJfdV"
	};
	chainsql.as(user);

------------------------------------------------------------------------------

-----------
use
-----------
.. code-block:: javascript

	chainsql.use(tableOwnerAddr)

use接口主要使用场景是针对ChainSQL的表操作，为其提供 **表的拥有者账户地址** 。

.. note::
	use接口和as接口的区别如下：

	- as接口的目的是设置全局操作账户GU，这个GU在表操作接口中扮演表的操作者，并且默认情况下也是表的拥有者（即表的创建者）。
	- 当as接口设置的账户不是即将操作的表的拥有者时，即可能通过表授权获得表的操作权限时，需要使用use接口设置表的拥有者地址。
	- use接口只需要提供账户地址即可，设置完之后，表的拥有者地址就确定了，再次使用use接口可以修改表拥有者地址。

参数说明
-----------

1. ``tableOwnerAddr`` - ``String`` : 账户地址

返回值
-----------

None

示例
-----------
.. code-block:: javascript

	const tableOwnerAddr = "zMpUjXckTSn6NacRq2rcGPbjADHBCPsURF"
	chainsql.use(tableOwnerAddr);

------------------------------------------------------------------------------

-----------
connect
-----------
.. code-block:: javascript

	chainsql.connect(wsAddr[, callback])

如果需要nodejs接口与节点进行交互，必须设置节点的websocket地址，与节点进行正常连接。

参数说明
-----------

1. ``wsAddr`` - ``String`` : 节点的websocket访问地址，格式为：``"ws://ip:port"`` 。
2. ``callback`` - ``Function`` : [**可选**]回调函数，如果指定，则通过指定回调函数返回结果，否则需要通过then和catch接收promise结果。

返回值
-----------

``JsonObject`` - 通过提供的websocket地址与节点连接的结果。返回方式取决于是否指定回调函数。

1. 连接成功 - 指定回调函数，则通过回调函数返回，否则返回返回一个resolve的Promise对象。 主要内容在返回值resObj的 ``resObj.api.connect`` 中，为一个 ``JsonObject``, 包括以下字段：
	
	* ``_fee_base`` - ``Number`` : 节点的基础交易手续费；
	* ``_ledgerVersion`` - ``Number`` : 当前节点区块高度；
	* ``_maxListeners`` - ``Number`` : 节点的最大连接数。
	* ``_url`` - ``String`` : 目前与节点连接使用的websocket地址
	
2. 部署失败 - 指定回调函数，则通过回调函数返回，否则返回返回一个reject的Promise对象。 ``JsonObject`` 主要包含以下字段：

	* ``message`` - ``String`` : 返回与指定ws地址链接失败："connect ECONNREFUSED wsAddr"；
	* ``name`` - ``String`` : 固定值："NotConnectedError"；

示例
-----------
.. code-block:: javascript

	// use the promise
	chainsql.connect("ws://127.0.0.1:6006").then(res => {
		console.log(res);
	}).catch(err => {
		console.error(err);
	});

	// use the callback
	chainsql.connect("ws://101.201.40.123:5006", function(err, res) {
		err ? console.error(err) : console.log(res);
	});

------------------------------------------------------------------------------

-----------
submit
-----------
.. code-block:: javascript

	chainsql.submit([param])

| 针对ChainSQL的交易类型的操作，需要使用submit接口执行提交上链操作，交易类型的操作是指需要进行区块链共识的操作。
| 还有一类ChainSQL的查询类操作，不需要使用submit接口，不需要进行区块链共识。 
| submit接口有使用前提，需要事先调用其他操作接口将交易主体构造，比如创建数据库表，需要调用createTable接口，然后调用submit接口，详细使用方法在具体接口处介绍。

.. note::
	在ChainSQL的查询类操作中，数据库查询接口[ :ref:`get接口 <get-API>` ]仍然需要submit是为了进行查询权限验证。

.. _submit-param:

参数说明
-----------

1. ``param`` - ``any`` : [**可选**]submit的参数是可选的，主要有以下3中情况：

	* ``JsonObject`` : 包含一个expect字段，表明期望交易发送之后达到某种预期结果就返回，可选值为``String``格式，可选结果如下：

		- "send_success" : 交易发送成功即返回结果；
		- "validate_success" ： 交易共识成功即返回结果；
		- "db_success" ： 涉及数据库交易，执行入库成功即返回结果。

	* ``Function`` : 回调函数，从该回调函数返回结果，expect以"send_success"为默认值进行交易提交。未指定回调函数则通过then和catch接收promise结果。
	* 不填写参数，则表明没有指定回调函数，然后expect以"send_success"为默认值进行交易提交。

.. _submit-return:

返回值
-----------

``JsonObject`` : 交易执行结果的返回值，如果指定回调函数，则通过回调函数返回，否则返回一个Promise的对象。

1. 执行成功，则 ``JsonObject`` 中包含两个字段：

	* ``status`` - ``String`` : 为提交时expect后的设定值，如果没有，则默认为"send_success"；
	* ``tx_hash`` - ``String`` : 交易哈希值，通过该值可以在链上查询交易。
2. 执行失败，有两种情况，一种是交易提交前的信息检测，一种是交易提交后共识出错。

	* 第一种信息检测出错，``JsonObject`` 中主要包含以下字段：

		- ``name`` - ``String`` : 错误类型；
		- ``message`` - ``String`` : 错误具体描述。

	* 第二种交易提交之后共识出错，``JsonObject`` 中包含以下字段：

		- ``resultCode`` - ``String`` : 错误类型或者说错误码，可参考 :ref:`交易类错误码 <tx-errcode>`；
		- ``resultMessage`` - ``String`` : 错误具体描述。

示例
-----------
.. code-block:: javascript

	const userAddr = "zMpUjXckTSn6NacRq2rcGPbjADHBCPsURF";

	//use the promise
	chainsql.pay(userAddr,"2000").submit({expect:'validate_success'}).then(res => {
		console.log(res);
	}).catch(err => {
		console.error(err);
	});
	> if success, res:
	{
		status:"validate_success",
		tx_hash:"8626D36AE98E081DD1647A6058377DC83E79666E65B925927496913A1D71DB8A"
	}

	//use the callback
	chainsql.pay(userAddr,"2000").submit(function(err, res) {
		err ? console.error(err) : console.log(res);
	});

------------------------------------------------------------------------------

.. _pay-introduce:

---------------
pay（账户转账）
---------------
.. code-block:: javascript

	chainsql.pay(address, amount[, memos])

ChainSQL区块链中账户之间转账接口，支持系统币与发行的代币。

.. note::

	**0.6.35** 版本之前，直接调用pay即可完成交易提交。
	**0.6.35** 版本及之后的版本，使用本接口只是构造转账交易，最后需要调用submit接口进行交易的提交。

参数说明
-----------

1. ``address`` - ``String`` : 接收转账方地址
2. ``amount`` - ``any`` : 转账数额，参数有两种提供格式：

	* ``String`` : 字符串格式，只能用于ChainSQL的系统币ZXC，直接提供金额的字符串形式进行转账；
	* ``JsonObject`` : Json对象，主要针对网关发行的非系统币，系统币ZXC也可以用这种形式，包含以下三个字段：

		- ``value`` - ``String`` : 转账数额；
		- ``currency`` - ``String`` : 转账币种；
		- ``issuer`` - ``String`` : 该币种的发行网关地址。
3. ``memos`` - ``String`` : [**可选**]用于对这笔转账的说明，最后会随该笔转账交易记录到区块链上。

返回值
-----------

``JsonObject`` : 返回chainsql对象本身。

示例
-----------
.. code-block:: javascript

	const userAddr = "zMpUjXckTSn6NacRq2rcGPbjADHBCPsURF";

	//use the promise
	chainsql.pay(userAddr, "2000").submit({expect:'validate_success'}).then(res => {
		console.log(res);
	}).catch(err => {
		console.error(err);
	});
	

------------------------------------------------------------------------------

----------------
generateAddress
----------------
.. code-block:: javascript

	chainsql.generateAddress([secret])

生成一个ChainSQL账户，但是此账户未在链上有效，需要链上有效账户对新账户发起pay操作，新账户才有效。

参数说明
-----------

1. ``secret`` - ``String`` : [**可选**]参数为私钥，通过指定私钥，可返回基于该私钥的账户和公私钥对。该参数为空则生成一对新的公私钥及对应地址。

返回值
-----------

1. ``JsonObject`` : 一个账户对象，主要包含字段如下：

	* ``address`` - ``String`` : 新账户地址，是原始十六进制的base58编码；
	* ``publicKey`` - ``String`` : 新账户公钥，是原始十六进制的base58编码；
	* ``secret`` - ``String`` : 新账户私钥，是原始十六进制的base58编码。

示例
-----------
.. code-block:: javascript

	let accountNew = chainsql.generateAddress();
	console.log(accountNew);
	>
	{
		address:"zwqrah4YEKCxLQM2oAG8Qm8p1KQ5dMB9tC"
		publicKey:"cB4uvqvj49hBjXT25aYYk91K9PwFn8A12wwQZq8WP5g2um9PJFSo"
		secret:"xnBUAtQZMEhDDtTtfjXhK1LE5yN6D"
	}

------------------------------------------------------------------------------

---------------
getServerInfo
---------------
.. code-block:: javascript

	chainsql.getServerInfo([callback])

获取当前区块链基础信息

参数说明
-----------

1. ``callback`` - ``Function`` : [**可选**]回调函数，如果指定，则通过指定回调函数返回结果，否则返回一个promise对象。

返回值
-----------

1. ``JsonObject`` : 包含区块链基础信息，详细字段可在 :ref:`命令行server_info返回结果 <serverInfo-return>` 中查看， 主要字段介绍如下：

	* ``buildVersion`` - ``String`` : 节点程序版本
	* ``completeLedgers`` - ``String`` : 当前区块范围
	* ``peers`` - ``Number`` : peer节点数量
	* ``validationQuorum`` - ``Number`` : 完成共识最少验证节点个数

示例
-----------
.. _return-example:
.. code-block:: javascript

	//use the promise
	chainsql.getServerInfo().then(res => {
		console.log(res);
	}).catch(err => {
		console.error(err);
	});

	//use the callback
	chainsql.getServerInfo(function(err, res) {
		err ? console.error(err) : console.log(res);
	});

------------------------------------------------------------------------------

---------------
getAccountInfo
---------------
.. code-block:: javascript

	chainsql.getAccountInfo(address[, callback])

从链上请求查询账户信息。

参数说明
-----------

1. ``address`` - ``String`` : 账户地址；
2. ``callback`` - ``Function`` : [**可选**]回调函数，如果指定，则通过指定回调函数返回结果，否则返回一个promise对象。

返回值
-----------

1. ``JsonObject`` : 包含账户基本信息。正常返回主要字段如下：

	* ``sequence`` - ``Number`` : 该账户交易次数；
	* ``zxcBalance`` - ``String`` : 账户ZXC系统币的余额；
	* ``ownerCount`` - ``Number`` : 该账户在链上拥有的对象个数，如与其他账户建立的TrustLine，或者创建的一个表都作为一个对象。
	* ``previousAffectingTransactionID`` - ``String`` : 上一个对该账户有影响的交易哈希值；
	* ``previousAffectingTransactionLedgerVersion`` - ``Number`` : 上一个对该账户有影响的区块号。

.. note::
	返回值中的 ``previousAffectingTransactionID`` 里的交易不包括 TrustLine 和 挂单交易。

示例
-----------
.. code-block:: javascript

	try {
		chainsql.getAccountInfo("zHb9CJAWyB4zj91VRWn96DkukG4bwdtyTh").then(res => {
			console.log(res);
		}).catch(err => {
			console.error(err);
		});
	}catch(e){
		console.error(e);
	}
	>
	{
		ownerCount:35,
		previousAffectingTransactionID:"0099076BA13A03AEE5179A71F0A2E678DEA6891CAC6E05EC1656D6F857E071CB",
		previousAffectingTransactionLedgerVersion:4957964,
		sequence:2792,
		zxcBalance:"9974287188.189876",
	}

------------------------------------------------------------------------------

-----------
getLedger
-----------
.. code-block:: javascript

	chainsql.getLedger([[opts], [callback]])

返回指定区块的区块头信息，如果在opt中指定详细信息，则会额外返回所有交易信息或者账户状态。

参数说明
-----------

1. ``opts`` - ``JsonObject`` : [**可选**]指定区块高度，定制返回内容。如果想默认获取最新区块信息，可以不填opt参数。可选字段如下：

	* ``includeAllData`` - ``Boolean`` : [**可选**]设置为True，如果又将includeState和(或)includeTransactions设置为True，会将详细交易和(或)详细账户状态返回；
	* ``includeState`` - ``Boolean`` : [**可选**]设置为True，会返回一个包含状态哈希值的数组；如果同时将includeAllData设置为True，则会返回一个包含状态详细信息的数组；
	* ``includeTransactions`` - ``Boolean`` : [**可选**]设置为True，会返回一个包含交易哈希值的数组；如果同时将includeAllData设置为True，则会返回一个包含交易详细信息的数组；
	* ``ledgerHash`` - ``String`` : [**可选**]将返回此指定区块哈希值的区块头信息；
	* ``ledgerVersion`` - ``integer`` : [**可选**]将返回此指定区块高度的区块头信息。

2. ``callback`` - ``Function`` : [**可选**]回调函数，如果指定，则通过指定回调函数返回结果，否则返回一个promise对象。

返回值
-----------

.. _区块信息字段说明: https://developers.ripple.com/rippleapi-reference.html#getledger

1. ``JsonObject`` : 区块信息，可参考 `区块信息字段说明`_


示例
-----------
.. code-block:: javascript

	const opts = {
		ledgerVersion : 418,
		includeAllData : false,
		includeTransactions : true,
		includeState : false
	}
	chainsql.getLedger(opts, function(err, res) {
		err ? console.error(err) : console.log(res);
	});
	> res:
	{
		closeFlags:0
		closeTime:"2019-04-18T07:58:10.000Z"
		closeTimeResolution:10
		ledgerHash:"6FBC02023B7D2042EAABB717B0C1F764658C7DAB24F20AF609DACDD9A6EFAAEA"
		ledgerVersion:666
		parentCloseTime:"2019-04-18T07:58:02.000Z"
		parentLedgerHash:"8FFB1DF583D37283232A328D5F97909A350DD92BC54491D42A04F670D62997A5"
		stateHash:"4CA23AC3023743077A75E2B180CEC976D7F96D7DFCF4E62889B45BE2106A44C7"
		totalDrops:"99999999999999988"
		transactionHash:"8636D36AE86E081DD1647A6058395DC83E79666E65B925927496418A1D71DB8A"
	}

------------------------------------------------------------------------------

-----------------
getLedgerVersion
-----------------
.. code-block:: javascript

	chainsql.getLedgerVersion([callback])

获取最新区块高度（区块号）

参数说明
-----------

1. ``callback`` - ``Function`` : [**可选**]回调函数，如果指定，则通过指定回调函数返回结果，否则返回一个promise对象。

返回值
-----------

1. ``Number`` : 最新区块高度

示例
-----------
.. code-block:: javascript

	chainsql.getLedgerVersion(function(err, res) {
		err ? console.error(err) : console.log(res);
	});
	> 503

------------------------------------------------------------------------------

-----------------------
getAccountTransactions
-----------------------
.. code-block:: javascript

	chainsql.getAccountTransactions(address[, opts][, cb])

获取账户的交易信息

参数说明
-----------

1. ``address`` - ``String`` : 账户地址
2. ``opts`` - ``JsonObject`` : [**可选**]返回值限定值，可选字段如下：

	* ``binary`` - ``boolean`` : [**可选**]如果为True，则节点返回一个二进制格式的结果，而不是一个可读的Json对象；
	* ``counterparty`` - ``address`` : [**可选**]如果为True，则节点只返回涉及指定网关的交易；
	* ``earliestFirst`` - ``boolean`` : [**可选**]如果为True，则节点将按最早的交易排前为顺序，默认是最新交易排前；
	* ``excludeFailures`` - ``boolean`` : [**可选**]如果为True，则节点只返回成功的交易；
	* ``includeRawTransactions`` - ``object`` : [**可选**]提供交易的原始数据，即交易对象，主要为调试使用，节点需要解析提供的原始交易；
	* ``initiated`` - ``boolean`` : [**可选**]如果为True，则节点只返回又第一个参数address作为交易发起者的交易，如果为False，则只返回address不是交易发起者的交易；
	* ``limit`` - ``integer`` : [**可选**]限定节点返回的交易个数；
	* ``maxLedgerVersion`` - ``integer`` : [**可选**]节点返回此指定区块之前区块中该账户的交易；
	* ``minLedgerVersion`` - ``integer`` : [**可选**]节点返回此指定区块之后区块中该账户的交易；
	* ``start`` - ``string`` : [**可选**]某个交易哈希值，如果指定，则从指定交易哈希的交易开始按序返回交易。不可以和maxLedgerVersion和minLedgerVersion一起使用；
	* ``types`` - ``TransactionTypes`` : [**可选**]节点只返回该交易类型的交易；

3. ``callback`` - ``Function`` : [**可选**]回调函数，如果指定，则通过指定回调函数返回结果，否则返回一个promise对象。。

返回值
-----------

1. ``Array`` : 返回一个包含交易信息的数组，每个交易信息的字段可参考getTransaction的返回值；如果指定回调函数，则通过回调函数返回，否则返回一个Promise对象。

示例
-----------
.. code-block:: javascript

	//use the callback
	const address = "zMpUjXckTSn6NacRq2rcGPbjADHBCPsURF";
	const opt = {limit:1};
	chainsql.getAccountTransactions(address, opt, function(err, res) {
		err ? console.error(err) : console.log(res);
	});
	>
	[
		{
			"type":"payment",
			"address":"zHyz3V6V3DZ2fYdb6AUc5WV4VKZP1pAEs9",
			"sequence":2791,
			"id":"0099076BA13A03AEE5179A71F0A2E678DEA6891CAC6E05EC1656D6F857E071CB",
			"specification":{
				"source":{
					"address":"zHyz3V6V3DZ2fYdb6AUc5WV4VKZP1pAEs9",
					"maxAmount":{"currency":"ZXX","value":"10000"}
				},
				"destination":{
					"address":"zob3V1NnCsFNYdGNod3auU5asmK1SkJs7",
					"amount":{"currency":"ZXX","value":"100"}
				}
			},
			"outcome":{
				"result":"tesSUCCESS",
				"timestamp":"2019-04-30T03:49:00.000Z",
				"fee":"0.000012",
				"balanceChanges":{
					"zHyz3V6V3DZ2fYdb6AUc5WV4VKZP1pAEs9":[{"currency":"ZXC","value":"-0.000012"},{"counterparty":"zob3V1NnCsFNYdGNod3auU5asmK1SkJs7","currency":"ZXX","value":"-100"}],
					"zob3V1NnCsFNYdGNod3auU5asmK1SkJs7":[{"counterparty":"zHyz3V6V3DZ2fYdb6AUc5WV4VKZP1pAEs9","currency":"ZXX","value":"100"}]
				},
				"orderbookChanges":{},
				"ledgerVersion":4957964,
				"indexInLedger":0,
				"deliveredAmount":{
					"currency":"ZXX",
					"value":"100",
					"counterparty":"zob3V1NnCsFNYdGNod3auU5asmK1SkJs7"
				}
			}
		}
	]

------------------------------------------------------------------------------

---------------
getTransaction
---------------
.. code-block:: javascript

	chainsql.getTransaction(txHash[, callback])

获取指定交易哈希值的交易详情

参数说明
-----------

1. ``txHash`` - ``String`` : 交易的哈希值
2. ``callback`` - ``Function`` : [**可选**]回调函数，如果指定，则通过指定回调函数返回结果，否则返回一个promise对象。

返回值
-----------

.. _获取交易: https://developers.ripple.com/rippleapi-reference.html#gettransaction

``JsonObject`` : 交易详情，具体字段描述可参考 `获取交易`_ 的Return Value部分对交易详情的描述。

示例
-----------
.. code-block:: javascript

	//use the callback
	const txHash = "F1C0981FDFA67CB60A30BD6373029662EC0BBCF3FC50BB02C98812726369B3F9";
	chainsql.getTransaction(txHash, function(err, res) {
		err ? console.error(err) : console.log(res);
	});
	>
	{
		"type":"payment",
		"address":"zHyz3V6V3DZ2fYdb6AUc5WV4VKZP1pAEs9",
		"sequence":2791,
		"id":"0099076BA13A03AEE5179A71F0A2E678DEA6891CAC6E05EC1656D6F857E071CB",
		"specification":{
			"source":{
				"address":"zHyz3V6V3DZ2fYdb6AUc5WV4VKZP1pAEs9",
				"maxAmount":{"currency":"ZXX","value":"10000"}
			},
			"destination":{
				"address":"zob3V1NnCsFNYdGNod3auU5asmK1SkJs7",
				"amount":{"currency":"ZXX","value":"100"}
			}
		},
		"outcome":{
			"result":"tesSUCCESS",
			"timestamp":"2019-04-30T03:49:00.000Z",
			"fee":"0.000012",
			"balanceChanges":{
				"zHyz3V6V3DZ2fYdb6AUc5WV4VKZP1pAEs9":[{"currency":"ZXC","value":"-0.000012"},{"counterparty":"zob3V1NnCsFNYdGNod3auU5asmK1SkJs7","currency":"ZXX","value":"-100"}],
				"zob3V1NnCsFNYdGNod3auU5asmK1SkJs7":[{"counterparty":"zHyz3V6V3DZ2fYdb6AUc5WV4VKZP1pAEs9","currency":"ZXX","value":"100"}]
			},
			"orderbookChanges":{},
			"ledgerVersion":4957964,
			"indexInLedger":0,
			"deliveredAmount":{
				"currency":"ZXX",
				"value":"100",
				"counterparty":"zob3V1NnCsFNYdGNod3auU5asmK1SkJs7"
			}
		}
	}

------------------------------------------------------------------------------

-----------
sign
-----------
.. code-block:: javascript

	chainsql.sign(txJson, secret[, option])

交易签名接口，只能对交易进行签名。

参数说明
-----------

.. _交易结构: https://developers.ripple.com/rippleapi-reference.html#transaction-types

1. ``txJson`` - ``JsonObject`` : 交易对象，不同交易类型，结构不同，可参考 `交易结构`_ 的说明。对chainsql的表及合约交易结构的说明可参考 :ref:`rpc接口 <rpc-tx>` 中每个接口的tx_json字段值；
2. ``secret`` - ``String`` : 签名者的私钥。
3. ``option`` - ``JsonObject`` : [**可选**]进行多方签名时，需要利用option参数提供签名账户地址，此时secret参数应为option中账户的私钥。此对象只包含一个字段：
	
	* ``signAs`` - ``String`` : 字符串，签名账户的地址。

返回值
-----------

``JsonObject`` : 返回的签名结果，主要包含以下字段：

	* ``signedTransaction`` - ``String`` : 原交易序列化之后的十六进制；
	* ``id`` - ``String`` : 交易签名之后的十六进制签名值。

示例
-----------
.. code-block:: javascript

	const user = {
		address:"zwqrah4YEKCxLQM2oAG8Qm8p1KQ5dMB9tC",
		publicKey:"cB4uvqvj49hBjXT25aYYk91K9PwFn8A12wwQZq8WP5g2um9PJFSo",
		secret:"xnBUAtQZMEhDDtTtfjXhK1LE5yN6D"
	}
	let info = await chainsql.getAccountInfo(user.address);
	chainsql.getLedgerVersion(function(err,data){
		const payment = {
			"Account": "zwqrah4YEKCxLQM2oAG8Qm8p1KQ5dMB9tC",
			"Amount":"1000000000",
			"Destination": "zPBJ6Ai7JoQxYcYhF8gqsBDdhUVdaqQq2u",
			"TransactionType": "Payment",
			"Sequence": info.sequence,
			"LastLedgerSequence":data + 5,
			"Fee":"50"
		}
		let signedRet = chainsql.sign(payment, user.secret);
	}
	>
	{
  		"signedTransaction": "12000322800000002400000017201B0086955368400000000000000C732102F89EAEC7667B30F33D0687BBA86C3FE2A08CCA40A9186C5BDE2DAA6FA97A37D874473045022100BDE09A1F6670403F341C21A77CF35BA47E45CDE974096E1AA5FC39811D8269E702203D60291B9A27F1DCABA9CF5DED307B4F23223E0B6F156991DB601DFB9C41CE1C770A726970706C652E636F6D81145E7B112523F68D2F5E879DB4EAC51C66980503AF",
  		"id": "02BAE87F1996E3A23690A5BB7F0503BF71CCBA68F79805831B42ABAD5913BA86"
	}

	//multisigning
	const signer = {
		address:"zN9xbg1aPEP8aror3n7yr6s6JBGr7eEPeC",
		publicKey:"cBQV4TokXMD4fumjtUrBi1Jj8p6iAeK74F2SeHih7jnTECzXYdJU",
		secret:"xnrMj6Nyc3KYHn244KdMyaJn1wLUy"
	}
	const option = {
		signAs:"zN9xbg1aPEP8aror3n7yr6s6JBGr7eEPeC"
	}
	let signedRet = chainsql.sign(payment, signer.secret, option);

------------------------------------------------------------------------------

-----------------
getTableNameInDB
-----------------
.. code-block:: javascript

	chainsql.getTableNameInDB(address, tableName);

查询表在数据库中的记录的名字，在外定义的表名，经过ChainSQL会进行格式转换，此接口查询转换之后的名字。

参数说明
-----------

1. ``address`` - ``String`` : 账户地址,表的拥有者；
2. ``tableName`` - ``String`` : 原始表名

返回值
-----------

``tableNameInDB`` - ``String`` : 通过Promise对象返回转换之后的表名。

示例
-----------
.. code-block:: javascript

	chainsql.getTableNameInDB(owner.address, sTableName).then(res => {
		console.log(res);
	}).catch(err => {
		console.error(err);
	});
	>
	"B3E76AD6CAF7D483C5339787FF0257C58223C8CB"

------------------------------------------------------------------------------

-------------
getTableAuth
-------------
.. code-block:: javascript

	chainsql.getTableAuth(ownerAddr, tableName[, account])

参数说明
-----------

1. ``ownerAddr`` - ``String`` : 表的拥有者地址；
2. ``tableName`` - ``String`` : 表名；
3. ``account`` - ``Array`` : 查询指定账户的授权情况，当此参数不填时，返回指定表的所有授权情况。指定账户数组，只返回指定账户的授权情况。

返回值
-----------

``JsonObject`` : 由以下字段组成：

	* ``owner`` - ``String`` : 表的拥有者地址；
	* ``tablename`` - ``String`` : 表名；
	* ``users`` - ``Array`` : 由 ``JsonObject`` 组成的数组，每个 ``JsonObject`` 为某个账户的授权信息，具体字段如下：

		- ``account`` - ``String`` : 被授权账户地址；
		- ``authority`` - ``JsonObject`` : 具体被授予的操作权限，key为insert、delete、update、select，value为true或者false；

示例
-----------
.. code-block:: javascript

	chainsql.getTableAuth(owner.address,"b1",[]).then(res => {
		console.log(res);
	}).catch(err => {
		console.error(err);
	});
	>
	{
		owner:"zHb9CJAWyB4zj91VRWn96DkukG4bwdtyTh",
		tablename:"tableTest",
		users:[
			{
				account:"zHb9CJAWyB4zj91VRWn96DkukG4bwdtyTh",
				authority:{
					delete:true,
					insert:true,
					select:true,
					update:true
				}
			},
			{
				account:"zLtH4NFSqDFioq5zifriKKLf8xcyyw7VCf",
				authority:{
					delete:false,
					insert:true,
					select:true,
					update:false
				}
			}
		]
	}

------------------------------------------------------------------------------

-----------------
getAccountTables
-----------------
.. code-block:: javascript

	chainsql.getAccountTables(address[, bGetDetailInfo])

查询账户名下创建的表。

参数说明
-----------

1. ``adress`` - ``String`` : 账户地址；
2. ``bGetDetailInfo`` - ``bGetDetailInfo`` : [**可选**]默认为false。为True时获取详细信息。

返回值
-----------

``JsonObject`` : key为tables， 值为一个数组，由账户下所有表信息组成，每个表信息为一个 ``JsonObject`` ，包含字段取决于bGetDetailInfo，为false，只包含ledger_index、nameInDB、tablename、tx_hash。为true，详细字段信息如下：

	* ``confidential`` - ``Boolean`` : 是否是加密表；
	* ``create_time`` - ``Number`` : 表创建时间，chainsql时间的秒数；
	* ``ledger_hash`` - ``String`` : 建表交易所在区块的区块哈希值；
	* ``ledger_index`` - ``Number`` : 建表交易所在区块的区块高度；
	* ``nameInDB`` - ``String`` : 转换之后的数据库表名；
	* ``raw`` - ``String`` : 建表交易raw的十六进制形式；
	* ``table_exist_inDB`` - ``Boolean`` : 表是否真正在数据库中；
	* ``tablename`` - ``String`` : 表名的可读形式；
	* ``tx_hash`` - ``String`` : 建表交易的哈希值。

示例
-----------
.. code-block:: javascript

	const ownerAddr = "zMpUjXckTSn6NacRq2rcGPbjADHBCPsURF";
	chainsql.getAccountTables(ownerAddr, true).then(res => {
		console.log(res);
	}).catch(err => {
		console.error(err);
	});
	>
	{
		tables : [
			{
				confidential:true
				create_time:608977031
				ledger_hash:"8AC443126E74AE250CD438EF5414358D23F79CCA5A67A375AB19077FD24DBAB9"
				ledger_index:11657
				nameInDB:"B3E76AD6CAF7D483C5339787FF0257C58223C8CB"
				raw:"594EC5E54AB2E6EBF8B2AC61A3A8839D5B2C3D15EEDD0CFA10D0D5EC3FD9A0C519B8AA03F3C4D0EDC1F9C5CF89C20EE684B510F5B4D14D980BEB60511BCB8852ED68F2FD75A7F5AF46DACB4380A72D5E3D89A0F3A1CD39B47A2EACC3C384F6F2EA2307FD497966CCC9317AA58BD01C56D79BCA8FBF0997393D0AD88546B767F8750F1CF17DD4523DAF6659B6697C8D955D37970C1E8A040961DFA6DBEA8E13C9"
				table_exist_inDB:false
				tablename:"tableTest"
				tx_hash:"2282CC29603E5141EAC96B25FF4BF10E896103A06F7C055FCA90C9956B6C98F9"
			}
		]
	}

------------------------------------------------------------------------------


网关交易
===========

-----------
accountSet
-----------
.. code-block:: javascript

	chainsql.accountSet(option)

账户属性设置。为交易类型，需要调用submit提交交易。

参数说明
-----------

1. ``option`` - ``JsonObject`` : 账户设置的可选属性，有一下几个：

	* ``enableRippling`` - ``Boolean`` : 是否开启网关的rippling功能，即信任同一网关同一货币的账户之间是否可以转账；
	* ``rate`` - ``Number`` : 信任同一网关同一货币的账户之间转账费率，取值为1.0~2.0；
	* ``min`` - ``Number`` : 根据rate计算出转账手续费后如果小于min值，则取min值；
	* ``max`` - ``Number`` : 根据rate计算出转账手续费后如果大于max值，则取max值

.. note::

	* 关于费率：可取消设置，min/max=0,rate=1.0为取消设置。
	* 每次设置都是重新设置，之前的设置会被替代。
	* min,max可只设置一个，但这时要同时设置rate。
	* 如果只设置rate，默认为取消设置min,max。
	* 如果只设置min与rate，max会被取消设置,同理只设置max与rate，min会被取消设置。

返回值
-----------

``JsonObject`` : 返回chainsql对象本身。

示例
-----------
.. code-block:: javascript

	chainsql opt = {
        enableRippling: true,
        rate: 1.002,
        min: 1,
        max: 1.5
    }
	chainsql.accountSet(opt).submit({ expect: "validate_success"}).then(res => {
		console.log(res);
	}).catch(err => {
		console.error(err);
	});

------------------------------------------------------------------------------

-----------
trustSet
-----------
.. code-block:: javascript

	chainsql.trustSet(amount)

信任网关，参数指定信任某个网关的某货币数量。从而可以交易该货币。为交易类型，需要调用submit提交交易。

参数说明
-----------

1. ``amount`` - ``JsonObject`` : Json对象，主要针对网关发行的非系统币，包含以下三个字段：

		* ``value`` - ``String`` : 转账数额；
		* ``currency`` - ``String`` : 转账币种；
		* ``issuer`` - ``String`` : 该币种的发行网关地址。

返回值
-----------

``JsonObject`` : 返回chainsql对象本身。

示例
-----------
.. code-block:: javascript

	const amount = {
        value: 20000,
        currency: "TES",
        issuer: "zxm9ptsc1H18Unmc9cThiRwLAYikat2Gp8"
    }

    chainsql.trustSet(amount).submit({ expect: 'validate_success' }).then(res => {
		console.log(res);
	}).catch(err => {
		console.error(err);
	});

------------------------------------------------------------------------------

---------------------
pay（网关发行币转账）
---------------------

网关发行货币转账和普通转账接口相同，只是在amount参数处有所不同，具体格式和用法可以参考 :ref:`pay <pay-introduce>`


表交易
===========

------------
createTable
------------
.. code-block:: javascript

	chainsql.createTable(tableName, raw[, option])

创建数据库表接口，交易发起者同时也为表的拥有者，即as接口指定的账户，此操作为交易类型，需要通过调用submit将该交易提交。

参数说明
-----------

1. ``tableName`` - ``String`` : 新建表的表名
2. ``raw`` - ``Array`` : 对新建表的属性设定，详细格式及内容可参看  :ref:`建表raw字段说明 <create-table>`
3. ``option`` - ``JsonObject`` : [**可选**]是否创建加密表及是否开启行及控制，字段如下：

	* ``confidential`` - ``Boolean`` : [**可选**]是否创建加密表，不传该值或者为false时，创建非加密表；
	* ``operationRule`` - ``JsonObject`` : [**可选**]行及控制规则，格式及内容可参看 :ref:`行级控制规则 <recordLevel>` 。

返回值
-----------

``JsonObject`` : 返回chainsql对象本身。

示例
-----------
.. code-block:: javascript

	const raw = [
		{'field':'id','type':'int','length':11,'PK':1,'NN':1},
		{'field':'name','type':'varchar','length':50,'default':""},
		{'field':'age','type':'int'}
	];
	const option = {
		confidential: true
	}

	chainsql.createTable("tableTest", raw, option).submit({expect:'db_success'}).then(res => {
		console.log(res);
	}).catch(err => {
		console.error(err);
	});

------------------------------------------------------------------------------

------------
renameTable
------------
.. code-block:: javascript

	chainsql.renameTable(oldName, newName)

对已创建表进行重命名操作。为交易类型，需要调用submit提交交易。

参数说明
-----------

1. ``oldName`` - ``String`` : 表当前使用的名字；
2. ``newName`` - ``String`` : 新表名。

返回值
-----------

``JsonObject`` : 返回chainsql对象本身。

示例
-----------
.. code-block:: javascript

	const oldName = "tableTest";
	const newName = "chainsqlTable";
	chainsql.renameTable(oldName, newName).submit({expect:'db_success'}).then(res => {
		console.log(res);
	}).catch(err => {
		console.error(err);
	});

------------------------------------------------------------------------------

-----------
dropTable
-----------
.. code-block:: javascript

	chainsql.dropTable(tableName)

执行删除表操作。为交易类型，需要调用submit提交交易。

参数说明
-----------

1. ``tableName`` - ``String`` : 欲删除表的表名。

返回值
-----------

``JsonObject`` : 返回chainsql对象本身。

示例
-----------
.. code-block:: javascript

	const tableName = "chainsqlTable";
	chainsql.dropTable(tableName).submit({expect:'db_success'}).then(res => {
		console.log(res);
	}).catch(err => {
		console.error(err);
	});

------------------------------------------------------------------------------

-----------
table
-----------
.. code-block:: javascript

	chainsql.table(tableName)

构造表对象，用于对表的增删改查操作。比如insert操作，需要调用table接口获取table对象，然后调用insert接口。

参数说明
-----------

1. ``tableName`` - ``String`` : 表名。

返回值
-----------

``JsonObject`` : 创建的对应表名的表对象tableObj，用于调用表的增删改查操作。

示例
-----------
.. code-block:: javascript

	const tableName = "tableTest";
	const raw = [
		{'id':1,'age': 111,'name':'rrr'}
	];
	chainsql.table(tableName).insert(raw).submit({expect:'db_success'}).then(res => {
		console.log(res);
	}).catch(err => {
		console.error(err);
	});

------------------------------------------------------------------------------

-----------
insert
-----------
.. code-block:: javascript

	tableObj.insert(raw[, feild])

| 对表进行插入操作，tableObj是由chainsql.table接口创建的。
| 此接口为交易类型，单独使用需要调用submit接口，事务中不需要。

参数说明
-----------

1. ``raw`` - ``Array`` : 插入操作的raw，详细格式和内容可参看 :ref:`插入raw字段说明 <insert-table>` ；
2. ``field`` - ``String`` : 插入操作支持将每次执行插入交易的哈希值作为字段同步插入到数据库中。需要提前在建表的时候指定一个字段为存储交易哈希，然后将该字段名作为参数传递给insert即可。

返回值
-----------

``JsonObject`` : 返回tableObj对象本身。

示例
-----------
.. code-block:: javascript

	const tableName = "tableTest";
	const raw = [
		{'id':1,'age': 111,'name':'rrr'}
	];
	chainsql.table(tableName).insert(raw).submit({expect:'db_success'}).then(res => {
		console.log(res);
	}).catch(err => {
		console.error(err);
	});

------------------------------------------------------------------------------

-----------
update
-----------
.. code-block:: javascript

	tableObj.update(raw)

| 对表内容进行更新操作，tableObj是由chainsql.table接口创建的。
| 此接口为交易类型，单独使用需要调用submit接口，事务中不需要。

.. warning::
	更新之前需要调用tableObj.get接口指定待修改内容，且必须指定，不能为空。

参数说明
-----------

1. ``raw`` - ``Array`` : 插入操作的raw，详细格式和内容可参看 :ref:`更新raw字段说明 <update-table>` 。

返回值
-----------

``JsonObject`` : 返回tableObj对象本身。

示例
-----------
.. code-block:: javascript

	const tableName = "tableTest";
	chainsql.table(tableName).get({'id': 1}).update({'age':200}).submit({expect:'db_success'}).then(res => {
		console.log(res);
	}).catch(err => {
		console.error(err);
	});

------------------------------------------------------------------------------

-----------
delete
-----------
.. code-block:: javascript

	tableObj.delete()

| 对表内容进行删除操作，tableObj是由chainsql.table接口创建的。
| 此接口为交易类型，单独使用需要调用submit接口，事务中不需要。

.. warning::
	更新之前需要调用tableObj.get接口指定待删除内容，且必须指定，不能为空。即不可删除表的所有内容。

参数说明
-----------

None

返回值
-----------

``JsonObject`` : 返回tableObj对象本身。

示例
-----------
.. code-block:: javascript

	const tableName = "tableTest";
	chainsql.table(tableName).get({'id': 5}).delete().submit({expect:'db_success'}).then(res => {
		console.log(res);
	}).catch(err => {
		console.error(err);
	});

------------------------------------------------------------------------------

-----------
beginTran
-----------
.. code-block:: javascript

	chainsql.beginTran()

开启事务操作。必须在事务操作的开始执行。

参数说明
-----------

None

返回值
-----------

None

示例
-----------
.. code-block:: javascript

	chainsql.beginTran();

------------------------------------------------------------------------------

-----------
commit
-----------
.. code-block:: javascript

	chainsql.commit([param])

| 事务完成的提交操作，在执行完beginTran()操作之后，就可以开始执行数据库操作，执行方法与普通数据库接口的操作一致，只是不用调用submit接口，而是最后统一调用commit接口进行提交，详细可参考下述示例。
| 事务目前只支持授权grant、插入insert、删除delete、更新update、断言assert五个接口。

参数说明
-----------

1. ``param`` : 同submit参数，可参考 :ref:`submit参数说明 <submit-param>`

返回值
-----------

同submit接口， 可参考 :ref:`submit返回值 <submit-return>`

示例
-----------
.. code-block:: javascript

	const tableName = "tableTest";

	chainsql.beginTran();
	chainsql.table(tableName).insert({ 'id':2, 'age': 222, 'name': 'id2' });
	chainsql.table(tableName).insert({ 'id':3, 'age': 555, 'name': 'id3' });
	chainsql.table(tableName).get({'id': 3}).update({'age':333})
	chainsql.table(tableName).get({ 'age': 333 }).update({ 'name': 'world' });
	chainsql.commit({expect: 'db_success'}).then(res => {
		console.log(res);
	}).catch(err => {
		console.error(err);
	});

------------------------------------------------------------------------------

-----------
grant
-----------
.. code-block:: javascript

	chainsql.grant(tableName, userAddr, raw[, userPub])

ChainSQL数据库表权限管理，控制自己创建的表被其他用户访问的权限，权限包括增删改查四种。为交易类型，需要调用submit提交交易。

参数说明
-----------

1. ``tableName`` - ``String`` : 准备授权给user的表名；
2. ``userAddr`` - ``String`` : 被授权用户地址；对新建表的属性设定，详细格式及内容可参看 :ref:`授权raw字段说明 <grant-table>`
3. ``raw`` - ``JsonObject`` : 指定这次给user授予的权限，key为增删改查，value为True或者False，希望授予的操作置位True，否则为False；
4. ``userPub`` - ``String`` : [**可选**]针对加密表，必须添加user的公钥，格式为base58编码的。非加密表不需要填此参数。

返回值
-----------

``JsonObject`` : 返回chainsql对象本身。

示例
-----------
.. code-block:: javascript

	const tableName = "tableTest";
	const userAddr = "zxm9ptsc1H18Unmc9cThiRwLAYikat2Gp8";
	
	const raw = {select:true, insert:true, update:false, delete:false};
	chainsql.grant(tableName, userAddr, raw).submit({expect:'db_success'}).then(res => {
		console.log(res);
	}).catch(err => {
		console.error(err);
	});

------------------------------------------------------------------------------

------------
setRestrict
------------
.. code-block:: javascript

	chainsql.setRestrict(mode)

设置ChainSQL严格模式的开关。

参数说明
-----------

1. ``mode`` - ``Boolean`` : 是否开启ChainSQL严格模式，true则开启，false则关闭。新建chainsql对象之后是默认关闭的。

返回值
-----------

None

示例
-----------
.. code-block:: javascript

	chainsql.setRestrict(true);

------------------------------------------------------------------------------

--------------
setNeedVerify
--------------
.. code-block:: javascript

	chainsql.setNeedVerify(isNeed)

设置ChainSQL是否开启数据库验证的开关，用于事务交易，在事务交易中对数据库操作在共识过程中进行实际数据库操作验证。

参数说明
-----------

1. ``isNeed`` - ``Boolean`` : 是否开启ChainSQL数据库验证的开关，true则开启，false则关闭。新建chainsql对象之后是默认开启的。

返回值
-----------

None

示例
-----------
.. code-block:: javascript

	chainsql.setNeedVerify(true);

------------------------------------------------------------------------------


表查询
===========

.. _get-API:

-----------
get
-----------
.. code-block:: javascript

	tableObj.get([raw])

| 对表内容进行查询操作，tableObj是由chainsql.table接口创建的。
| 此接口不属于交易类型，但是为了检查用户是否有查询权限，需要签名之后调用，因此需要调用submit接口。
| 此外ChainSQL还提供了三个查询限定接口，主要包括limit、order、withField。在对应接口下查看具体说明。

.. note::
	现在查询无论数据库有多少内容，get接口一次最多返回200条结果，如果数据较多，可以结合 :ref:`limit接口 <limit-API>` 做分页查询。

参数说明
-----------

1. ``raw`` - ``JsonObject`` : [**可选**]raw参数的详细格式及内容可参看 :ref:`查询raw字段说明 <查询Raw详解>` ，需要查询所有内容时，不传raw参数即可。

返回值
-----------

``JsonObject`` : 返回tableObj对象本身。

.. _get-return:

.. note::

	通过get接口调用submit提交查询请求之后，实际返回查询结果为一个 ``JsonObject`` ，具体包含字段如下：

	* ``diff`` - ``Number`` : 数据库内容与区块链上是否是最新的，当链上最新区块与数据库所同步到的区块差值小于3，则返回0，认为可接受；差值小于3会返回差值，提示数据库内容可能不是最新的；
	* ``lines`` - ``Array`` : 查询结果组成的数组，每个元素为数据库中的一行，类型为 ``JsonObject`` ，key为字段名，value为字段值。

示例
-----------
.. code-block:: javascript

	const tableName = "tableTest";
	const raw = {
		$and: [{id: {$gt: 1}}, {id: {$lt: 100}}]
	};
	chainsql.table(tableName).get(raw).submit().then(res => {
		console.log(res);
	}).catch(err => {
		console.error(err);
	});

.. _limit-API:
-----------
limit
-----------
.. code-block:: javascript

	tableObj.limit(option)

| 对表内容进行查询操作的限定条件，tableObj是由chainsql.table接口创建的。
| 对数据库进行分页查询.返回对应条件的数据。
| 此接口单独使用没有意义，需要在get接口之后级联调用。并可以与order和withFields依次级联，但最后需要调用submit接口。

参数说明
-----------

1. ``option`` - ``JsonObject`` : 主要包括起始位置和总条数：

	* ``index`` - ``Number`` : 起始位置；
	* ``total`` - ``Number`` : 返回总条数。

返回值
-----------

``JsonObject`` : 返回tableObj对象本身。

示例
-----------
.. code-block:: javascript

	const tableName = "tableTest";
	const raw = [{'age': 16}];
	chainsql.table(tableName).get(raw).limit({index:0,total:10}).submit().then(res => {
		console.log(res);
	}).catch(err => {
		console.error(err);
	});

-----------
order
-----------
.. code-block:: javascript

	tableObj.order(option1[, option2[, ...]])

| 对表内容进行查询操作的限定条件，tableObj是由chainsql.table接口创建的。
| 对查询的数据按指定字段进行排序；
| 此接口单独使用没有意义，需要在get接口之后级联调用。并可以与limit和withFields依次级联，但最后需要调用submit接口。

参数说明
-----------

1. ``option`` - ``JsonObject`` : 可以添加对多个字段的升降序控制，每个JsonObject的key为字段名，value为1表示升序，-1表示降序。

返回值
-----------

``JsonObject`` : 返回tableObj对象本身。

示例
-----------
.. code-block:: javascript

	const tableName = "tableTest";
	const raw = [{'id': 1}];
	chainsql.table(tableName).get(raw).order({id:1}, {name:-1}).submit().then(res => {
		console.log(res);
	}).catch(err => {
		console.error(err);
	});

-----------
withFields
-----------
.. code-block:: javascript

	tableObj.withFields(feildArray)

| 对表内容进行查询操作的限定条件，tableObj是由chainsql.table接口创建的。
| 从数据库查询数据,通过参数指定返回字段。
| 此接口单独使用没有意义，需要在get接口之后级联调用。并可以与limit和order依次级联，但最后需要调用submit接口。

参数说明
-----------

1. ``feildArray`` - ``Array`` : 由限定的返回字段组成的数组，每个数组元素为 ``String`` 格式。数组元素可以是字段名，也可以是其他SQL语句，SQL作为参数可参考 :ref:`字段统计 <withField-introduce>` 。

返回值
-----------

``JsonObject`` : 返回tableObj对象本身。

示例
-----------
.. code-block:: javascript

	//查询表中记录数
	const tableName = "tableTest";
	const raw = [{'id': 1}];
	chainsql.table(tableName).get(raw).withFields(["COUNT(*) as count"]).submit().then(res => {
		console.log(res);
	}).catch(err => {
		console.error(err);
	});

------------------------------------------------------------------------------

--------------
getBySqlAdmin
--------------
.. code-block:: javascript

	chainsql.getBySqlAdmin(sql)

直接传入SQL语句进行数据库查询操作，因为直接操作数据库中的表，所有需要配合getTableNameInDB接口获取表在数据库中的真实表名。

.. note::

	* 本接口不做表权限判断，但是只能由节点配置文件中 **[port_ws_admin_local]** 的admin里所配置的ip才可以发起此接口的调用，否则调用失败。
	* 即nodejs接口运行所在的主机ip必须是配置在配置文件的 **[port_ws_admin_local]** 的admin里。
	* 因为在配置文件中配置为admin的ip认为是该节点的管理员。拥有对本节点已经同步表的查询权限。

参数说明
-----------

1. ``sql`` - ``String`` : SQL语句，用于查询

返回值
-----------

``JsonObject`` : 返回查询结果，格式与调用get接口之后在submit中获取的结果格式一致，但是没有diff字段。具体可参考 :ref:`get接口 <get-return>`

示例
-----------
.. code-block:: javascript

	const tableOwnerAddr = "zMpUjXckTSn6NacRq2rcGPbjADHBCPsURF";
	const tableName = "tabelTest";
	let tableNameInDB = await chainsql.getTableNameInDB(tableOwnerAddr, tableName);
	let tableName = "t_" + tableNameInDB;

	chainsql.getBySqlAdmin("select * from " + tableName).then(res => {
		console.log(res);
	}).catch(err => {
		console.error(err);
	});

------------------------------------------------------------------------------

-------------
getBySqlUser
-------------
.. code-block:: javascript

	chainsql.getBySqlUser(sql)

直接传入SQL语句进行数据库查询操作，因为直接操作数据库中的表，所有需要配合getTableNameInDB接口获取表在数据库中的真实表名。

.. note::
	* 调用此接口前as接口中设置的用户，对SQL语句中的表有查询权限，才可以调用此接口。

参数说明
-----------

1. ``sql`` - ``String`` : SQL语句，用于查询

返回值
-----------

``JsonObject`` : 返回查询结果，格式与调用get接口之后在submit中获取的结果格式一致，但是没有diff字段。可参考 :ref:`get接口 <get-return>`

示例
-----------
.. code-block:: javascript

	const user = {
		secret: "xx9rPirSmNSG6SDCsyDi8Du6x7qKc",
		address: "zxm9ptsc1H18Unmc9cThiRwLAYikat2Gp8",
		publicKey: "cBQh8rTr1FuQL97rMdtPZUhAPEN9airXqGwTw5bzrTVtcHMsjaLo"
	};
	chainsql.as(user);

	const tableOwnerAddr = "zMpUjXckTSn6NacRq2rcGPbjADHBCPsURF";
	const tableName = "tabelTest";
	let tableNameInDB = await chainsql.getTableNameInDB(tableOwnerAddr, tableName);
	let tableName = "t_" + tableNameInDB;

	chainsql.getBySqlUser("select * from " + tableName).then(res => {
		console.log(res);
	}).catch(err => {
		console.error(err);
	});

------------------------------------------------------------------------------


订阅
===========

---------------
subscribeTable
---------------
.. code-block:: javascript

	chainsql.event.subscribeTable(owner, tableName, callback)

| 订阅表操作的事件，提供表名和表的拥有者，就可以订阅此表。
| 普通表不用授权即可订阅。对于加密表需要被授权，即被grant之后，才可以看到交易明文，否则看到数据库操作部分为密文。

参数说明
-----------

1. ``owner`` - ``String`` : 被订阅的表拥有者地址；
2. ``tableName`` - ``String`` : 被订阅的数据库表名；
3. ``callback`` - ``Function`` : 回调函数，为必填项，需用通过此函数接收后续订阅结果。

返回值
-----------

``Promise`` : 返回一个promise对象，指示是否订阅成功。

示例
-----------
.. code-block:: javascript

	const tableOwnerAddr = "zMpUjXckTSn6NacRq2rcGPbjADHBCPsURF";
	const tableName = "tabelTest";
	chainsql.event.subscribeTable(tableOwnerAddr, tableName, function(err, data) {
		err ? console.error(err) : console.log(data)
	}).then(res => {
		console.log("subTable success.");
	}).catch(error => {
		console.error("subTable error:" + error);
	});

------------------------------------------------------------------------------

----------------
unsubcribeTable
----------------
.. code-block:: javascript

	chainsql.event.unsubcribeTable(owner, tableName)

取消对表的订阅。

参数说明
-----------

1. ``owner`` - ``String`` : 被订阅的表拥有者地址；
2. ``tableName`` - ``String`` : 被订阅的数据库表名；

返回值
-----------

``Promise`` : 返回一个promise对象，指示是否取消订阅成功。

示例
-----------
.. code-block:: javascript

	chainsql.event.unsubscribeTable(owner, tb).then(res => {
		console.log("unsubTable success.");
	}).catch(error => {
		console.error("unsubTable error:" + error);
	});

------------------------------------------------------------------------------

-----------
subscribeTx
-----------
.. code-block:: javascript

	chainsql.event.subscribeTx(txHash, callback)

订阅交易事件，提供交易的哈希值，就可以订阅此交易。

参数说明
-----------

1. ``txHash`` - ``String`` : 被订阅的交易哈希值；
2. ``callback`` - ``Function`` : 回调函数，为必填项，需用通过此函数接收后续订阅结果。

返回值
-----------

``Promise`` : 返回一个promise对象，指示是否订阅成功。

示例
-----------
.. code-block:: javascript

	let txHash = "02BAE87F1996E3A23690A5BB7F0503BF71CCBA68F79805831B42ABAD5913BA86";
	chainsql.event.subscribeTx(txHash, function(err, data) {
		err ? console.error(err) : console.log(data)
	}).then(res => {
		console.log("subTx success.");
	}).catch(error => {
		console.error("subTx error:" + error);
	});

------------------------------------------------------------------------------

--------------
unsubscribeTx
--------------
.. code-block:: javascript

	chainsql.event.unsubscribeTx(txHash, callback)

取消对交易的订阅。

参数说明
-----------

1. ``txHash`` - ``String`` : 被订阅的交易哈希值；

返回值
-----------

``Promise`` : 返回一个promise对象，指示是否订阅成功。

示例
-----------
.. code-block:: javascript

	let txHash = "02BAE87F1996E3A23690A5BB7F0503BF71CCBA68F79805831B42ABAD5913BA86";
	event.unsubscribeTx(txHash).then(res => {
		console.log("unsubTx success.");
	}).catch(error => {
		console.error("unsubTx error:" + error);
	});	

------------------------------------------------------------------------------


智能合约接口
============

.. toctree::
   :maxdepth: 2

   nodejsSmartContract