# Ubuntu 编译
## 系统需求
- 建议 Ubutun 16.04 及以上版本

## 注:
Ubuntu默认是自动升级的，16.04版本自 2018年7月份自动升级后，chainsql在ubuntu下无法链接通过，错误提示：
```
/usr/local/openssl-1.0.2/lib/libcrypto.a(err.o): In function `ERR_remove_thread_state':
err.c:(.text+0x1ac0): multiple definition of `ERR_remove_thread_state'
/usr/lib/x86_64-linux-gnu/libmysqlclient.a(ssl.cpp.o):(.text+0x1df0): first defined here
```
目前暂时无好的解决方案，只有替换回旧的.a文件，可成功编译的libmysqlclient.a文件已放至 Builds/Ubuntu目录下，替换可编译通过

## 安装工具
- sudo apt-get install git

## 下载源码
- git clone git@github.com:ChainSQL/chainsqld.git chainsqld

## 安装开发环境依赖
- 进入编译目录
```
> cd Builds/Ubuntu
```
- 分别执行下面指令
```
> sudo ./install_rippled_depends_ubuntu.sh
> ./install_boost.sh
```
- 在 ~/.bashrc 文件中保存 BOOST_ROOT 环境变量

```
export BOOST_ROOT=/home/dbliu/work/chainSQL/Builds/Ubuntu/boost_1_63_0
```
- 让 BOOST_ROOT 环境变量生效
```
> source ~/.bashrc
```

## 编译 chainsqld
- 重新进入源码根目录
```
> cd ~/work/chainsqld
```
- 执行编译指令
```
> scons --static --enable-mysql -j2
```
- 编译成功后，chainsqld 程序在 build 目录下
