macOS-Xcode 编译指南
## 重要提示
我们并不推荐使用OS X来编译运行chainsqld工程。根据当前的测试数据来看，使用Ubuntu平台能获得最高的性能保障。
## 前置条件
OSX 10.8及更高版本

需提前准备好下列软件：<br>
- [XCode](https://developer.apple.com/xcode/)
- [HomeBrew](https://brew.sh/)
- [Git](https://git-scm.com/)
- [SCons](http://www.scons.org/)
- [MySQL](https://www.mysql.com/)

## 软件安装
**安装XCode**<br>
如果你已经安装XCode，请直接跳过此步骤。<br>
XCode可直接通过AppStore安装或直接点击此[安装链接](https://developer.apple.com/xcode/).<br>
命令行工具可在终端输入如下命令安装：
```
xcode-select --install
```

**安装HomeBrew**<br>
如果已安装HomeBrew请跳过此步骤。<br>
安装HomeBrew，需要先安装Ruby，安装Ruby的的方法可参考[这里](http://blog.csdn.net/u012701023/article/details/52183100).<br>
安装完之后在命令行运行如下命令：
```
/usr/bin/ruby -e "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/master/install)"
```

**安装Git**
```
brew install git
```

**安装Scons**<br>
Scons版本最低要求是2.3.0，brew默认安装最新版本的软件，所以通过brew安装将会满足chainSQL对Scons的最低版本要求。
```
brew install scons 
```

**安装MySQL**<br>
```
brew install mysql
```

## 依赖及环境变量设置

**安装Package Config**<br>
```
brew install pkg-config
```

**安装cmake**<br>
```
brew install cmake
```

**安装Google Protocol Buffers编译环境**<br>
```
brew install protobuf
```

**安装OpenSSL**<br>
```
brew install openssl
```

**安装Boost环境**<br>
通过brew安装boost库：
```
brew install boost
```
然后将BOOST_ROOT设置为环境变量，具体方法为：<br>
1.进入当前用户home目录<br>
2.文本编辑器打开“.bash_profile”文件<br>
3.在文件最后添加如下语句(其中等号后面的路径为boost文件夹所在目录，可以根据brew安装完之后的提示获得路径，也可以去“/usr/local/Cellar/boost”路径下查找)：
```
export BOOST_ROOT=/usr/local/Cellar/boost/boost版本
```
4.执行如下命令使更改生效：
```
source .bash_profile
```
5.最后可以执行“echo $BOOST_ROOT”检测设置是否生效

## 克隆chainSQL代码库

在终端执行如下命令：
```
git clone git@github.com:ChainSQL/chainsqld.git
```

## 创建chainSQL-Xcode项目文件
进入chainSQL代码库根目录，然后进入“Builds/XCode”目录，执行下面命令：
```
cmake -G Xcode ../..
```
之后会在“Builds/XCode”目录下生成xcode项目文件：chainsqld.xcodeproj，然后双击即可打开Xcode。
可通过选择“Product/Scheme”下面的chainsqld或者chainsqld_classic或者ALL_BUILD来选择不同的编译方式生成chainsqld。
