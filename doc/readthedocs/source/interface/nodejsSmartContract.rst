.. _remix.chainsql.net: http://remix.chainsql.net

在chainsql的nodejs接口中，我们提供了chainsql.contract对象，通过创建该对象，就可以开始与chainsql智能合约的交互了。

.. _contract-newObj:

创建合约对象
============
.. code-block:: javascript

	chainsql.contract(abi[, contractAddr]);

创建一个包含了所有在abi中定义的函数和事件的新合约对象。chainsql是一个chainsql-nodejs的对象。

参数说明
--------

1. ``abi`` - ``Array`` : 一个由Json对象组成的数组，每个Json对象都是solidity的函数或者事件,可通过 `remix.chainsql.net`_ 获取；
2. ``contractAddr`` - ``String`` : [**可选**]合约地址，在初次创建合约对象时，没有合约地址，可不填；合约已经存在，可直接利用合约地址，创建一个包含该合约函数和事件的合约对象。


返回值
--------

``Object`` : 包含合约函数和事件的智能合约对象。

示例
--------
.. code-block:: javascript

    //first create a contract object without contract address.
    const abi = '[{"constant":false,"inputs":[{"name":"to","type":"address"},{"name":"amount","type":"uint256"}],"name":"transferToUser","outputs":[],"payable":true,"stateMutability":"payable","type":"function"},{"constant":true,"inputs":[],"name":"returnMixType","outputs":[{"name":"","type":"uint256"},{"name":"","type":"string"}],"payable":false,"stateMutability":"pure","type":"function"},{"constant":false,"inputs":[{"name":"newMem","type":"uint256"}],"name":"setMem","outputs":[],"payable":false,"stateMutability":"nonpayable","type":"function"},{"constant":true,"inputs":[],"name":"returnString","outputs":[{"name":"","type":"string"}],"payable":false,"stateMutability":"pure","type":"function"},{"constant":true,"inputs":[],"name":"getMsgSender","outputs":[{"name":"","type":"address"},{"name":"","type":"uint256"}],"payable":false,"stateMutability":"view","type":"function"},{"constant":true,"inputs":[],"name":"getTxOrigin","outputs":[{"name":"","type":"address"}],"payable":false,"stateMutability":"view","type":"function"},{"constant":false,"inputs":[{"name":"a","type":"uint256"}],"name":"multiply","outputs":[{"name":"d","type":"uint256"}],"payable":false,"stateMutability":"nonpayable","type":"function"},{"constant":false,"inputs":[{"name":"to","type":"address"}],"name":"userTransferUser","outputs":[],"payable":true,"stateMutability":"payable","type":"function"},{"constant":true,"inputs":[],"name":"getMem","outputs":[{"name":"","type":"uint256"}],"payable":false,"stateMutability":"view","type":"function"},{"constant":true,"inputs":[{"name":"user","type":"address"}],"name":"getBalance","outputs":[{"name":"","type":"uint256"}],"payable":false,"stateMutability":"view","type":"function"},{"inputs":[],"payable":true,"stateMutability":"payable","type":"constructor"},{"payable":true,"stateMutability":"payable","type":"fallback"},{"anonymous":false,"inputs":[{"indexed":false,"name":"sender","type":"address"},{"indexed":true,"name":"number","type":"uint256"},{"indexed":false,"name":"result","type":"uint256"}],"name":"multiplylog","type":"event"}]';
    const contractObj = chainsql.contract(JSON.parse(abi));

    //create a contract object with contract address already in chainsql blockchain.
    const abi = '[{"constant":false,"inputs":[{"name":"to","type":"address"},{"name":"amount","type":"uint256"}],"name":"transferToUser","outputs":[],"payable":true,"stateMutability":"payable","type":"function"},{"constant":true,"inputs":[],"name":"returnMixType","outputs":[{"name":"","type":"uint256"},{"name":"","type":"string"}],"payable":false,"stateMutability":"pure","type":"function"},{"constant":false,"inputs":[{"name":"newMem","type":"uint256"}],"name":"setMem","outputs":[],"payable":false,"stateMutability":"nonpayable","type":"function"},{"constant":true,"inputs":[],"name":"returnString","outputs":[{"name":"","type":"string"}],"payable":false,"stateMutability":"pure","type":"function"},{"constant":true,"inputs":[],"name":"getMsgSender","outputs":[{"name":"","type":"address"},{"name":"","type":"uint256"}],"payable":false,"stateMutability":"view","type":"function"},{"constant":true,"inputs":[],"name":"getTxOrigin","outputs":[{"name":"","type":"address"}],"payable":false,"stateMutability":"view","type":"function"},{"constant":false,"inputs":[{"name":"a","type":"uint256"}],"name":"multiply","outputs":[{"name":"d","type":"uint256"}],"payable":false,"stateMutability":"nonpayable","type":"function"},{"constant":false,"inputs":[{"name":"to","type":"address"}],"name":"userTransferUser","outputs":[],"payable":true,"stateMutability":"payable","type":"function"},{"constant":true,"inputs":[],"name":"getMem","outputs":[{"name":"","type":"uint256"}],"payable":false,"stateMutability":"view","type":"function"},{"constant":true,"inputs":[{"name":"user","type":"address"}],"name":"getBalance","outputs":[{"name":"","type":"uint256"}],"payable":false,"stateMutability":"view","type":"function"},{"inputs":[],"payable":true,"stateMutability":"payable","type":"constructor"},{"payable":true,"stateMutability":"payable","type":"fallback"},{"anonymous":false,"inputs":[{"indexed":false,"name":"sender","type":"address"},{"indexed":true,"name":"number","type":"uint256"},{"indexed":false,"name":"result","type":"uint256"}],"name":"multiplylog","type":"event"}]';
    const contractObj = chainsql.contract(JSON.parse(abi), "z9cT6mSw2QG...YGD5MUpHwk");

------------------------------------------------------------------------------

.. _contract-deploy:

合约部署
=========
.. code-block:: javascript

    contractObj.deploy(paramsNeeded).submit(options[, callback]);

通过deploy函数将合约部署到链上，返回结果可以通过指定callback函数或者直接使用then和catch接收promise的resolve和reject。

参数说明
--------

1. ``paramsNeeded`` - ``JsonObject`` : 由以下两个必选字段组成：

    * ``ContractData`` - ``String`` : 智能合约字节码，以0x开头。可通过 `remix.chainsql.net`_ 获取；
    * ``arguments`` - ``Array``: 智能合约构造函数的参数作为元素组成的数组，每个元素根据类型填写元素值，没有参数提供一个空数组[]。

2. ``options`` - ``JsonObject`` : 由以下两个字段组成：

    * ``ContractValue`` - ``String`` : [**可选**]如果合约构造函数为payable类型，则可使用该字段给合约转账，单位为drop；
    * ``Gas`` - ``Number`` : 执行合约部署所需要的手续费用。

3. ``callback`` - ``Function`` : [**可选**]回调函数，如果指定，则通过指定回调函数返回结果，否则需要通过then和catch接收promise结果。

返回值
--------

``JsonObject`` - 智能合约的部署成功或失败的结果。返回方式取决于是否指定回调函数。

1. 部署成功 - 指定回调函数，则通过回调函数返回，否则返回返回一个resolve的Promise对象。 ``JsonObject`` 包含以下字段：

	* ``status`` - ``String`` : 表示部署状态，成功则为固定值:"validate_success"；
	* ``tx_hash`` - ``String`` : 部署合约的交易hash值；
	* ``contractAddress`` - ``String`` : 部署成功的合约地址。
	
2. 部署失败 - 指定回调函数，则通过回调函数返回，否则返回返回一个reject的Promise对象。 ``JsonObject`` 内容依具体错误形式返回。

示例
--------
.. code-block:: javascript

    // use the callback
    const deployBytecode = '0x...';
    contractObj.deploy({
        ContractData : deployBytecode,
        arguments : [666]
    }).submit({
        Gas : 4000000000
    }, function (err, res) {
        err ? console.log(err) : console.log(res);
    })
    > res
    {
        status:"validate_success"
        tx_hash:"DD443076A8A4B02B6661261CCD456F2DC7F4031F12EC38EAD35E821782328318"
        contractAddress:"zPqMARn53PpN2fu8eScac4cEYW6b4w8ZH"
    }


    // use the promise
    const deployBytecode = '0x...';
    contractObj.deploy({
        ContractData : deployBytecode,
        arguments : [666]
    }).submit({
        Gas : 4000000000
    }).then(res => {
        console.log(res);
    }).catch(err => {
        console.error(err);
    })

------------------------------------------------------------------------------

.. _contract-submit:

更改合约内部状态调用
====================
.. code-block:: javascript

    contractObj.method.function([params1[, params2, ...]]]).submit(options[, callback])

这种调用方式实际是以交易的形式发送到chainsql链上。然后执行合约的对应方法。并会对合约内部状态产生影响。function为合约的具体方法名。

参数说明
---------

1. ``params`` - ``any`` : 合约本身function的参数值，依据合约方法的参数个数和类型进行传递；
2. ``options`` - ``JsonObject`` : 由以下两个字段组成：

    * ``ContractValue`` - ``String`` : [**可选**]如果合约函数为payable类型，则可使用该字段给合约转账，单位为drop；
    * ``Gas`` - ``Number`` : 执行合约函数所需要的手续费用。

	.. _tx-expect:
    * ``expect`` - ``String`` : [**可选**]在chainsql中提供几种预期交易执行结果的返回，不指定则使用"send_success"，可选执行结果如下：

        - "send_success" : 交易发送成功即返回结果；
    	- "validate_success" ： 交易共识成功即返回结果；
    	- "db_success" ： 涉及数据库交易，执行入库成功即返回结果。
3. ``callback`` - ``Function`` : [**可选**]回调函数，如果指定，则通过指定回调函数返回结果，否则需要通过then和catch接收promise结果。

返回值
--------

``JsonObject`` : 合约函数执行成功或失败的结果。返回方式取决于是否指定回调函数。

1. 调用成功 - 指定回调函数，则通过回调函数返回，否则返回返回一个resolve的Promise对象。 ``JsonObject`` 包含以下字段：

	* ``status`` - ``String`` : 表示合约函数执行状态，其值由调用时的expect决定；
	* ``tx_hash`` - ``String`` : 合约函数的交易hash值。
	
2. 调用失败 - 指定回调函数，则通过回调函数返回，否则返回返回一个reject的Promise对象。 ``JsonObject`` 内容依具体错误形式返回。

示例
--------
.. code-block:: javascript

    // use the promise
    contractObj.methods.multiply(6).submit({
        Gas: 500000,
        expect: "validate_success"
    }).then(res => {
        console.log(res);
    }).catch(err => {
        console.log(err);
    });


	// use the callback
    myContract.methods.multiply(6).submit({
        Gas: 500000,
        expect: "validate_success"
    },function (err, res) {
        err ? console.error(err) : console.log(res);
    });
    > res
    {
        status:"validate_success"
        tx_hash:"F29FE3A0652162A480E591B92CB6982408FB4AFEB5BF645024D847E4218385BB"
    }

.. _contract-call:

读取合约内部状态调用
====================
::

    myContract.methods.function([params1[, params2[, ...]]]).call([callback])

这种调用方式只是读取合约内部某个变量状态，非交易，不会对合约内部状态产生影响。function为合约的具体方法名。

参数说明
---------

1. ``params`` - ``any`` : 合约本身function的参数值，依据合约方法的参数个数和类型进行传递；
2. ``callback`` - ``Function`` : [**可选**]回调函数，如果指定，则通过指定回调函数返回结果，否则需要通过then和catch接收promise结果。

返回值
--------

返回值由合约本身的函数规定的返回值个数及类型决定，个数为1时，直接返回该值，个数大于1时，构造为一个JsonObject返回。返回方式取决于是否指定回调函数。

1. 调用成功时，指定回调函数，则通过回调函数返回，否则返回返回一个resolve的Promise对象；
2. 调用失败时，指定回调函数，则通过回调函数返回，否则返回返回一个reject的Promise对象。依具体错误形式返回。

示例
--------
.. code-block:: javascript

    // return only one value
    myContract.methods.getMem().call(function(err, res) {
        err ? console.log(err) : console.log(res);
    });
    > res
    666

    // return more than one value
    myContract.methods.returnMixType().call(function(err, res) {
        err ? console.log(err) : console.log(res);
    });
    > res
    {
        0:"666"
        1:"stringTest2forMixTypeReturn"
    }

------------------------------------------------------------------------------

.. _contract-fallback:

合约fallback函数调用
====================
::

	chainsql.payToContract(contractAddr, contractValue, gas).submit(options)

当合约定义了fallback函数时，可通过payToContract接口直接向合约转账。如果未定义则调用出错。

参数说明
---------

1. ``contractAddr`` - ``String`` : 接收转账的合约地址；
2. ``contractValue`` - ``Number`` : 转账数额；
3. ``gas`` - ``Number`` : 执行转账交易的手续费用；
4. ``options`` - ``JsonObject`` : 指定交易执行到何种状态返回，默认为"send_success", 具体可参考 :ref:`交易expect <tx-expect>`。

返回值
--------

``Promise`` : 根据调用时的expect值，返回对应的执行状态。成功返回一个resolve的Promise对象，失败返回一个reject的Promise对象。

示例
--------
.. code-block:: javascript

    chainsql.payToContract(contractAddr, 2000, 30000000).submit({
        expect: "validate_success"
    }).then(res => {
        console.log(res);
    }).catch(err => {
        console.log(err);
    });
    > res
    {
        status:"validate_success",
        tx_hash:"92A7E277BB4229DAEC71A2D9D8C282FB307E328E8FC05C4BE29D20240A5F9E13"
    }

------------------------------------------------------------------------------

.. _contract-encode:

获取合约函数编码值
==================
.. code-block:: javascript

    contractObj.method.function([params1[, params2, ...]]]).encodeABI()

将合约函数包括参数在内进行编码，得到contract data，或者称为inputdata。可以直接用于合约函数调用，或者在其他合约中作为参数传递，或者使用chainsql的rpc接口调用合约。

参数说明
---------

1. ``params`` - ``any`` : 合约本身function的参数值，依据合约方法的参数个数和类型进行传递；

返回值
--------

``String`` : 进行合约调用时可以使用的函数编码值，contract data。

示例
--------
.. code-block:: javascript

    let funInputData = contractObj.methods.setMem(16).encodeABI();
    console.log(funInputData);
    > "0x6606873b0000000000000000000000000000000000000000000000000000000000000010"

------------------------------------------------------------------------------

.. _contract-event:

合约事件监听
=============
.. code-block:: javascript

    myContract.events.eventFunc([callback]);

参数说明
---------

1. ``callback`` - ``Function`` : [**可选**]回调函数，如果指定，则通过指定回调函数返回结果，否则需要通过then和catch接收promise结果。

返回值
--------

返回值包含合约事件指定的监听内容，返回方式由是否指定回调函数决定。

1. 正常监听：指定回调函数，则通过回调函数返回，否则返回返回一个resolve的Promise对象。具体返回内容包括：
``JsonObject`` : 返回事件内容，具体包含以下字段：

    * ``ContractAddress`` - ``String`` : 合约地址；
    * ``event`` - ``String`` : 事件函数名称；
    * ``raw`` - ``JsonObject`` :  事件返回原始十六进制数据，包括data和topic两个字段；
    * ``returnValues`` - ``JsonObject`` :  按事件定义的返回值顺序以及返回值变量名，给出可读形式的返回值；
    * ``signature`` - ``String`` : 事件函数签名；
    * ``type`` - ``String`` : 类型，固定值为"contract_event"。

2. 监听异常：指定回调函数，则通过回调函数返回，否则返回返回一个reject的Promise对象。依具体错误形式返回。

示例
--------
.. code-block:: javascript

    myContract.events.multiplylog((err, res) => {
        err ? console.log(err) : console.log(res);
    });
    >
    {
        ContractAddress:"zcdFPChLUNYXQTV6zr2osrWG8pV7Zyh8FL"
        event:"multiplylog"
        raw:{
            data:"0x000000000000000000000000B5F762798A53D543A014CAF8B297CFF8F2F937E8000000000000000000000000000000000000000000000000000000000000002A"
            topics:["0x414b7ab3d46ecc8ab359636c133f9a1b88ffc8c08e9560da2b3ef7949edf8ca3", 
                   "0x0000000000000000000000000000000000000000000000000000000000000006"]
        }
        returnValues:{
            number:"6"
            result:"42"
            sender:"zHb9CJAWyB4zj91VRWn96DkukG4bwdtyTh"
            0:"zHb9CJAWyB4zj91VRWn96DkukG4bwdtyTh"
            1:"6"
            2:"42"
        }
        signature:"0x414b7ab3d46ecc8ab359636c133f9a1b88ffc8c08e9560da2b3ef7949edf8ca3"
        type:"contract_event"
    }
