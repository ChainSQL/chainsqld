# 1.记录级权限控制规则
由于加密表在共识阶段Raw字段是无法解密的，无法判断行级控制条件的合法性，所以不同时支持加密表与行级控制功能
## 建表时指定增删改规则
TableListSet的TxFormat中添加一个新的字段sfOperateRule，取值如下：
```
{
    "OperateRule":{
        "Insert":[{"field1":"$account"},{"field2":0},{"field3":"$tx_hash"}],
        "Update":"{'field2':{$le:8}} or {'field3':10}",
        "Delete":{"field1":"$account"},
    }
}
```
说明：
* Insert
    - 插入条件的原则：执行插入操作时字段的值要与这里提供的保持一致
    - 插入时要满足的条件，比如上面，field1这个字段要等于操作者账户，field2要等于0，field3要等于交易hash值；
    - 如果插入时，提供了比较字段的值，则与建表时的条件进行比较，如果未提供，则用建表时的条件赋值给插入字段；
    - **扩展**：限制每个账户可以插入的记录数，如果要做到这一点，需要在表中记录**插入者的账户地址**？Insert的条件是下面这样：
    ```
    {
        "Condition":[{"field1":"$account"},{"field2":0},{"field3":"$tx_hash"}],
        "Count":{"AccountField":"fieldName","CountLimit":10}
    }
    ```
* Update/Delete
    - 与插入不同，后面的条件相当于给每一个Update/Delete交易添加了额外的限制，在Dispose执行Sql操作时，要把Update/Delete的的条件带上去执行；
    - 因为是执行SQL时的判断条件，所以要在共识时去Dispose执行一遍
    - **扩展**：限制可修改的字段，像下面这样：
    ```
    {
        "Condition":{'field2':{$le:8}} or {'field3':10},
        "Fields":["field1","field2"]
    }
    ```

## 插入
修改点：
1. txPrepare
* 根据建表的限制条件为tx填充默认字段
* 如果比较字段已经赋值，进行比较，不通过则返回失败
2. 共识阶段
* 对插入条数条件的验证
3. 同步入库
* 与之前相同

## 更新/删除
1. txPrepare
* 根据建表时指定的条件将Update/Delete的条件组装，然后放到tx的sfCondition字段中
2. 共识阶段
* 需要预执行，在Dispose时取出STTx中sfCondition的值，附加到Update/Delete的where条件中
3. 同步入库
* Dispose时要取出STTx中的sfCondition字段值，附加到Update/Delete的where条件中
## 查找
 完全走授权机制

## 授权接口修改
* 只做授权，不做取消授权
* 可给所有人授权
    - 如果没有做过任何授权操作，只有表的创建者可以操作表
    - 给所有人授权后，可单独给个人授权，判断权限时，优先走个人授权，然后是对所有人的授权，都没有的话默认无任何操作权限
    - 给单个用户授权后，再对所有人授权，之前被授权的单个用户最终权限与本次对所有人授权的权限保持一致，也就是授权要遵循先后顺序
    - 加密表不能对所有人授权，直接返回失败
* 在STEntry中添加通用权限判断，接口格式如下：
```
bool hasAuthority(Account account,int authFlag);
```

# 2.提供加解密接口
## 非对称加密接口
```
{
    "method": "encrypt",
    "params": [{
        "publickeys": ["",""],
        "value":""
    }]
}
```
返回结果
```
{
    "result": [{
        "status": "success",
        "value":""
    }]
}
```
## 非对称解密接口
```
{
    "method": "decrypt",
    "params": [{
        "secret": "",
        "value":""
    }]
}
```
返回结果
```
{
    "result": [{
        "status": "success",
        "value":""
    }]
}
```
