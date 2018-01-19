# 1.Chainsql中的账户

## 1.1 根账户
根账户是在创世区块中就存在的，区块链中所有的众享币(1000亿ZXC)默认都在根账户中。<br>
生成/查看根账户的命令如下：
```
./chainsqld wallet_propose masterpassphrase
```
根账户的地址及种子（种子可生成公钥及私钥，在Chainsql中一般不直接使用私钥，而是使用种子）
```
{
    "account_id" : "zHb9CJAWyB4zj91VRWn96DkukG4bwdtyTh",
    "public_key" : "cBQG8RQArjx1eTKFEAQXz2gS4utaDiEC9wmi7pfUPTi27VCchwgw",
    "master_seed" : "xnoPBzXtMeMyMHUVTgbuqAfg1SUTb"
}
```

## 1.2账户的生成与激活
在chainsql中，除了根账户外，其它账户都是需要激活才能生效的<br>
我们可以在命令行中使用 wallet_propose 命令生成一个新的账户，也可以调用 generateAddress 接口来生成一个新的账户：
```
let account = c.generateAddress();
console.log(account);
```
新生成的账户在链中是无效的，在node.js中查询账户信息：
```
let info = await c.api.getAccountInfo(account.address);
console.log(info);
```
会输出 actNotFound 的错误信息，想要使用一个账户，需要使用pay接口给账户打钱：
```
var owner = {
	secret: "xnoPBzXtMeMyMHUVTgbuqAfg1SUTb",
	address: "zHb9CJAWyB4zj91VRWn96DkukG4bwdtyTh"	
}

await c.connect('ws://106.75.99.244:6006');
console.log('连接成功')
c.as(owner);    //这里owner指一个有足够zxc的账户，第一个转账操作肯定要用根账户
		
let rs = await c.pay(account,200);
console.log(rs);
```
输出结果为tesSUCCESS 说明提交成功，大约2-3秒后交易共识通过，可在链上查询到账户信息

# 2.Chainsql中的预留费用
## 2.1 账户基础预留费
账户预留费用为一个账户激活需要的最少费用，chainsql网络中默认为5zxc<br>

## 2.2 对象增加预留费用
为了防止每个账户恶意创建对象（如建表操作），导致整个区块链网络占用内存过大，每增加一个对象，chainsql会冻结一个ZXC作为对象增加预留费用，相对的，每减少一个对象，也会解除一个ZXC的保留费用冻结。<br>
Chainsql中的对象包括：
- 原Ripple对象 Escrow,PayChannel,Offer,TrustLine
- Chainsql增加的对象Table

> Chainsql预留费用 = 账户基础预留费用 + 对象增加预留费用<br>
预留费用是被冻结的，不能用于转账操作

比如我新生成一个账户A，并且用10ZXC把它激活，那这时A账户中只有5个ZXC是能用的。<br>
A账户要建一张表，建表交易费用为0.5ZXC，对象增加费用为1ZXC，那这时，A账户余额为9.5ZXC，总预留费用为6ZXC，可用资金为3.5ZXC。

# 3.交易费用
在Chainsql中交易费用将会被销毁，不会给任何人，也就是说，Chainsql网络中总的ZXC数量是随着交易不断减少的。
## 3.1 Chainsql交易费用计算规则
基础费用为 1010drop，也就是0.00101zxc，1ZXC = 1000000 drop<br>
Chainsql类型交易费用 = 0.00101(ZXC) + 交易中Raw字段字节数/1024(ZXC)

比如我要建一张表，建表的rpc命令如下：
```
{
    "method": "t_create",
    "params": [
        {
            "offline": false,
            "secret": "xnoPBzXtMeMyMHUVTgbuqAfg1SUTb",
            "tx_json": {
                "TransactionType": "TableListSet",
                "Account": "zHb9CJAWyB4zj91VRWn96DkukG4bwdtyTh",
                "Tables":[
                    {
                        "Table":{
                                "TableName":"aaa"
                        }
                     }
                ],
               "OpType": 1,
               "Raw": [
                    {"field":"id","type":"int","length":11,"PK":1,"NN":1,"UQ":1,"AI":1},
                    {"field":"age","type":"int"},
					{"field":"name","type":"varchar","length":64}
               ]
		
          }
        }
    ]
}
```
这个建表操作中Raw字段较小，假设只有0.1K，那这个交易的交易费用为<br>
0.00101 + 0.1 = 0.10101(zxc)