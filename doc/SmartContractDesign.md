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
- 合约地址与普通账户地址生成规则一致，合约只有地址，无公私钥
- 合约在Chainsql中也是以AccountRoot这种LedgerNode的形式存在
- AccountRoot增加了下面的可选字段：

字段名 |类型| 说明
---|---|---
StorageOverlay |STMap256| 合约中的存储
Nonce |UINT32| 属于创建合约的账户，生成合约地址时使用
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
bool ret = owner.table("table_name").create("create raw string");
//example
function createTable(string name,string raw) public returns(bool ret){
    ret = msg.sender.table(name).create(raw);
}
```
2. 插入
```
bool ret = owner.table("table_name").insert("insert raw string");
//example
function insertToTable(address owner,string name,string raw) public retunrs(bool ret){
    ret = owner.table(name).insert(raw);
}
```
3. 删除
```
//delete参数代表删除条件
bool ret = owner.table("table_name").delete("raw string");
//example
function deleteFromTable(address owner,string name,string raw) public returns (bool ret){
    ret = owner.table(name).delete(raw);
}
```
4. 修改
```
//update需要两个参数
bool ret = owner.table("table_name").get("get raw").update("raw string");
//example
function updateTable(address owner,string name,string getRaw,string updateRaw) public returns (bool ret){
    ret = owner.table(name).get(getRaw).update(updateRaw);
}
```
5. 查询
- 查询返回一个句柄，需要自定义一个类型，如handle（或者直接使用uint256）
- handle不可作为函数返回值返回（只能作为临时对象使用），也不能作为成员变量使用（作为成员变量使用，跨交易时，会获取不到内容）
- 可根据查询得到的句柄去获取查询结果中的字段值
- 提供遍历方法，可根据句柄遍历查询结果
```
handle result = owner.table(name).get(raw);
uint rowSize = getRowSize(result);
uint colSize = getColSize(result);
string output;
for(uint i=0; i<size; i++){
    if(getValueByKey(result,i,"status") > 0){
        for(uint j=0; j<colSize; j++){
            output += getValueByIndex(result,i,j);
            output += ",";
        }
        output += ";";
    }
}
```
6. 事务相关
- 增加两个指令beginTrans()、commit()，指令之间的部分组成事务
- 两个指令之间的操作逐行执行
```
beginTrans();
owner.table(name).insert(raw);
handle res = owner.table(name).get(getRaw);
if(getRowSize(res) > 0){
    owner.table(name).get(getRaw).update(updateRaw);
}
...
//every op is alone
commit();
```
7. 授权
- 必须由表的拥有者发起
```
owner.table("table_name").grant(user_address,"grant_raw");
//example
function grantTable(string name,address user,string raw) public returns(bool ret){
    ret = msg.sender.table(name).grant(user,raw);
}
```

8. 删除表
- 必须由表的拥有者发起
```
owner.table("table_name").drop();
//example
function dropTable(string name) public returns(bool ret){
    ret = msg.sender.table(name).drop();
}
```
9. 重命名表
- 必须由表的拥有者发起
```
owner.table("table_name").rename("new_name");
//example
function renameTable(string name,string newName) public returns(bool ret){
    ret = msg.sender.table(name).rename(newName);
}
```
#### 9.代币接口
1. 合约信任发行币网关接口
- 可设置由合约拥有者发起
```
modifier onlyOwner {
    require(msg.sender == owner);
    _;
}
function trust(address gateway,string coin,uint amount) public onlyOwner
{
    msg.sender.trustSet(gateway,coin,amount);
}
```
2. 给合约转网关代币
- 如果fallback函数是payable的可以转直接转账系统币，转网关币也需要fallback？


3. 查询网关代币余额
```
function gatewayBalance(address addr,address gateway,string coin) public returns(uint amount){
    return addr.gatewayBalance(gateway,coin);
}
```
4. 转账发行币
- 接收网关代币的方法必须是payable的
- 先将代币转给合约，然后合约内调用transfer/send转出
```
function transfer(address to,address gateway,string coin,uint amount) public payable{
    uint balance = gatewayBalance(msg.sender,gateway,coin);
    require(balance > amount,"balance insufficient");
    to.transfer(gateway,coin,amount);   
}
```
