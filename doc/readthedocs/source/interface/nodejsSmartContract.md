# chainsql-nodejs智能合约接口文档

### Node.js

- 引入chainsql模块并设置默认账号

```
    const ChainsqlAPI = require('chainsql').ChainsqlAPI
    const chainsql = new ChainsqlAPI();
    await chainsql.connect("ws://*.*.*.*:5125");
	chainsql.as({
		secret: "****BzXtMeMyMHUVTgbuqAfg1****",
		address: "****CJAWyB4zj91VRWn96DkukG4bwd****"
	});
```
- 合约部署

```
    const myContract = chainsql.contract(JSON.parse(abi));
    myContract.deploy({
        ContractData :  deployBytecode,
        arguments : params // Array
    }).submit({
        ContractValue :  "10000000",
        Gas :  4000000
    }, callback);
```


    字段要求：
|     字段      |           要求           | 是否必须 |
| ------------- | ------------------------ | ------- |
| abi           | JSON格式                 | must    |
| ContractData  | 以0x开头的字符串          | must    |
| arguments     | 合约构造函数参数，数组形式 | option  |
| ContractValue | 单位为drop，字符串形式    | option  |
| Gas           | 智能合约所需Gas数         | must    |
   回调返回说明：

```
    部署成功：
    {
		status : “validate_success”,
		tx_hash : transaction_hash,
		contractAddress : contractAddr
    }
```
```
    部署失败：
    {
		status : /*错误码*/,error_message : /*错误信息*/,
		tx_hash(option) : transaction_hash
    }
```
- 合约调用

	合约调用需要使用合约对象，获取合约对象有以下两种方式：

	1.部署成功之后的合约对象，如上所示myContract;

	2.通过合约地址和合约abi文件构造合约对象：

```
    const myContract = chainsql.contract(abi, contractAddr);
```

|     字段     |        要求         | 是否必须 |
| ------------ | ------------------ | ------- |
| abi          | JSON格式            | must    |
| contractAddr | 合约地址，字符串形式 | must    |

   合约调用两种方式：
   1.改变合约内部状态发送交易：

```
    myContract.methods.function(params).submit({
		ContractValue :  "10000000",
		Gas:500000，
		expect:"send_success"
	}, (err, res) => {
		//callback
	});
```
|     字段      |         要求          | 是否必须 |
| ------------- | -------------------- | ------- |
| ContractValue | 单位为drop，字符串形式 | option  |
| Gas           | 智能合约所需Gas数      | must    |
| expect        | 合约调用期望结果       | option   |
   function为要调用的智能合约函数，且要提供对应参数。submit中需要合约调用所需Gas。
   expect可选参数为：send_success、validate_success、db_success, 默认为send_success。
   交易发送成功回调返回结果：

```
    {
    	status: 'send_success',
    	tx_hash: transaction_hash
    }
```

   交易发送失败回调返回结果为具体失败信息，如果触发合约require，则增加xxx字段，显示require具体提示

```
    {
    	resultCode: /*错误码*/,
    	resultMessage: /*错误信息*/
		xxx: /*触发require时显示*/
    }
```

   2.不改变合约内部状态本地调用：

```
    myContract.methods.function(params).call(function(err, res){
		//callback
    })
```

   function为要调用的智能合约函数，且要提供对应参数。
   执行成功，res会返回调用结果，执行失败，err会返回失败内容。

   3.支持智能合约event调用。调用方式如下：
```
    myContract.events.eventFunc(callback);
```

   eventFunc为要调用的智能合约事件，事件返回结果在callback中提供。事件返回结果包含以下字段：

```
    ContractAddress : 合约地址，    
    event : 事件函数名称，
    raw : 事件返回原始十六进制数据，包括data和topic两个字段
    returnValues : 按事件定义的返回值顺序以及返回值变量名，给出可读形式的返回值。
    signature : 事件签名
    type : "contract_event"
```
   4.提供encodeABI()方法，返回带指定参数的合约函数的inputdata。
```
    let funInputData = contractObj.method.function(param).encodeABI()
```
