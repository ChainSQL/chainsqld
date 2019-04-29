Raw字段解析
=======================

    | Chainsql表相关操作中，一般用Raw字段来表示实际操作内容，用到Raw字段的操作包括：建表、授权、插入、更新、删除、查询。
    | 下面针对各个操作，对Raw字段进行解析。

.. _create-table:

建表
---------

示例
************
    建表交易Raw字段示例：

.. code-block:: json

    {
        "Raw": [
            {
                "field": "id",
                "type": "int",
                "length": 11,
                "PK": 1,
                "NN": 1,
                "UQ": 1,
                "Index": 1
            },
            {
                "field": "age",
                "type": "int"
            },
            {
                "field": "name",
                "type": "varchar",
                "length": 64
            },
            {
                "field":"pay_money",
                "type":"decimal",
                "length":16,
                "accuracy":4
            },
            {            
                "field":"create_time",
                "type":"date",
                "default":"2018-09-09"
            },
            {
                "field":"update_time",
                "type":"datetime",
                "default":"2018-09-09 11:12:13"
            },
            {
                "field":"tx_hash",
                "type":"varchar",
                "length":64
            }
        ]
    }

说明
***********

- 在建表交易中，Raw字段用来描述表的结构
- 如果想要向表中插入交易hash，需要预留hash字段，如上例中的 tx_hash（名称非固定）
- Raw字段是一个数组，数组中每个元素是对表中一个字段的描述，包括列名，数据类型，数据长度，约束，索引等。 具体见下表:

.. list-table::

    * - **域**
      - **类型**
      - **值**
    * - field
      - 字符串
      - 列名。
    * - type
      - 字符串
      - 列的数据类型，可选值有int/float/double/decimal/varchar/blob/text/datetime
    * - length
      - 整数
      - 列的数据长度。
    * - default
      - 整数或字符串
      - 字段默认值，在插入一行记录，字段不予赋值时默认填充的值
    * - PK
      - 整数
      - 主键（Primary Key),值为1表示为列创建主键约束。
    * - NN
      - 整数
      - 非空（Not Null),值为1表示列的值不能为空（NULL）。
    * - UQ
      - 整数
      - 唯一（Unique），值为1表示为列创建唯一约束。
    * - Index
      - 整数
      - 索引，值为1表示为列建立索引。
    * - FK
      - 整数
      - 外键，值为1表示为列创建外键约束。必须配合REFERENCES使用。
    * - REFERENCES
      - 对象
      - 设置对其它表的外键引用，值的格式为{"table": "tablename", "field": "filedname"}。

.. _grant-table:

授权
--------------

示例
************
    授权交易Raw字段示例：

.. code-block:: json

    {
        "Raw": [
            {
                "select": true,
                "insert": true,
                "update": true,
                "delete": true
            }
        ]
    }

说明
**********
- 授权交易的Raw字段比较容易理解，分别表示对各个操作的授权或取消授权。
- 权限包括：insert,update,delete,select四种
- 可以四种都写全，也可以只写一部分（没写的部分继承之前的权限）

.. _insert-table:

插入
-------------

示例
****************
    插入交易Raw字段示例：

.. code-block:: json

    {
        "Raw": [
            {
                "id": 1,
                "name": "张三"
                "age": 11,
                "pay_money":12.345,
                "create_time":"2019-4-26",
                "update_time":"2019-4-26 13:14:15"
            },
            {
                "id": 2,
                "name": "李四",
                "age": 12,
                "pay_money":10,
                "create_time":"2019-4-28",
                "update_time":"2019-4-29 13:14:15"
            }
        ]
    }

说明
************
- 插入交易的Raw字段也比较简单，就是对各个字段赋值
- 一个插入交易中，可插入多条记录
- 可通过设置与Raw字段同级的 ``AutoFillField`` 字段来插入当前交易的 ``hash`` 值，示例如下：

.. code-block:: json

    {
        "method": "r_insert",
        "params": [
            {
                "offline": false,
                "secret": "xnoPBzXtMeMyMHUVTgbuqAfg1SUTb",
                "tx_json": {
                    "TransactionType": "SQLStatement",
                    "Account": "zHb9CJAWyB4zj91VRWn96DkukG4bwdtyTh",
                    "Owner": "zHb9CJAWyB4zj91VRWn96DkukG4bwdtyTh",
                    "Tables":[{
                        "Table":{
                            "TableName":"hi33"
                        }
                    }],
                    "Raw": [{"id":3,"name":"hi","age":11,"pay_money":12.3456,"create_time":"2014-19-16","update_time":"2019-04-26 19:20:30"}],
                    "OpType": 6,
                    "AutoFillField":"tx_hash"
                }
            }
        ]
    }

.. _update-table:

更新
-----------

示例
*********

    更新交易Raw字段示例：

.. code-block:: json

    {
        "Raw": [
            {
                "name": "李四"
                "update_time":"2019-4-26 14:14:15"
            },
            {
                "$or":{
                    "id": 2,
                    "name": "张三"
                }
            }
        ]
    }

说明
*********
- Raw数组中第一个对象是要更新的列名和值，其后的所有对象均表示更新条件， 对象内的多个条件为与（and）关系，对象之间为或（or）关系
- 上例中Raw字段的意义为：

.. code-block:: sql

    update xxx set name='李四',update_time='2019-4-26 14:14:15' where id=2 or name='张三';

删除
----------------
示例
*******
    
    删除交易Raw字段示例：

.. code-block:: json

    {
        "Raw": [
            {
                "$or":{
                    "id": 2,
                    "name": "张三"
                }
            }
        ]
    }

说明
**********

- 删除交易中，Raw字段表示要删除记录的查询条件，查询条件的表示方法参考 查询
- 上例中Raw字段的意义为：

.. code-block:: sql

    delete from xxx where id=2 or name='张三';

.. _查询Raw详解:

查询
-----------
示例
************
    查询的Raw字段较为复杂，可以实现mysql中的 ``limit`` , ``order`` , ``withfields`` 等关键字

.. code-block:: json

    {
        "Raw": [
            [],
            {
                "$or":{
                    "id": 2,
                    "name": "张三"
                }
            }
        ]
    }

说明
***********
- | Raw数组中，第一个元素为一个数组，表示要查询的结果中包含哪些字段，后面的元素表示查询条件，多个查询条件之间是or的关系，单个查询条件内部是and关系。
  | 也可以如上例中在单个元素内部用 ``$or`` 、 ``$and`` 来表示or与and关系
- 上例中Raw字段的意义为：

.. code-block:: sql

    select * from xxx where id=2 or name='张三';

复杂查询
-----------------

.. _withField-introduce:

对结果进行统计
******************************
    查询中可对结果作统计 ，如 ``count(*)`` ， ``sum(age)`` 等，需要在字段数组中指定，示例如下：

.. code-block:: json

    {
        "Raw": [
            [ "count(*) as count" ],
            {
                "name": "张三"
            }
        ]
    }

表示的意义为:

.. code-block:: sql

    select count(*) as count from xxx where name='张三';


对查询结果进行排序
******************************
    对结果排序需在查询条件中指定

.. code-block:: json

    {
        "Raw": [
            [ ],
            {
                "name":"xxx"
            }, 
            {
                "$order":[{"id":1},{"name":-1}]
            }
        ]
    }

表示的意义为:

.. code-block:: sql

    select * from xxx where name='张三' order by id ASC, name desc;

分页查询
******************************

    在查询条件中使用limit关键字来指定返回查询结果的起始下标，以及返回的数量限制

  .. code-block:: json

    {
        "Raw": [
            [ ],
            {
                "name":"xxx"
            }, 
            {
                "$limit":{
                    "total":10,
                    "index":0
                }
            },
            {
                "$order":[{"id":1},{"name":-1}]
            }
        ]
    }  

表示的意义为:

.. code-block:: sql

    select * from xxx where name='张三' order by id ASC, name desc limit 0,10;

运算符
*****************

比较运算符(comparison operators)
````````````````````````````````````````````````````````


============   ===========================     =========================================
运算符	        说明	                           语法
============   ===========================     =========================================
$ne             不等于                           {field:{$ne:value}}
$eq	            等于                            	{field:{$eq:value}} or {field:value}
$lt	           小于	                            {field:{$lt:value}}
$le	           小于等于	                             {field:{$le:value}}
$gt	           大于	                            {field:{$gt:value}}
$ge	           大于等于	                           {field:{$ge:value}}
$in	            字段值在指定的数组中	                 {field:{$in:[v1, v2, ..., vn]}}
$nin	         字段值不在指定的数组中	                 {field:{$nin:[v1, v2, ..., vn]}}
============   ===========================     =========================================


逻辑运算符(logical operators)
````````````````````````````````````````````````````````


============   ===========     =========================================
逻辑符	        说明	                           语法
============   ===========     =========================================
$and	          逻辑与	             {$and:[{express1},{express2},...,{expressN}]}
$or           	逻辑或                {$or:[{express1},{express2},...,{expressN}]}
============   ===========     =========================================


示例
````````````````````````````````````````````````````````

.. code-block:: javascript

  where id > 10 and id < 100
  // 对应 json 对象
    {
      $and: [
        {
          id: {$gt: 10}
        },
        {
          id: {$lt: 100}
        }
      ]
    }

.. code-block:: javascript

  where id > 10 and id < 100
  //对应 json 对象
  {
    $and: [
      {
        id: {$gt:10}
      },
      {
        id: {$lt:100}
      }
    ]
  }

.. code-block:: javascript

  where name = 'peersafe' or name = 'zongxiang'
  //对应 json 对象
  {
    $or: [
      {
        name: {$eq:'peersafe'}
      },
      {
        name: {$eq:zongxiang}
      }
    ]
  }

.. code-block:: javascript

  where (id > 10 and name = 'peersafe') or name = 'zongxiang'
  //对应 json 对象
  {
    $or: [
      {
        $and: [
          {
            id: {$gt:10}
          },
          {
            name:'peersafe'
          }]
      },
      {
        name:'zongxiang'
      }
    ]
  }

模糊匹配(fuzzy matching)
**********************************************


=========================================     =========================================
语法	                                           说明	                           
=========================================     =========================================
{"field":{"$regex":"/value/"}}	                like "%value%"
{"field":{"$regex":"/^value/"}}	                like "%value"
=========================================     =========================================


.. code-block:: javascript

  where name like '%peersafe%'
  //对应 json 对象
  {
    name: {
      $regex:'/peersafe/'
    }
  }

.. code-block:: javascript

  where name like '%peersafe'

  //对应 json 对象

  {
    name: {
      $regex:'/^peersafe/'
    }
  }


