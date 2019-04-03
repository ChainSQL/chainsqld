环境要求：
============
Jdk1.8

调用流程
==============
#.	使用智能合约编译工具(solc或remix)编译合约脚本得到bin与abi
#.	调用codegen_chainsql工具生成java类
#.	将Java类导入到工程中，进行合约部署/调用

合约示例
==============

::

	pragma solidity ^0.4.2;

	// Modified Greeter contract. Based on example at https://www.ethereum.org/greeter.

	contract mortal {
		/* Define variable owner of the type address*/
		address owner;

		/* this function is executed at initialization and sets the owner of the contract */
		constructor() public { owner = msg.sender; }

		/* Function to recover the funds on the contract */
		function kill() public { if (msg.sender == owner) selfdestruct(owner); }
	}

	contract greeter is mortal {
		/* define variable greeting of the type string */
		string greeting;

		/* this runs when the contract is executed */
		constructor(string _greeting) public payable{
			greeting = _greeting;
		}

		function newGreeting(string _greeting) public {
			emit Modified(greeting, _greeting, greeting, _greeting);
			greeting = _greeting;
		}

		/* main function */
		function greet() public view returns (string) {
			return greeting;
		}

		/* we include indexed events to demonstrate the difference that can be
		captured versus non-indexed */
		event Modified(
				string indexed oldGreetingIdx, string indexed newGreetingIdx,
				string oldGreeting, string newGreeting);
	}


使用codegen_chainsql 工具生成java类
==========================================
codegen_chainsql.jar 是一个智能合约编译得到的bin与abi数据转化为可调用的java类的可执行jar包，获取链接如下：
codegen_chainsql.jar_

.. _codegen_chainsql.jar: https://github.com/ChainSQL/java-chainsql-api/tree/feature/contract/codegen/codegen_chainsql.jar

调用格式
----------------

.. code-block:: bash

	java -jar codegen_chainsql.jar <input binary file>.bin <input abi file>.abi 
		-p|--package <base package name> -o|--output <destination base directory>

:参数说明:

==========  =================================
  Param     Explanation
==========  =================================
.bin  		编译得到的字节码文件(.bin)路径      
.abi   		编译得到的abi 文件路径		
–p  		指定包名
–o   		生成的java文件存储位置   
==========  =================================

示例，合约Greeter.sol经编译后生成Greeter.bin,Greeter.abi文件，通过codegen_chainsql工具生成Greeter.java 文件到 e:/solc 路径下，并且包名为com.peersafe.example::

	java -jar codegen_chainsql.jar ./Greeter.bin ./Greeter.abi -p com.peersafe.example -o e:/solc


合约部署及调用
============================
生成合约对应的java类后，就可以进行调用了，首先，需要初始化chainsql对象：

1. 初始化chainsql对象：
---------------------------------------------

::

	Chainsql c = Chainsql.c;
	c.connect("ws://127.0.0.1:6007");
	c.as(rootAddress, rootSecret);


然后，可用同步/异步两种方式进行合约部署:

2. 合约部署
------------------------------
同步

.. code-block:: java

	//同步调用,共识通过返回
	Greeter contract = Greeter.deploy(c,Contract.GAS_LIMIT,Contract.INITIAL_DROPS,"Hello blockchain world!").send();
	//输出合约地址
	System.out.println("Smart contract deployed to address " + contract.getContractAddress());

异步

.. code-block:: java

	Greeter.deploy(c, Contract.GAS_LIMIT, Contract.INITIAL_DROPS, "hello world",new Callback<Greeter>() {
		@Override
		public void called(Greeter args) {
			String contractAddress = args.getContractAddress();
			System.out.println("Smart contract deployed to address " + contractAddress);
		}
	});

deploy方法说明：
以Greeter合约为例，合约 ``(solidity)`` 的构造函数为

.. code-block:: java

    //this runs when the contract is executed
    constructor(string _greeting) public payable{
        greeting = _greeting;
    }

对应的deploy方法声明为：

.. code-block:: java

	/**
	合约部署方法，共识成功后返回
	@param chainsql			Chainsql对象
	@param gasLimit			用户提供的Gas上限
	@param initialDropsValue	(可选)当合约构造函数被 payable 关键字修饰时，可在合约部署时给合约打系统币（单位为drop)
	@param greeting			合约构造函数参数，当参数有多个时，依次向后排列
	@return 			合约对象
	*/
	public static Greeter deploy(Chainsql chainsql, 
				   	BigInteger gasLimit, 
				   	BigInteger initialDropsValue, 
				   	String greeting);

3.合约调用
--------------------
当合约部署成功后，可调用合约中的其它方法，除了deploy方法可得到合约对象外，通常我们通过load方法，初始化合约对象：

.. code-block:: java
	
	//合约加载
	Greeter contract = Greeter.load(c,"zKotgrRHyoc7dywd7vf6LgFBXnv3K66rEg", Contract.GAS_LIMIT);

load方法的声明：

.. code-block:: java
 
	/** 
	加载合约对象
	@param chainsql		Chainsql对象
	@param contractAddress	合约地址
	@param gasLimit		用户提供的Gas上限
	@return 		合约对象
	*/
 	public static Greeter load(Chainsql chainsql, String contractAddress, BigInteger gasLimit)

``````````````````````````````````````````
3.1 改变合约状态的调用（交易）
``````````````````````````````````````````
合约 ``(solidity)`` 方法定义：

::

	function newGreeting(string _greeting) public {
		emit Modified(greeting, _greeting, greeting, _greeting);
		greeting = _greeting;
	}

同步调用：

.. code-block:: java
	
	JSONObject ret = contract.newGreeting("Well hello again3").submit(SyncCond.validate_success);
	System.out.println(ret);


异步调用：

.. code-block:: java

	//set，Lets modify the value in our smart contract
	contract.newGreeting("Well hello again3").submit(new Callback<JSONObject>() {
		@Override
		public void called(JSONObject args) {
			System.out.println(args);
		}
	});

``````````````````````````````````````
3.2 不改变合约状态的调用
``````````````````````````````````````
合约方法定义：

::

	function greet() public view returns (string) {
		return greeting;
	}

同步：

.. code-block:: java
	
	//get
	System.out.println("Value stored in remote smart contract: " + contract.greet());

异步：

.. code-block:: java

	greet.greet(new Callback<String>() {
		@Override
		public void called(String args) {
			// TODO Auto-generated method stub
		}
	});

4. 事件监控
---------------------------
这里只能实时监控正在触发的事件，不能根据indexed的值去过滤之前的log</p>
合约 ``(solidity)`` 方法定义：

::

	/* we include indexed events to demonstrate the difference that can be
	captured versus non-indexed */
	event Modified(
			string indexed oldGreetingIdx, string indexed newGreetingIdx,
			string oldGreeting, string newGreeting);

对应的调用为：

.. code-block:: java

	contract.onModifiedEvents(new Callback<Greeter.ModifiedEventResponse>() {
		@Override
		public void called(ModifiedEventResponse event) {
			//todo
			System.out.println("Modify event fired, previous value: " + event.oldGreeting + ", new value: "
					+ event.newGreeting);
			System.out.println("Indexed event previous value: " + Numeric.toHexString(event.oldGreetingIdx)
					+ ", new value: " + Numeric.toHexString(event.newGreetingIdx));
		}
	});
	
5.通过fallback函数给合约打钱
-----------------------------------------------
在solidity中的可以定义一个fallback函数，它没有名字，没有参数，也没有返回值，且必须被 external 关键字修饰，这个方法一般用来在合约未定义转账方法的情况下给合约转账，前提是它必须被 payable 关键字修饰。

::

	function() external payable { }

Chainsql中通过fallback函数给合约转账的接口为payToContract:

.. code-block:: java
	
	/**
	通过fallback函数给合约转账系统币
	@param contractAddress		合约地址
	@param value			要转移的ZXC数量，可包含小数
	@param gasLimit			用户提供的Gas上限
	@return 			Ripple对象，可调用submit方法提交交易
	*/
	Ripple payToContract(String contract_address, String value, int gasLimit)

6.合约中调用新增的表相关指令
----------------------------------
`合约示例 <https://github.com/ChainSQL/java-chainsql-api/blob/feature/contract/chainsql/src/main/resources/solidity/table/solidity-TableTxs.sol>`_ 

`对应的java类文件 <https://github.com/ChainSQL/java-chainsql-api/tree/feature/contract/chainsql/src/test/java/com/peersafe/example/contract/DBTest.java>`_ 

`测试用例 <https://github.com/ChainSQL/java-chainsql-api/blob/feature/contract/chainsql/src/test/java/com/peersafe/example/contract/TestContractTableTxs.java>`_

.. note:: 调用表相关操作时，submit参数可以传SyncCond.db_success，入库成功后返回
