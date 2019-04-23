.. _amendments:

特性-修订
===============


背景
------------
特性( ``Feature`` )/修订( ``Amendment`` )是Ripple中为了在去中心化网络中 ``同步地`` 引入新功能或修改某些共识规则的机制。

    - 对交易处理的任何更改都可能导致服务器使用同一组交易构建不同的账本。
    - 如果某些验证节点（参与共识的rippled服务器）已升级到新版本的软件，而其他验证节点使用旧版本，则这可能会导致任何问题，从小的不便到完全中断。

修订解决了这个问题，只有足够的验证节点支持这些功能时才能启用新功能。Amendment的特点如下：

- Amendment需要得到整个网络中80%的支持并且持续两周才能生效
- Amendment一旦生效，后面的区块中会一直生效，要想禁用某一已经生效的Amendment，需要引入新的Amendment才能实现。

有关已知修订的完整列表，其状态和ID，请参阅：`Ripple中的已知修订 <https://developers.ripple.com/known-amendments.html>`_。

.. note:: 
    只有与共识相关的相关修改需要使用修订来升级，对于不影响共识的逻辑修改（如查询结果的返回格式），是不需要用修订来实现的。

使用方式
------------
1. 在cfg中配置Amendment
**************************************
::

    //1. 强制生效（一般用于开发测试，在单机模式下使用），这里只是配置当前节点启用特性
    [features]
    MultiSign
    TrustSetAuth

    //2.投反对票
    [veto_amendments]
    C1B8D934087225F509BEB5A8EC24447854713EE447D277F69545ABFA0E0FD490 Tickets
    DA1BD556B42D85EA9C84066D028D355B52416734D3283F85E216EA5DA6DB7E13 SusPay

.. note:: 
    | 在 ``features`` 中配置的特性，只是让当前节点临时生效，在所有共识节点都配置了同样的 ``features`` 时，这一特性相当于临时在全网生效。
    | 要想让特性在链上真正生效，需要用第二种方式。

2. 自动生效
************************************
    在当前最新区块后的第一个标志区块（256的整数倍），会自动给节点识别的特性投赞成票，超过80%的赞成票且维持两周则在链中永久生效  

3. 查询特性启用情况
************************************
查询命令命令::
    
    ./chainsqld feature

查询结果示例:

.. code-block:: json

    {
        "id":1,
        "result":{
            "features":{
                "07D43DCE529B15A10827E5E04943B496762F9A88E3268269D69C44BE49E21104":{
                    "enabled":true,
                    "name":"Escrow",
                    "supported":true,
                    "vetoed":false
                },
                "08DE7D96082187F6E6578530258C77FAABABE4C20474BDB82F04B021F1A68647":{
                    "enabled":true,
                    "name":"PayChan",
                    "supported":true,
                    "vetoed":false
                },
                "1562511F573A19AE9BD103B5D6B9E01B3B46805AEC5D3C4805C902B514399146":{
                    "enabled":true,
                    "name":"CryptoConditions",
                    "supported":true,
                    "vetoed":false
                },
                "1D3463A5891F9E589C5AE839FFAC4A917CE96197098A1EF22304E1BC5B98A454":{
                    "enabled":true,
                    "name":"fix1528",
                    "supported":true,
                    "vetoed":false
                },
                "3012E8230864E95A58C60FD61430D7E1B4D3353195F2981DC12B0C7C0950FFAC":{
                    "enabled":true,
                    "name":"FlowCross",
                    "supported":true,
                    "vetoed":false
                },
                "37D94ABEB2C32B9BE9846EB023D8EFFF7607BFFB1857D1365B5807E1C8EA318D":{
                    "enabled":true,
                    "name":"DisableV2",
                    "supported":true,
                    "vetoed":false
                },
                "42426C4D4F1009EE67080A9B7965B44656D7714D104A72F9B4369F97ABF044EE":{
                    "enabled":true,
                    "name":"FeeEscalation",
                    "supported":true,
                    "vetoed":false
                }
            },
            "status":"success"
        }
    }

实现原理
------------

关于修订
*************

修订是一个全新的功能或功能的变化，等待对等网络启用，作为共识流程的一部分。一个rippled服务器对于修订有两种模式：

1. 不支持该修订（旧的行为）
2. 支持修订（新的行为）

每项修订都有一个唯一的标识十六进制值和一个简称。简称目的是使人看起来容易辨认，并未在修改过程中使用。两台服务器可以支持相同的修订ID，同时使用不同的名称来描述它。修正案的名称不保证是唯一的。

按照惯例，Ripple的开发人员使用修订名称的SHA-512Half散列作为修订ID。

生效过程
******************
每个第256个区块都称为 ``标志`` 区块。审批修订的过程始于标志账本之前的区块版本，当rippled验证节点服务器发送该账本的验证消息时，这些服务器也会提交投票以支持特定的修改。如果验证节点不赞成修正案，则对修正案投反对票。

| 在标志区块上，服务器会查看他们信任的验证节点的投票，并决定是否将 ``EnableAmendment`` 伪交易注入到以下账本中。
| ``EnableAmendment`` 伪交易的标志显示服务器认为发生了什么：

1. ``tfGotMajority`` 标志意味着对修改的支持已经增加到至少80％的可信验证节点。
2. ``tfLostMajority`` 标志意味着对修订的支持减少到不到80％的可信验证者。
3. 没有标志的 ``EnableAmendment`` 伪交易意味着两周时间已到，要正式启用对修改的支持。

| 对于 ``tfGotMajority`` 标志的修订，后面的 ``标志`` 区块不会重复注入伪交易，直到时间满足两周，会再注入一个针对此修订的无标志的伪交易。
| 共识类（Change类）中进行共识过程时，发现伪交易中未设置标志，则正式启用该修订。

节点不升级导致服务不可用
********************************
当修订在整个链上启用后，未升级的节点会因为不再了解网络规则而停止服务，所有发到这一节点上的请求都会返回  ``amendmentBlocked`` 错误:

.. code-block:: json

    {
        "result":{
            "error":"amendmentBlocked",
            "error_code":14,
            "error_message":"Amendment blocked,need upgrade.",
            "request":{
                "command":"submit",            
                "tx_blob":"1200002280000000240000001E61D4838D7EA4C6800000000000000000000000000055534400000000004B4E9C06F24296074F7BC48F92A97916C6DC5EA968400000000000000B732103AB40A0490F9B7ED8DF29D246BF2D6269820A0EE7742ACDD457BEA7C7D0931EDB7447304502210095D23D8AF107DF50651F266259CC7139D0CD0C64ABBA3A958156352A0D95A21E02207FCF9B77D7510380E49FF250C21B57169E14E9B4ACFD314CEDC79DDD0A38B8A681144B4E9C06F24296074F7BC48F92A97916C6DC5EA983143E9D4A2B8AA0780F682D136F7A56D6724EF53754"
            },
            "status":"error"
        }
    }

服务器将：

- 无法确定账本的有效性
- 无法提交或处理交易
- 无法参与共识流程
- 无法对将来的修改进行投票

| 成为修订被阻止（ ``amendmentBlocked`` ）是Ripple的是一项安全功能，用于保护依赖于Ripple共识的应用程序。
| 如果您的服务器遭到修改阻止，您必须升级到新版本才能与网络同步。