# windows 编译 
## 必备工具和开发库
- [Visual studio 2015](README.md#install-visual-studio-2015)
- [Git for windows](README.md#install-git-for-windows)
- [cmake](README.md#install-cmake-for-windows)
- [Goole Protocol Buffers Complier](README.md#install-protocol)
- [OpenSSL](README.md#install-openssl)
- [Boost library](README.md#install-boost)
- [mysql](README.md#install-mysql)

## 安装工具
### install visual studio 2015
> [Visual Studio 2015 Download](https://www.visualstudio.com/downloads/download-visual-studio-vs) 

### install git for windows
> [Git for windows](https://git-scm.com/downloads)

### install cmake for windows
> [cmake for windows](https://cmake.org/download/)

### install protocol
> 1. 编译 chainsqld 需要 2.5.1 以上版本的 [protoc.exe](https://ripple.github.io/Downloads/protoc/2.5.1/protoc.exe)

> 2. 将 protoc.exe 所在目录加入至 PATH 环境变量中

### install openssl
> 1. 下载 [Win64 OpenSSL](http://slproweb.com/products/Win32OpenSSL.html)
> 2. 安装完 Open SSL 后，并将 OPENSSL_ROOT_DIR 环境变量设置为 OpenSSL 安装路径

### install boost
> 1. 下载 [boost](http://slproweb.com/products/Win32OpenSSL.html)
> 2. 编译 boost
```cmd
> cd C:\lib\boost_1_62_0
> bootstrap
> bjam --toolset=msvc-14.0 address-model=64 architecture=x86 link=static threading=multi runtime-link=shared,static stage --stagedir=stage64
```
> 3. 将 BOOST_ROOT 环境变量设置为 C:\lib\boost_1_62_0

### install mysql
> 1. 下载 [libmysqlclient for windwos](https://dev.mysql.com/downloads/connector/c/)。建议优先选择 x86，64-bit 的压缩包。
> 2. 将 MYSQL_ROOT_DIR 环境变量设置为 libmysqlclient 的安装目录

## 编译 chainsqld
### 下载源码
- git clone git@github.com:ChainSQL/chainsqld.git chainsqld

### 生成 chainsqld 解决方案
> 1. 打开 Developer Command Prompt for VS2015
> 2. 切换 Command Prompt 的工作目录
```
> cd E:/work/chainsqld/Builds/VisualStudio2015
```

> 3. 执行 cmake，target 可以为 msvc.debug.unity/msvc.debug.nounity/msvc.release.unity/msvc.release.nonity/msvc.debug/msvc.release

```
# E:/work/chainsqld 为源码根目录
> cmake -G"Visual Studio 14 2015 Win64"  -Dtarget=msvc.debug.unity E:/work/chainsqld
```

### 编译 chainsqld
> 1. 进入 Builds/VisualStudio2015 目录
> 2. 打开 chainsqld.sln
> 3. 如果要运行测试用例，编译 chainsqld-classic 工程；否则编译 chainsqld 工程
> 4. 编译成功后 chainsqld 实例编译至 build/msvc.debug.unity/Debug 目录下，运行 chainsqld 实例前需要将 MYSQL_ROOT_DIR/lib 下的 libmysql.dll 拷贝至此 Debug 目录下

## 调试 chainsqld
> 1. 创建一个工作目录用于存储配置文件 chainsqld.cfg，如
```
E:/work/chainsqld/build/chainsqld.cfg
```
> 2. 使用 --conf 参数指定 chainsqld 的配置文件，如

![Visual Studio 2013 Command Args Prop Page](images/VSCommandArgsPropPage.png)

> 3. chainsqld 在使用 Windows Debug Heap 调试程序的时候会导致运行非常慢。我们可以设置 _NO_DEBUG_HEAP 环境变量禁用 Debug Heap，如

![Visual Studio 2013 No Debug Heap Prop Page](images/NoDebugHeapPropPage.png)

## 运行测试用例
> 1. 需要编译 chainsqld_classic 工程
> 2. chainsqld.exe --unittest="Transaction2Sql" --unittest-arg="conf=E:\work\chainsqld\build\chainsqld.cfg"
