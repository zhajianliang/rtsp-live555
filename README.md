# rtsp-live555
# 编译过程

live会依赖openssl的库文件，编译live555源代码之前需要先确保openssl正常安装。

## openssl交叉编译

以全志937较差编译链为例

**1.去官网下载openssl源代码**

**2、进入到openssl源代码目录，执行以下语句**

```c
./config no-asm shared no-async --prefix=/opt/openssl/ --cross-compile-prefix=arm-openwrt-linux-muslgnueabi-
参数说明:
	no-asm: 在交叉编译过程中不使用汇编代码代码加速编译过程.原因是它的汇编代码是对arm格式不支持的。
	shared: 生成动态连接库（会同时生成动态库和静态库，如果使用静态库的时候需要export LD_LIBRARY_PATH）。
	no-async: 交叉编译工具链没有提供GNU C的ucontext库
	–prefix=: 安装路径，编译完成install后将有bin，lib，include等文件夹
	–cross-compile-prefix=: 交叉编译工具
    -static ：生成静态库
```

**3、修改makefile，去除掉 -m64和-m32选项**

**4、执行make && make install**

```
make&&make install
库和头文件会安装到/opt/openssl/下
```

## live555交叉编译

**1、去live555下载release源代码，最近的版本编译会有问题**

**2、生成对应make文件**

```
1、如果不需要交叉编译只需要执行
./genMakefile linux
make 会生成对应的头文件和库文件，使用的使用只需要将该库和头文件集成到自己的项目中
```

