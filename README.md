
# 快传

### 项目描述: 

	搜索附近的在线用户,获取用户列表之后,可以查看指定主机的文件列表,并下载自己想要的文件

### 实现:
	使用httplib搭建http服务器和客户端

### 整体思路:

#### 连接流程(传输层):

	服务器创建Server类,注册回调函数(socket,bind) 

	调用Listen创建监听socket ,等待连接请求(listen,accept)  

									---->       客户端创建Client对象连接服务端(socket.bind)

									<----	     组织头部键值发起请求(connect,write)

	服务器收到请求创建新的socket并传给新线程

	读取数据并解析,处理后返回响应(read,write)                      

#### 通信流程(应用层):

	客户端发起配对请求 --->   服务器响应配对

	客户端发起文件列表请求 ---> 服务器响应一个文件列表

    	客户端发起下载请求 ---> 服务器传输数据

###### 服务器功能: 

	 提供能被附近客户端发现的功能
	 提供客户端请求文件列表的功能
	 提供客户端下载文件功能

##### 客户端功能:

	 搜索附近主机的功能
	 获取指定主机的文件列表的功能
	 下载指定文件的功能
    

#### 遇到的问题:

	Q1:g++ httplib makefile 出错

	A1:httplib库需要C++11支持  -std=0x /11

    	Q2:g++ -lboost_filesystem 出错 

	A2:使用boost/filesystem库,  在g++需要这样写 -lboost_system -lboost_filesystem

	Q3:关于boost/filesystem接口使用

	A3:bf::exits()  bf::create_directory  boost::split   bf::directory_iterator及其path.string()                       									bf::is_directory     bf::file_size  bf::thread

	Q4:如何注册回调函数?

 	A4:请求方法Get("请求路径",具体回调函数); 服务器实现回调函数 ; 客户端直接请求(路径)

	Q5:ip绑定哪个?

	A5:0.0.0.0 监听本地全部网卡

	Q6:req , rsp主要有什么?

	A6: req: 协议版本,请求方法,资源路径,头部,正文,设置头部
            rsp: 协议版本,响应码,头部,正文,设置头部

	Q7:如何搜索附近的主机

	A7:	1.获取本机所有的网卡信息来求出所有主机的ip地址
		2.对所有主机发起配对请求.响应的便是在线主机.

    	Q8:搜索的具体细节?

	A8: 	使用ipv4结构体接收网卡的ip和掩码信息,除去ipv4协议簇和本机换环网卡(inet_addr转换127.0.0.1);
		计算网络号和主机数量,转换好的ip地址放入list中.   
		主机号2~253   254不知道为啥,老是卡主程序.
		接口: inet_addr 标准ip转数字 		inet_ntoa 数字转标准ip
		htonl  主机字节序转网络字节序    ntohl 网络字节序转主机字节序     

	Q9:如何配对?

	A9:使用多线程并行配对请求,并把有响应的主机放入在线主机列表当中

	Q10:C++11线程的问题

    	A10: makefile 需要 -lpthread
			move(thr) 线程所有权转移,放入线程数组管理.  
			thread传参用ref,否则编译出错
			thread需要传参this,否则编译出错

	Q11:获取文件列表?

	A11:服务器扫描共享目录,将其添加到rsp中,客户端用file_list接收

    	Q12:如何整合和拆分rsp->body?

    	A12:服务器写的时候给每个文件名后面+'\n'标记,客户端读的时候用bf::split函数拆分接收

    	Q13:测试文件?

    	A13:  dd if=/dev/zero of=50M.file bs=1M count=50

	Q14:如何下载?

    	A14: 1.客户端请求文件大小,服务器返回文件大小
	     2.客户端计算分块
	     3.线程向服务器请求下载分块. 服务器响应下载

   	Q15:请求细节?

   	A15:客户端组织路径HEAD请求,服务器解析路径,bf::file_size响应HEAD请求.

  	Q16:下载请求?

 	A16:客户端使用boost库线程数组和线程返回值数组,进行多线程下载请求    
         服务器拿到分块数据,使用ifstream读取文件并放入rsp的body中返回线
	 程用fd读入rsp的body.

	Q17:makefile?

   	A17:-使用了boost库的线程需要 -lbf::thread 

   	Q18: ifstream

    	A18:fstream子类,从硬盘到内存.

     	Q19:配对慢,下载慢

     	A19:使用线程并行配对

    	Q20:验证算法

    	A20:md5sum校验

    	Q21:分块下载的线程传入参数过多

    	A21:使用boost::thread

 	Q22:c++使用fstream默认在打开文件时截断文件,分块传输会出现问题

	A22:使用系统调用接口


