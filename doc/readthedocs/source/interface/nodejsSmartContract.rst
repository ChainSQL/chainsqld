chainsql-nodejs智能合约接口文档
===============================

引入chainsql模块并设置默认账号
==============

::

    const ChainsqlAPI = require('chainsql').ChainsqlAPI
    const chainsql = new ChainsqlAPI();
    await chainsql.connect("ws://*.*.*.*:6006");
    chainsql.as({
        secret: "****BzXtMeMyMHUVTgbuqAfg1****",
        address: "****CJAWyB4zj91VRWn96DkukG4bwd****"
    });

合约部署
==============

::

    const myContract = chainsql.contract(JSON.parse(abi));
    myContract.deploy({
        ContractData :  deployBytecode,
        arguments : params // Array
    }).submit({
        ContractValue :  "10000000",
        Gas :  4000000
    }, callback);



1.字段要求：

============= =========================== =========
字段           要求                         是否必须 
------------- --------------------------- ---------
abi            JSON格式                    must      
ContractData   以0x开头的字符串             must    
arguments      合约构造函数参数，数组形式    option  
ContractValue  单位为drop，字符串形式       option  
Gas            智能合约所需Gas数            must   
============= =========================== =========

2.回调返回说明：
::

    // 部署成功：
    {
        status : "validate_success",
        tx_hash : transaction_hash,
        contractAddress : contractAddr
    }

::

    // 部署失败：
    {
        status : /*错误码*/,
        error_message : /*错误信息*/,
        tx_hash(option) : transaction_hash
    }

3.示例展示：
::

    const abi = '[...]';
    const deployBytecode = '0x...';
    const myContract = chainsql.contract(JSON.parse(abi));
    myContract.deploy({
        ContractData : deployBytecode,
        arguments : [666]
    }).submit({
        Gas : 4000000000
    }, function (err, res) {
        err ? console.log(err) : console.log(res);
    })
    // res结果：
    // {
    //   contractAddress:"zPqMARn53PpN2fu8eScac4cEYW6b4w8ZH"
    //   status:"validate_success"
    //   tx_hash:"DD443076A8A4B02B6661261CCD456F2DC7F4031F12EC38EAD35E821782328318"
    // }


合约调用
==============


	合约调用需要使用合约对象，获取合约对象有以下两种方式：

	1.部署成功之后的合约对象，如上所示myContract;

	2.通过合约地址和合约abi文件构造合约对象：

::

	const myContract = chainsql.contract(abi, contractAddr);

字段要求：

============= ==================== ========
字段           要求                是否必须
------------- -------------------- --------
abi           JSON格式             must
contractAddr  合约地址，字符串形式   must
============= ==================== ========


   合约调用两种方式：
   
   1.改变合约内部状态发送交易：

::

    myContract.methods.function(params).submit({
        ContractValue :  "10000000",
        Gas:500000,
        expect:"send_success"
    }, (err, res) => {
        //callback
    });

1)字段要求

============= ======================= ==========
    字段               要求            是否必须
------------- ----------------------- ----------
ContractValue 单位为drop，字符串形式   option  
Gas           智能合约所需Gas数        must    
expect        合约调用期望结果         option
============= ======================= ==========

   function为要调用的智能合约函数，且要提供对应参数。submit中需要合约调用所需Gas。
   expect可选参数为：send_success、validate_success、db_success, 默认为send_success。
   
2)交易发送成功回调返回结果：
::

    {
        status: 'send_success',
        tx_hash: transaction_hash
    }


交易发送失败回调返回结果为具体失败信息，如果触发合约require，则增加resultMessageDetail字段，显示require具体提示
::

    {
        resultCode: /*错误码*/,
        resultMessage: /*错误信息*/
        resultMessageDetail: /*触发require时显示*/
    }

3）示例展示
::

    // promise接收结果
    myContract.methods.multiply(6).submit({
        Gas: 500000,
        expect: "validate_success"
    }).then(data => {
        console.log(data);
    }).catch(err => {
        console.log(err);
    });
	// 回调函数接收结果
    myContract.methods.multiply(6).submit({
        Gas: 500000,
        expect: "validate_success"
    },function (err, res) {
        err ? console.log(err) : console.log(res);
    });
    // data或res结果为:
    // {
    //   status:"validate_success"
    //   tx_hash:"F29FE3A0652162A480E591B92CB6982408FB4AFEB5BF645024D847E4218385BB"
    // }


2.不改变合约内部状态本地调用：
::

    myContract.methods.function(params).call(function(err, res){
        //callback
    })


1)function为要调用的智能合约函数，且要提供对应参数。
   
2)执行成功，res会返回调用结果，执行失败，err会返回失败内容。
   
3)示例展示
::

    myContract.methods.getMem().call(function(err, res) {
        err ? console.log(err) : console.log(res);
    });
    // res为mem的值：666


3.支持智能合约event调用。调用方式如下：
::
    myContract.events.eventFunc(callback);


1)eventFunc为要调用的智能合约事件，事件返回结果在callback中提供。事件返回结果包含以下字段：

================ =========================================================
字段              说明
---------------- ---------------------------------------------------------
ContractAddress  合约地址  
event            事件函数名称，
raw              事件返回原始十六进制数据，包括data和topic两个字段
returnValues     按事件定义的返回值顺序以及返回值变量名，给出可读形式的返回值。
signature        事件签名
type             "contract_event"
================ =========================================================

2)示例展示
::
    myContract.events.multiplylog((err, res) => {
        err ? console.log(err) : console.log(res);
    });
    //成功结果：
    //{
    //  ContractAddress:"zcdFPChLUNYXQTV6zr2osrWG8pV7Zyh8FL"
    //  event:"multiplylog"
    //  raw:{
    //    data:"0x000000000000000000000000B5F762798A53D543A014CAF8B297CFF8F2F937E8000000000000000000000000000000000000000000000000000000000000002A"
    //    topics:["0x414b7ab3d46ecc8ab359636c133f9a1b88ffc8c08e9560da2b3ef7949edf8ca3", 
    //            "0x0000000000000000000000000000000000000000000000000000000000000006"]
    //      }
    //  returnValues:{
    //    number:"6"
    //    result:"42"
    //    sender:"zHb9CJAWyB4zj91VRWn96DkukG4bwdtyTh"
    //    0:"zHb9CJAWyB4zj91VRWn96DkukG4bwdtyTh"
    //    1:"6"
    //    2:"42"}
    //  signature:"0x414b7ab3d46ecc8ab359636c133f9a1b88ffc8c08e9560da2b3ef7949edf8ca3"
    //  type:"contract_event"

4.提供encodeABI()方法，返回带指定参数的合约函数的inputdata。
::
    let funInputData = contractObj.method.function(param).encodeABI()

1）示例展示
::

    let funInputData = contractObj.methods.setMem(16).encodeABI();
    console.log(funInputData);
    //结果为：0x6606873b0000000000000000000000000000000000000000000000000000000000000010