## 一、数据库安装配置
### 1. 安装mysql
在需要安装mysql数据库的节点上按照提示安装mysql
以ubuntu 14.04为例，安装配置步骤如下： 
```
sudo apt-get install mysql-server
```
如果apt-get install不成功，可以选择
安装过程中会提示设置密码，要记下密码，在后面的配置文件中会用到。
### 2.检查是否安装成功
1)	检查是否安装成功：<br>
```
mysql --version
```
能查询到mysql版本号则表示安装成功。
2)	检查是否能正常登录：
```
mysql -uroot –p
```
-u 表示选择登录的用户名<br>
-p 表示登录的用户密码<br>
上面命令输入之后会提示输入密码，此时正确输入密码就可以登录到mysql。
### 3.	创建数据库并支持utf8编码
登入mysql 后：
1)	创建名字为chainsql的database：
```
CREATE DATABASE IF NOT EXISTS chainsql DEFAULT CHARSET utf8 
```
2)	检查是否创建成功
```
show databases;
```
在databases中显示有chainsql字样，则为创建成功。
3)	确认是否为utf8编码：
```
show variables like 'character%';
```
显示如下图则认为database是utf8编码

```
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
```
### 4.	最大连接数设置（可选）
show variables like '%max_connections%';<br>
默认是151：<br>
 
修改配置命令如下：<br>
set GLOBAL max_connections = 10000;<br>
	
## 二、区块链网络搭建
需要至少 4 个验证节点，每个验证节点需要生成public key和seed。
### 1.	验证节点的生成
1)	将可执行程序放在目录/opt/chainsql/bin，将配置文件放在目录/opt/chainsql/etc
2)	进入/opt/chainsql/bin目录，输入:
```
sudo ./chainsqld --conf="/opt/chainsql/etc/chainsqld.cfg"
```
3)	确认chainsqld程序已经启动，输入ps –ef | grep chainsqld，看是否列出chainsqld进程
4)	生成validation_public_key及validation_seed, 输入:<br>
```
sudo ./chainsqld --conf="/opt/chainsql/etc/chainsqld.cfg"  validation_create
```
5)	返回结果如下：
```
{
   "status" : "success",
   "validation_key" : "TUCK NUDE CORD BERN LARD COCK ENDS ETC GLUM GALE CASK KEG",
   "validation_public_key" : "n9L9BaBQr3KwGuMoRWisBbqXfVoKfdJg3Nb3H1gjRSiM1arQ4vNg",
   "validation_seed" : "xxjX5VuTjQKvkTSw6EUyZnahbpgS1"
}
```
6)	分别在4个节点进行同样的操作，得到各自的validation_public_key及validation_seed

### 2.	配置文件的修改
所有节点的chainsqld.cfg都要进行下面的修改：
1)	字段[ips]，添加其它三个节点的ip及端口号5123，如下例所示：
```
[ips]
139.129.99.7 5123
101.201.40.124 5123
139.224.0.105 5123
```

2)	字段[validation_seed]，添加本节点的validation_seed，如下例所示：
```
[validation_seed]
xxjX5VuTjQKvkTSw6EUyZnahbpgS1
```
3)	字段[validation_public_key]，添加本节点的validation_public_key，如下例所示：
```
[validation_public_key]
n9L9BaBQr3KwGuMoRWisBbqXfVoKfdJg3Nb3H1gjRSiM1arQ4vNg
```
4)	字段 [validators]，添加另外三个节点的validation_public_key，如下例所示：
```
[validators]
n9KdidFafxRB4izPH1tdLFpBjVboQQy7MXjC8SvLvD1wsahmGc2E
n9KdidFafxRB4izPH1tdLFpBjVboQQy7MXjC8SvLvD1wsahmGc2E
n9LrzPopoh3CUiJx7AFRaCFoy4t3RafAhyoYEeYWhkMb5R7Z19oL
```
5) mysql数据库配置：<br>

字段[sync_db]中pass变量要设置为mysql安装数据库时设置的密码
```
[sync_db]
type=mysql
host=localhost
port=3306
user=root
pass=mypass
db=chainsql
first_storage=0
charset=utf8
```
**注**：Chainsql中的事务与行级控制要求每个节点必须配置数据库，如果用不到这两个特性，也可以选择只在需要查看数据的节点配置数据库

## 3.	架设网络 　　
1)	启动chainsqld程序
进入chainsqld应用程序目录：/opt/chainsql/bin，执行下面的命令
```
sudo nohup ./chainsqld  -q  --conf="/opt/chainsql/etc/chainsqld.cfg"&
```
每个网络节点均要执行上述命令，使chainsql服务在后台运行。
2)	检查是否成功<br>
进入chainsql应用程序目录：/opt/chainsql/bin，执行下面的命令
```
sudo watch ./chainsqld  -q  --conf="/opt/chainsql/etc/chainsqld.cfg" server_info
```
若输出结果中，字段"complete_ledgers" :类似 "1-10"，则chainsqld服务启动成功<br>
每个网络节点的chainsql服务都要求成功运行
