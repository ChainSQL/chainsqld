# Ubuntu 编译
## 系统需求
- 建议 Ubutun 16.04 及以上版本

## 需特殊注意的系统依赖项版本
| Component | Minimum Recommended Version |
|-----------|-----------------------|
| gcc | 7.4.0 |
| cmake | 3.12 |
| boost | 1.70.0 |



## 下载源码
- git clone git@github.com:ChainSQL/chainsqld.git chainsqld

## 安装开发环境依赖
- 进入编译目录
```bash
> cd Builds/Ubuntu
```
- 分别执行下面指令
```bash
> sudo ./install_chainsqld_depends_ubuntu.sh
> ./install_boost.sh
```

- 在 ~/.bashrc 文件中保存 BOOST_ROOT 环境变量

```bash
export BOOST_ROOT=/home/dbliu/work/chainSQL/Builds/Ubuntu/boost_1_70_0
```
- 让 BOOST_ROOT 环境变量生效
```bash
> source ~/.bashrc
```

### cmake版本升级
- 检查cmake版本，如果版本号在3.12以下，则需要升级
1. 卸载已经安装的旧版的CMake（非必需）
```
sudo apt-get autoremove cmake
```
2. 文件下载解压：
```
wget https://cmake.org/files/v3.12/cmake-3.12.2-Linux-x86_64.tar.gz
tar zxvf cmake-3.12.2-Linux-x86_64.tar.gz
```
3. 创建软链接
```
mv cmake-3.12.2-Linux-x86_64 /opt/cmake-3.12.2
ln -sf /opt/cmake-3.12.2/bin/*  /usr/bin/
```
4. 确认版本
```
cmake --version
```

### gcc版本升级
1. 添加源
```
sudo add-apt-repository ppa:ubuntu-toolchain-r/test
sudo apt-get update
```
2. 安装新版gcc/g++
```
sudo apt-get install gcc-7 g++-7
```
3. 切换gcc/g++版本
```
cd /usr/bin
sudo rm gcc
sudo ln -s gcc-7 gcc
sudo rm g++
sudo ln -s g++-7 g++
```
4. 确认版本
```
gcc -v
```
## 编译 chainsqld
- 重新进入源码根目录
```bash
> cd ~/work/chainsqld
```
- 创建并进入编译目录 build
```bash
> mkdir build && cd build
```
- 执行 cmake
```bash
> cmake -DCMAKE_BUILD_TYPE=Release|Debug ..
```

- 编译

```base
> make -j<Num Parallel>
```
注意上面的Num Parallel需要指定一个并行编译数量，如：2
