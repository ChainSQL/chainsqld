ChainSQL
========

.. image:: ../../../images/logo.png
    :width: 301px
    :alt: ChainSQL logo
    :align: center

ChainSQL是全球首个基于区块链的数据库应用平台，基于开源项目Ripple建立，众享比特公司负责日常维护开发。

设计目的
    在不改原有项目整体结构的前提下，在逻辑层与数据层之间加入区块链，使得对数据库的操作记录不可更改、可追溯，并且与传统数据库相关项目对接比较方便。

功能
    2016年初开始开发， 目前最新版本是0.30.3，支持功能包括：

- 货币发行、转账、交易所
- 数据库表操作
- solidity语言编写的智能合约

系统架构

    .. image:: ../../../images/ChainSQL.png
        :width: 626px
        :alt: ChainSQL Framework
        :align: center

.. toctree::
   :maxdepth: 2
   :caption: 入门教程

   tutorial/commonSense
   tutorial/deploy

.. toctree::
    :maxdepth: 2
    :caption: 原理

    theory/tableDesign
    theory/smartContractDesign
    theory/cfg
    theory/amendments

.. toctree::
    :maxdepth: 2
    :caption: 功能介绍

    functions/recordLevel
    functions/raw
    
..
    functions/currency
    functions/tableOperation
    functions/smartContract

.. toctree::
   :maxdepth: 2
   :caption: 交互

   interface/cmdLine
   interface/rpc
   interface/websocket
   interface/javaAPI
   interface/nodeAPI

.. toctree::
   :maxdepth: 1
   :caption: 运维相关

   maintenance/errCode
   maintenance/QA
