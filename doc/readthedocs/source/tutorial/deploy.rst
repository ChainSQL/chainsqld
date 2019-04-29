======================
ChainSQL网络搭建
======================


版本获取
==============
ChainSQL 的节点程序可在 `Github开源仓库 <https://github.com/ChainSQL/chainsqld/releases>`_ 获取到，有 windows,linux 两个版本的可执行程序。


软硬件环境要求
==============

    .. image:: ../../../../images/environment.png
        :width: 600px
        :alt: ChainSQL Framework
        :align: center


数据库安装配置（可选）
===============================

.. NOTE::
    ChainSQL中的交易首先发到链上共识，共识过后如果配置了数据库，并且开启auto_sync，会去自动同步交易对应的sql操作，如果不配置数据库，交易也只是存在于链上，可以在想同步链上交易时再

1. 安装mysql
-------------------------

在需要安装mysql数据库的节点上按照提示安装mysql 以ubuntu 16.04为例，安装配置步骤如下：

.. code-block:: bash

	sudo apt-get install mysql-server

如果apt-get install不成功，可以选择 安装过程中会提示设置密码，要记下密码，在后面的配置文件中会用到。

2.检查是否安装成功
-------------------------
检查是否安装成功::

	mysql --version

能查询到mysql版本号则表示安装成功。 

检查是否能正常登录:

.. code-block:: bash

	mysql -uroot –p

上面命令输入之后会提示输入密码，此时正确输入密码就可以登录到mysql。

3.	创建数据库并支持utf8编码
------------------------------------------
登入mysql 后，创建名字为chainsql的database：

.. code-block:: sql

	CREATE DATABASE IF NOT EXISTS chainsql DEFAULT CHARSET utf8 

也可以将mysql的默认编码设置为utf8，然后直接创建数据库

.. code-block:: sql

	create database chainsql;

设置mysql 默认UTF8编码:
修改/etc/mysql/mysql.conf.d/mysqld.cnf文件

``[mysqld]`` 下添加：::

	character_set_server = utf8

然后在配置文件最后添加如下配置：::

	[mysql.server]
	default-character-set = utf8
	[client]
	default-character-set = utf8

然后重启mysql：::

	/etc/init.d/mysql restart

确认是否为utf8编码：

.. code-block:: sql

	show variables like 'character%';

显示如下图则认为database是utf8编码

::

	+-------------------------------+----------------------------+
	| Variable_name                 | Value                      |
	+-------------------------------+----------------------------+
	| character_set_client  	| utf8                       |
	| character_set_connection	| utf8                       |
	| character_set_database   	| utf8                       |
	| character_set_filesystem 	| binary                     |
	| character_set_results    	| utf8                       |
	| character_set_server     	| utf8                       |
	| character_set_system     	| utf8                       |
	| character_sets_dir       	| /usr/share/mysql/charsets/ |



4.	最大连接数设置（可选）
---------------------------------------
.. code-block:: sql

	show variables like '%max_connections%';

| 默认是151， 最大可以达到16384。修改方法有两种。
| 第一种，命令行修改：

.. code-block:: sql
	
	set GLOBAL max_connections = 10000;

| 这种方式有个问题，就是设置的最大连接数只在mysql当前服务进程有效，一旦mysql重启，又会恢复到初始状态。

| 第二种，修改配置文件：

| 这种方式也很简单，只要修改MySQL配置文件my.cnf的参数max_connections，
| 将其改为max_connections=10000，然后重启MySQL即可。

区块链网络搭建
===============================

需要至少 4 个验证节点，每个验证节点需要生成public key和seed。

下面以一个验证节点为例进行说明，想要得到更多节点，重复以下步骤即可。

1.	验证节点公私钥的生成
----------------------------
将可执行程序与配置文件（如chainsqld-example.cfg）放在用户目录，先启动一下：

.. code-block:: bash

    nohup ./chainsqld --conf="./chainsqld-example.cfg"&

确认chainsqld程序已经启动，输入 ``ps -ef|grep chainsqld`` ，看是否列出chainsqld进程

.. WARNING::
    在0.30.3版本之前，执行这一命令要提前启动chainsqld进程，是因为下面的validation_create命令要向进程发送rpc请求，如果进程启动不成功，命令会返回错误。0.30.3及之后的版本可以不启动chainsqld程序直接返回结果。

生成 ``validation_public_key`` 及 ``validation_seed`` , 输入:

.. code-block:: bash

    ./chainsqld --conf="./ chainsqld-example.cfg"  validation_create
    
返回结果如下：

.. code-block:: json

    {
        "status" : "success",
        "validation_key" : "TUCK NUDE CORD BERN LARD COCK ENDS ETC GLUM GALE CASK KEG",
        "validation_public_key" : "n9L9BaBQr3KwGuMoRWisBbqXfVoKfdJg3Nb3H1gjRSiM1arQ4vNg",
        "validation_seed" : "xxjX5VuTjQKvkTSw6EUyZnahbpgS1"
    }

.. IMPORTANT::

    如果配置文件在当前目录，且名称为 ``chainsqld.cfg``  ，可以不用加 ``--conf`` 指定配置文件路径，直接运行 ``nohup ./chainsqld &`` 命令即可启动节点。

2.	配置文件的修改
---------------------------
以下仅针对部分字段进行说明，针对配置文件的详细说明参考 :ref:`配置文件详解 <配置文件>` 。

``[sync_db]``

  配置ip，port，db，mysql安装时设置的(user,pass)等。

  Chainsql中的事务与行级控制要求每个节点必须配置数据库，如果用不到这两个特性，也可以选择只在需要查看数据的节点配置数据库。

  例如::

	[sync_db]
	type=mysql
	host=localhost
	port=3306
	user=root
	pass=root
	db=chainsql
	first_storage=0
	unix_socket=/var/lib/mysql/mysql.sock

.. note::

	使用localhost连接时，会默认使用 ``sock`` 方式连接，默认sock路径是 ``/var/run/mysqld/mysqld.sock`` 在非ubuntu系统中，这个路径是不对的，会导致连接数据库失败，需要用 ``unix_socket`` 选项来指定 ``sock`` 路径，如果用ip去连接，会使用 ``tcp`` 方式连接，就不会有这个问题

``[node_db]``

- windows平台: type=NuDB
- Ubuntu平台: type=RocksDB

``[ips_fixed]``

  chainsql始终尝试进行对等连接的IP地址或主机名（其它三个节点的ip及端口号5123）。

例如：::

	[ips_fixed]
	127.0.0.1 51236
	127.0.0.1 51237
	127.0.0.1 51238

``[validators]`` 或 ``[validators_file]``

  添加其他(三个)节点的 ``validation_public_key`` ；

例如：::

	[validators]
	n9MRden4YqNe1oM9CTtpjtYdLHamKZwb1GmmnRgmSmu3JLghBGGJ
	n9Ko97E3xBCrgTy4SR7bRMomytxgkXePRoQUBAsdz1KU1C7qC4xq
	n9Km65gnE4uzT1V9L7yAY9TpjWK1orVPthCkSNX8nRhpRaeCN6ga

``[validation_public_key]``

  添加本节点的validation_public_key。此字段可不配置，但方便后续查阅，建议配置。

例如：::

	[validation_public_key]
	n9Jq6dyM2jbxspDu92qbiz4pq7zg8umnVCmNmEDGGyyJv9XchvVn

``[validation_seed]``

  添加本节点的 ``validation_seed`` 。只有验证节点需要配 ``validation_seed`` ，普通节点不需要这一配置。

例如：::

	[validation_public_key]
	n9Jq6dyM2jbxspDu92qbiz4pq7zg8umnVCmNmEDGGyyJv9XchvVn
	[validation_seed]
	xnvq8z6C1hpcYPP94dbBib1VyoEQ1

``[auto_sync]``

auto_sync配置为1表示开启表自动同步，开启后，在节点正常运行的情况下，新建表会自动入同步到数据库。

如果不想自动同步，只想同步需要同步的表，使用 ``sync_tables`` 配置项。

``[sync_tables]``::

	[sync_tables]
	zBUunFenERVydrqTD3J3U1FFqtmtYJGjNP tablename
	zxryEYgWvpjh6UGguKmS6vqgCwRyV16zuy tablename2

配置格式：

- 非加密表格式：	建表账户 表名
- 加密表格式：		建表账户 表名 可解密账户私钥

3.	架设网络
---------------------------
启动chainsqld程序
进入chainsqld应用程序目录，执行下面的命令::

	nohup ./chainsqld &

每个网络节点均要执行上述命令，使chainsql服务在后台运行。

检查是否成功
进入chainsql应用程序目录，执行命令::

	watch ./chainsqld server_info

若输出结果中，字段 ``complete_ledgers``  :类似 "1-10"，则chainsqld服务启动成功
每个网络节点的chainsql服务都要求成功运行

查看其它节点的运行情况：::

	watch ./chainsqld peers

链重启/节点重启
节点全部挂掉的情况：

- 如果想要清空链，将 ``db,rocksdb/NuDb`` 文件夹清空，然后重新执行节链启动过程；
- 如果想要加载之前的区块链数据启动，在某一全节点下执行下面的命令::

	nohup ./chainsqld --load &

其它节点执行：::

	nohup ./chainsqld &

这样即可加载原来的数据启动链

还有节点在运行的情况

只要网络中还有节点还在跑，就不需要用 ``load`` 方式重启链，只需要启动挂掉的节点即可：::

		nohup ./chainsqld &

4.退出终端
---------------------------
在终端输入 ``exit`` 退出，不然之前在终端上启动的chainsqld进程会退出