
############
QA
############

Api 调用问题
---------------

1. 两笔挂单，分别是买单与卖单，兑换比例一致，但未成交
    原因：网关未开启 `Rippling <https://developers.ripple.com/rippling.html>`_

2. Insufficient reserve to create offer
    原因：挂单时Zxc余额不满足保留费用要求，需要最终满足::
    
        ZxcBalance >= AccountReserve(5) + AccountObjectCount*1

3. Fee of xxx exceeds 
    原因：不在交易中填写Fee会自己去算，而负载高时，算出来的Fee比较大，>100时，会报这个错误。所以建议自己在交易中填上Fee的值。

4. 对表进行操作，无论什么交易都是db_timeout
    原因：表的某个区块没有同步到，由于某种原因被跳过了，然后后面的交易，PreviousTxnLgrSeq总是无法与当前的对上
    目前暂未发现导致的根本原因，只能把LedgerSeq重新置为TxnLedgerSeq:

.. code-block:: sql

    update SyncTableState as t1,(select * from SyncTableState where 
        Owner='zDmdwbqbtcxPcj9ytzuKyQLPQi2DuWXmgk' and deleted=0) as t2 set 
        t1.LedgerSeq=t2.TxnLedgerSeq where t1.TableNameInDB=t2.TableNameInDB;

5. Insufficient reserve to create offer   
    众享币不足  需要预留对象+5  查询对象 accountObject

6. Auth for unclaimed account needs correct master key 
    账户与签名私钥不一致

7. Current user doesn\'t have this auth   
    用户没有信任这个货币

8. Ledger sequence too high / tefMAX_LEDGER
    这个是因为发送交易的时候，用API发送交易时，会在交易中带一个LastLedgerSequence，当交易发送至节点时，发到节点的时候，发现当前区块号已经大于LastLedgerSequence，就会报这个错误。
    一般是客户端调试引起的。

Chainsql节点问题
-----------------

1. 客户端频繁发请求，过一段时间就发不了了，直接丢包
    输出如下：

::

    2018-Dec-05 02:45:23 Resource:WRN Consumer entry 114.242.47.14 dropped with balance 525166 at or above drop threshold 15000
    2018-Dec-05 02:45:23 Resource:WRN Consumer entry 114.242.47.14 dropped with balance 525260 at or above drop threshold 15000
    2018-Dec-05 02:45:23 Resource:WRN Consumer entry 114.242.47.14 dropped with balance 525354 at or above drop threshold 15000
    2018-Dec-05 02:45:23 Resource:WRN Consumer entry 114.242.47.14 dropped with balance 525447 at or above drop threshold 15000

这是因为ripple本身不允许一个ip频繁发请求，认为这是在攻击，如果一个已知ip要这样做，需要把它放到节点的admin列表中::

    [port_ws_public]
    port = 5006
    ip = 0.0.0.0
    protocol = ws
    admin=101.201.40.124,59.110.154.242,114.242.47.102

然后重启节点

2. 启动节点报：make db backends error,please check cfg!
    数据库连接失败，请检查配置文件中数据库(sync_db)的配置

3. cfg文件中配置项不起作用
    注释只能写在开头，中间写注释会导致配置项不起作用（#）

4. 如何升级chainsql节点
    一般升级chainsql节点只需要挨个节点替换重启即可，步骤如下：

    1. 停掉一个正在运行的节点（先用 ``./chainsqld stop`` 命令，如果停不掉再用 ``kill`` 命令杀进程）
    2. 替换新的chainsqld可执行程序
    3. 启动chainsqld进程
    4. 查看 ``server_info``，直到 ``completed_ledgers`` 正常出块
    5. 依次对所有节点执行1-4过程

5. 节点全部挂掉，找不到原因
    | 使用secureCRT或者Xshell连接服务器，退出时，直接关闭对话窗口，会将nohup后台运行的进程杀死。
    | 应该使用 ``exit`` 命令退出 ssh 工具终端