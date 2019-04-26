.. _recordLevel:

======================
行级权限控制规则
======================

.. note:: 由于加密表在共识阶段Raw字段是无法解密的，无法判断行级控制条件的合法性，所以不同时支持加密表与行级控制功能.

设计原则
*****************

1. 灵活 可以单独使用某种规则，也可以自由组合使用
2. 精细 可精确到控制每一行的增删改查规则，可控制插入条数、可修改字段

------------------------------------------------------------------------------

建表时指定增删改规则
*************************

TableListSet的交易中添加一个新的字段 ``OperateRule`` ，示例如下：

.. code-block:: json

    {
        "OperationRule":{
            "Insert":{
                "Condition":{"account":"$account","txid":"$tx_hash"},
                "Count":{"AccountField":"account","CountLimit":5}
            },
            "Update":{
                "Condition":{"$or":[{"age":{"$le":28}},{"id":2}]},
                "Fields":["age"]
            },
            "Delete":{
                "Condition":{"account":"$account"}
            },
            "Get":{
                "Condition":{"id":{"$ge":3}}
            }
        }
    }

完整的建表交易：

.. code-block:: json

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
                                    "TableName":"tableWithRule"
                            }
                        }
                    ],
                "OpType": 1,
                "Raw": [
                        {"field":"id","type":"int","length":11,"PK":1,"NN":1,"UQ":1},
                        {"field":"age","type":"int"},
                        {"field":"name","type":"varchar","length":32},
                        {"field":"account","type":"varchar","length":64},
                        {"field":"txid","type":"varchar","length":64}
                ],
                "OperationRule":{
                        "Insert":{
                            "Condition":{"account":"$account","txid":"$tx_hash"},
                            "Count":{"AccountField":"account","CountLimit":5}
                        },
                        "Update":{
                            "Condition":{"$or":[{"age":{"$le":28}},{"id":2}]},
                            "Fields":["age"]
                        },
                        "Delete":{
                            "Condition":{"account":"$account"}
                        },
                        "Get":{
                            "Condition":{"id":{"$ge":3}}
                        }
                }
            }
            }
        ]
    }

------------------------------------------------------------------------------  

详细说明
*****************

Insert
============

Insert中可设置 ``Condition`` 与 ``Count`` 两个字段的值

-----------------------
Condition
-----------------------

指定插入操作可设置的默认值

1 表示插入表的 ``field1`` 字段的默认值为0

.. code-block:: json

    {"field1":0 }

2 表示插入表的 ``field2`` 的默认值为当前执行插入操作的账户地址

.. code-block:: json

    {"field2":"$account"}

3 表示插入表的 ``field3`` 的默认值为当前插入交易的hash值

.. code-block:: json

    {"field3":"$tx_hash"}


-----------------------
Count
-----------------------

Count 可以限制每个账户可以插入的记录数

示例

.. code-block:: json

    {
        "Count":{
            "AccountField":"fieldName",
            "CountLimit":10
        }
    }

说明:

1 ``AccountField`` - 指定建表字段中哪个字段为账户字段

2 ``CountLimit``   - 表示每个账户可插入几行（这里说的账户指的是AccountField指定的账户字段的值）

--------------
注意
--------------

1. 如果指定了默认值，插入时又指定了其它值，插入交易会执行失败
2. Insert中Condition与Count可同时使用，也可以只指定一个

    * 只指定Condition:指定一些字段的默认值
    * 只指定Count：这种情况下，插入者可以指定账户字段的值为其它账户，但是每个账户相关的记录还是会受条数限制影响。

------------------------------------------------------------------------------

Update
============

示例

.. code-block:: json

    {
        "Condition":{
            "$or":[
                {"field2":{"$le":8}},
                {"field3":10}
            ]
        },
        "Fields":["field1","field2"]
    }

-----------------------
Condition
-----------------------

指定更新操作的条件，这个条件会在执行真正的更新交易时与更新交易的条件做‘and’操作，如：执行下面的更新操作，将id=1的记录中的age的值更新为11：

.. code-block:: json

    {
        "method":"r_update",
        "params":[
            {
                "offline":false,
                "secret":"xxWFBu6veVgMnAqNf6YFRV2UENRd3",
                "tx_json":{
                    "TransactionType":"SQLStatement",
                    "Account":"z9VF7yQPLcKgUoHwMbzmQBjvPsyMy19ubs",
                    "Owner":"zHb9CJAWyB4zj91VRWn96DkukG4bwdtyTh",
                    "Tables":[
                        {
                            "Table":{
                                "TableName":"tableWithRule"
                            }
                        }
                    ],
                    "Raw":[
                        {"age":11},
                        {"id":1}
                    ],
                    "OpType":8
                }
            }
        ]
    }

结合行级控制里面的Update条件，最终的Raw字段会取值如下： 

.. code-block:: json

    {
        "Raw":[
            {
                "age":11
            },
            {
                "$and":[
                    {
                        "id":1
                    },
                    {
                        "$or":[
                            {
                                "field2":{"$le":8}
                            },
                            {
                                "field3":10
                            }
                        ]
                    }
                ]
            }
        ]
    }

-----------------------
Fields
-----------------------

Fields指定了更新操作所能更新的字段，如果不添加Fields条件，默认可以更新表中所有字段

.. note:: 如Insert条件中某个字段在Count条件中被指定为账户字段，那这个字段是一定不能出现在Update条件的Fields中的，这种条件下必须显式的在Fields中将账户字段排除出去，否则会报 ``temBAD_OPERATIONRULE`` 错误

----------------------------------------------------------------------------------------

Delete/Get
=========================================

Delete与Get操作只有 ``Condition`` 条件

如Insert条件中某个字段在 ``Count`` 条件中被指定为账户字段，删除的 ``Condition`` 中必须显示指定操作账户字段为本账户字段：

.. code-block:: json

    {
        "Condition":{"account":"$account"}
    }
