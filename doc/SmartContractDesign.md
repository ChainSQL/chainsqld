### 1. 引言
#### 1.1 编写目的：
- 设计Chainsql中智能合约的实现方案，可通过此文档可对Chainsql中的智能合约有深入的了解
#### 1.2 背景
- 基于以下技术实现
    - Ripple 的区块链技术实现
    - 以太坊客户端 [C++实现](https://github.com/ethereum/aleth)
    - 以太坊虚拟机EVM
    - llvm框架
### 2. 总体说明
#### 2.1 阶段目标
1. 一期实现对以太坊智能合约的支持，兼容用solidity语言编写的智能合约
2. 二期：
    1. 实现在智能合约中支持Chainsql对表的操作
    2. 支持在智能合约中操作通过网关发行的代币，支持在智能合约中发行网关代币及相关操作
#### 2.2 修改方案
一期修改方案：
- 底层：在Chainsql最新源码基础上添加对以太坊智能合约的支持，集成evm中JIT的实现模块，llvm编译模块，并添加新的交易类型与虚拟机进行交互，最终实现可通过交易完成合约的发布、调用
- 上层：上层提供Node.js与Java版本的api，在原chainsql api的基础上增加对智能合约的支持


#### 2.3 系统架构
![SmartContractDesign](../images/SmartContract.png)


### 3. 基本功能
#### 3.1 合约部署
- 支持通过chainsql节点部署智能合约，通过api进行操作的步骤如下
    1. 用户编写solidity合约
    2. 编译solidity代码，得到可执行字节码
    3. 调用chainsql提交部署智能合约交易，并订阅交易状态
    4. 交易共识通过触发回调，合约部署成功
    5. 通过查询合约交易信息，获得合约地址

#### 3.2 合约调用
- 合约部署成功后，api层可调用合约对象中的方法(与solidity中的定义一致)来调用合约，合约的调用方式分为两种：
    1. set方法，改变合约状态，这种方法需要以发送交易的方式进行调用
    2. get方法，不改变合约状态，这种方法可直接调用chainsql提供的接口来获取返回结果

#### 3.3 给合约地址转账
- 与其它账户地址不同，合约地址在Chainsql网络中是可以是没有ZXC余额的
- 不能通过Payment类型的交易给合约地址转账
- Api层提供了**payToContract**接口对合约地址进行转账
- 合约中必须提供payable修饰的fallback函数，不然合约地址无法接受转账

#### 3.4 Gas
- Chainsql中的智能合约执行也是消耗Gas的，Gas计算规则与以太坊中的一致
- Chainsql中用户不可以设置GasPrice，只可以设置GasLimit，交易出现排队时，根据交易中Fee字段的值排列优先级
- Chainsql中的GasPrice由系统决定，并且会随当前网络负载而变化
- GasPrice初始值为10drop(1e-5 ZXC)，最大值为20drop

#### 3.5 支持表操作
- 支持在智能合约中进行表的各种操作


#### 3.6 支持网关发行代币、代币流通
- 支持智能合约中进行网关设置、信任网关、代币的转账等

### 4. 性能指标

### 5. 实现
#### 5.1 LedgerNode修改： AccountRoot
- 合约地址地址生成使用原有地址计算规则，以部署合约帐户与帐户当前交易序号为原像，合约只有地址，无公私钥
- 合约在Chainsql中也是以AccountRoot这种LedgerNode的形式存在
- AccountRoot增加了下面的可选字段：

字段名 |类型| 说明
---|---|---
StorageOverlay |STMap256| 合约中的存储
ContractCode |STBlob| 合约中的字节码，调用合约时使用

#### 5.2 增加交易类型Contract
- Chainsql中智能合约的部署、修改状态的方法调用，都要通过Contract类型的交易进行
- 交易中的字段说明（略过常规字段如Account,Sequence等）

字段名 | 类型| 是否必填 | 说明
---|--- | --- | ---
ContractOpType | UINT16 | 必填 | 操作类型，1为合约部署，2为合约调用
ContractData |STBlob| 必填 | 部署合约/调用合约时的输入值
Gas | UINT32 | 必填 | 部署/调用合约交易时，需设置的Gas上限
ContractAddress | STACCOUNT | 选填 | 合约地址，调用合约时填写
ContractValue |STAMOUNT| 选填 | 本次交易要给合约地址转账的金额

#### 5.3 增加接口 contract_call
- Chainsql中不修改合约状态的方法调用，需要通过contract_call接口来实现
- 接口中的字段说明：

字段名 |类型|说明
---|---|---
account |字符串| 调用合约的地址
contract_address |字符串| 合约地址
contract_data |字符串| 合约数据

#### 5.4 自定义数据类型 STMap256
- key 与 value均为 uint256 类型的map，用于存储合约中的状态


### 6. RPC接口
#### 6.1 合约部署交易
示例：
```
{
    "method": "submit",
    "params": [
        {
            "secret": "x████████████████████████████",
            "tx_json": {
                "TransactionType": "Contract",
                "Account": "zHyz3V6V3DZ2fYdb6AUc5WV4VKZP1pAEs9",
                "ContractOpType":1,
                "ContractData":"60806040526001600055600180556101ad8061001c6000396000f300608060405260043610610057576000357c0100000000000000000000000000000000000000000000000000000000900463ffffffff1680632ddbd13a1461005c57806336005839146100875780638a4068dd146100b4575b600080fd5b34801561006857600080fd5b506100716100be565b6040518082815260200191505060405180910390f35b34801561009357600080fd5b506100b2600480360381019080803590602001909291905050506100c4565b005b6100bc61017f565b005b60015481565b60005473",
                "Gas":3000000,
                "Fee":"10000"
          }
        }
    ]
}
```
tx_json中的必填字段：
- TransactionType：交易类型，此处为Contract
- Account：操作账户地址
- ContractOpType：合约操作类型，部署为1，调用为2
- ContractData：合约数据
- Gas：Gas上限

#### 6.2 合约的调用（Set）
示例：
```
{
    "method": "submit",
    "params": [
        {
            "secret": "x████████████████████████████",
            "tx_json": {
                "TransactionType": "Contract",
                "Account": "zHyz3V6V3DZ2fYdb6AUc5WV4VKZP1pAEs9",
                "ContractOpType":2,
                "ContractData":"360058390000000000000000000000000000000000000000000000000000000000000004",
                "Gas":3000000,
                "ContractAddress":"zKLWJLq4oKyqkexhUbVrF77ai4TNPQZSAQ"
           }
        }
    ]
}
```
tx_json中的必填字段：
- TransactionType：交易类型，此处为Contract
- Account：操作账户地址
- ContractOpType：合约操作类型，部署为1，调用为2
- ContractData：合约数据
- Gas：Gas上限
- ContractAddress：调用的合约地址

#### 6.3 合约的调用（Get）
```
{
    "method": "contract_call",
    "params": [
        {
            "account": "x████████████████████████████",
            "contract_data":"70a082310000000000000000000000009ce44096251e2c23ad95af33564a6b7addfb3317",
            "contract_address":"zJAErHfpG8rpzucKQoUMU4NAPKpR1DrmU2"
        }
    ]
}
```
### 7.Websocket接口
与RPC类似，略

### 8. 对表的支持
- 说明：
    - owner address 类型，表的拥有者地址
    - raw字符串为json字符串，非16进制
1. 建表
```
owner.create("table_name","create raw string");
//example
function createTable(string name,string raw) public{
    msg.sender.create(name, raw);
}
```
2. 插入
```
owner.insert("table_name", "insert raw string");
//example
function insertToTable(address owner,string name,string raw) public{
    owner.insert(name,raw);
}
```
3. 删除
```
//delete参数代表删除条件
owner.delete("table_name","raw string");
//example
function deleteFromTable(address owner,string name,string raw) public{
    owner.delete(name,raw);
}
```
4. 修改
```
//update需要两个参数
owner.update(table_name,"raw string","get raw");
//example
function updateTable(address owner,string name,string getRaw,string updateRaw) public{
    owner.update(name, updateRaw, getRaw);
}
```
5. 查询
- 查询返回一个句柄，需要自定义一个类型，如handle（或者直接使用uint256）
- handle不可作为函数返回值返回（只能作为临时对象使用），也不能作为成员变量使用（作为成员变量使用，跨交易时，会获取不到内容）
- 可根据查询得到的句柄去获取查询结果中的字段值
- 提供遍历方法，可根据句柄遍历查询结果
```
uint256 handle = owner.get(tableName, raw);
uint row = db.getRowSize(handle);
uint col = db.getColSize(handle);
string memory xxx;
for(uint i=0; i<row; i++)
{
      for(uint j=0; j<col; j++)
      {
         string memory y1 = (db.getValueByIndex(handle, i, j));
         string memory y2 = (db.getValueByKey(handle, i, field));
      }   
  }
```
6. 事务相关
- 增加两个指令beginTrans()、commit()，指令之间的部分组成事务
- 两个指令之间的操作逐行执行
```
db.beginTrans();
owner.insert(name.raw);
uint256 handle = owner.get(name,getRaw);
if(db.getRowSize(handle) > 0){
    owner.update(name, updateRaw, getRaw);
}
...
//every op is alone
db.commit();
```
7. 授权
- 必须由表的拥有者发起
```
owner.grant(user_address,table_name,"grant_raw");
//example
function grantTable(string name,address user,string raw) public{
    msg.sender.grant(user,name,raw);
}
```

8. 删除表
- 必须由表的拥有者发起
```
owner.drop("table_name");
//example
function dropTable(string name) public{
    msg.sender.drop(name);
}
```
9. 重命名表
- 必须由表的拥有者发起
```
owner.rename("table_name","new_name");
//example
function renameTable(string name,string newName) public{
    msg.sender.rename(name,newName);
}
```
#### 9.
- 说明：
    - 添加了合约中对网关设置，信任，转账网关代币，查询信任额度，查询网关代币余额功能的支持
    - 函数中涉及到给合约地址转账网关代币的操作，需要添加payable修饰符。
    - solidity本身没有提供获取合约地址的指令，需要通过接口传入。
    - 无信任关系时，查询信任额度，查询网关代币余额返回-1


相关指令示例:

1 网关的accoutSet属性设置。

```
   /*
	*  设置网关相关属性
	* @param uFlag   一般情况下为8，表示asfDefaultRipple，详见https://developers.ripple.com/accountset.html#accountset-flags
	* @param bSet    true，开启uFlag；false 取消uFlag。
	*/
	function accountSet(uint32 uFlag,bool bSet) public {
		msg.sender.accountSet(uFlag,bSet);
	}
```

2  网关交易费率
```
   /*
	*  设置网关交易费率
	* @param sRate  交易费率。范围为"1.0" - "2.0",例如 "1.005","1.008"  
	*/
	function setTransferRate(string sRate) public {
		msg.sender.setTransferRate(sRate);
	}
```

3  设置网关交易费用范围
```
    /*
	*  设置网关交易费用范围
	* @param minFee   网关交易最小花费
	* @param maxFee   网关交易最大花费
	*/
	function setTransferRange(string minFee,string maxFee) public {
		msg.sender.setTransferRange(minFee,maxFee);
	}	
```

4 设置信任网关代币以及代币的额度
```
	/*
	*   设置信任网关代币以及代币的额度
	* @param value           代币额度
	* @param sCurrency       代币名称
	* @param sGateway        信任网关地址
	*/
	function trustSet(string value,	string sCurrency, string sGateway) public {

		msg.sender.trustSet(value,sCurrency,sGateway);
	}
	
	/*
	*   设置信任网关代币以及代币的额度
	* @param contractAddr    合约地址
	* @param value           代币额度
	* @param sCurrency       代币名称
	* @param sGateway        信任网关地址
	*/
	function trustSet(address contractAddr,string value,
						string sCurrency, string sGateway) public {

		// 合约地址也可信任网关
		contractAddr.trustSet(value,sCurrency,sGateway);
	}
	
```
5 查询网关的信任代币信息
```
	/*
	*   查询网关的信任代币信息
	* @param  sCurrency          代币名称
	* @param  sGateway           网关地址
	* @return -1:不存在网关代币信任关系; >=0 信任网关代币额度
	*/
	function trustLimit(string sCurrency,string sGateway)
	public view returns(int) {

		return msg.sender.trustLimit(sCurrency,sGateway);
	}
	
	
	/*
	*   查询网关的信任代币信息
	* @param  contractAddr       合约地址
	* @param  sCurrency          代币名称
	* @param  sGateway           网关地址
	* @return -1:不存在网关代币信任关系; >=0 信任网关代币额度
	*/
	function trustLimit(address contractAddr,string sCurrency,string sGateway)
	public view returns(int) {
	    // 合约地址也可查询网关信任代币信息
	    return contractAddr.trustLimit(sCurrency,sGateway);

	}	
```

6 查询网关代币余额 
```
	/*
	*   获取网关代币的余额
	* @param  sCurrency       代币名称
	* @param  sGateway        网关地址
	* @return -1:不存在该网关代币; >=0 网关代币的余额
	*/
	function gateWayBalance(string sCurrency,string sGateway) returns(uint256) public {

	   return msg.sender.gateWayBalance(sCurrency,sGateway);
	}
	
	
	/*
	*   获取网关代币的余额
	* @param  contractAddr    合约地址
	* @param  sCurrency       代币名称
	* @param  sGateway        网关地址
	* @return -1:不存在该网关代币; >=0 网关代币的余额
	*/
	function gateWayBalance(address contractAddr,string sCurrency,string sGateway) returns(uint256) public {
	   // 合约地址也可获取网关代币的余额
	   return contractAddr.gateWayBalance(sCurrency,sGateway);
	}	
```


7 代币转账接口
```
	/*
	*   转账代币
	* @param accountTo         转入账户
	* @param value             代币数量
	* @param sCurrency         代币名称
	* @param sGateway          网关地址
	*/
	function pay(string accountTo,string value,
						string sCurrency,string sGateway) public payable{
		uint balance = gatewayBalance(sCurrency,sGateway);
    	require(balance >= value,"balance insufficient");
    	
		msg.sender.pay(accountTo,value,sCurrency,sGateway);
	}
	
	/*
	*   转账代币
	* @param contractAddr      合约地址
	* @param accountTo         转入账户
	* @param value             代币数量
	* @param sCurrency         代币名称
	* @param sGateway          网关地址
	*/
	function pay(address contractAddr,string accountTo,string value,
						string sCurrency,string sGateway) public payable{
		uint balance = gatewayBalance(contractAddr,sCurrency,sGateway);
    	require(balance >= value,"balance insufficient");
    	
    	// 合约地址也可转账代币
	    contractAddr.pay(accountTo,value,sCurrency,sGateway);
	}	
```
