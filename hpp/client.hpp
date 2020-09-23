#ifndef _CLI_ 
#define _CLI_
#include<stdio.h>
#include<iostream>
#include<sys/types.h> //网卡接口
#include<ifaddrs.h>  
#include <sys/stat.h>  //open
#include <fcntl.h>

#include <sys/socket.h>//ip字符串与数字转换接口
#include <netinet/in.h>
#include <arpa/inet.h>

#include <boost/algorithm/string.hpp>//boost::split
#include<boost/thread/thread.hpp>//boost::thread

#include<thread>

#include"httplib.h"

using namespace httplib;

#define RANGE_SIZE (30<<20) //30M 

class MyClient{
  public:
    //1.客户端构造 --- 接收端口号
    MyClient(uint16_t port):_port(port){}
    //2.1选择界面
    int DoFace()
    {
      std::cout<<"1.搜索附近主机"<<std::endl;
      std::cout<<"2.显示在线主机"<<std::endl;
      std::cout<<"3.显示文件列表"<<std::endl;
      std::cout<<"0.退出"<<std::endl;

      std::cout<<"请您输入选择:";
      fflush(stdout);
      int choice = -1;
      std::cin>>choice;
      return choice;
    }
    //2.客户端启动
    bool ClientStart()
    {

      while(1)
      {
        int choice = DoFace();
        std::vector<std::string> list; //所有主机
        std::string file_name;//选定主机
        switch(choice)
        {
          case 1: 
            //(1)搜索附近主机
            GetAllHost(list);
            //(2)对所有主机进行配对请求
            GetOnlineHost(list);
            break;

          case 2: 
            //(3)展示在线主机列表,选择指定主机
            if(ShowOnlineHost()==false) break;
            //(4)获取文件列表
            GetFileList();
            break;

          case 3: 
            //(5)展示文件列表
            if(ShowFileList(file_name)==false) break;
            //(6)选择下载
            DownloadFile(file_name);
            break;

          case 0:
            exit(0);

          default: 
            std::cout<<"输入有误,请重新输入"<<std::endl;
            break;

        }
      }
      return true;
    }

  private:
    //(1)搜索附近在线主机
    bool GetAllHost(std::vector<std::string>& list)
    {
      struct ifaddrs *ifa=NULL,*ifa_cur=NULL;
      struct sockaddr_in *ip = NULL;
      struct sockaddr_in *mask = NULL;

      //获取本机网卡信息
      if(getifaddrs(&ifa)==-1)
      {
        perror("网卡信息获取失败!\n");
        return false;
      }

      //循环遍历网卡
      for(ifa_cur=ifa;ifa_cur!=NULL;ifa_cur=ifa_cur->ifa_next)
      {
        ip = (struct sockaddr_in*)ifa_cur->ifa_addr;
        mask = (struct sockaddr_in*)ifa_cur->ifa_netmask;

        //网卡不是TCP协议簇,跳过
        if(ip->sin_family != AF_INET) continue; 
        //本机环回网卡,跳过
        if(ip->sin_addr.s_addr == inet_addr("127.0.0.1") ) continue;

        //计算网络号和掩码
        //net = ip & mask
        //主机数量 = ~mask 
        uint32_t net,sum;
        net = ntohl(ip->sin_addr.s_addr & mask->sin_addr.s_addr);
        sum = ntohl(~(mask->sin_addr.s_addr));
        //放入主机列表(1为本机,sum为广播地址)
        for(uint32_t i=2;i<sum-1;++i)
        {
          struct in_addr ip;   //ip地址结构体
          ip.s_addr = htonl(net+i);   
          list.push_back(inet_ntoa(ip));
        }
      }
      //释放网卡信息
      freeifaddrs(ifa);
      return true;
    }

    //(2)对所有主机进行配对请求
    void HostPair(std::string& ip)
    {
      //创建客户端发起请求
      Client client(&ip[0],_port);
      auto rsp = client.Get("/hostpair");

      //响应正确的主机加入在线主机列表
      if(rsp && rsp->status==200)
      {
        std::cout<<ip<<std::endl;
        _online_host.push_back(ip);
        return;
      }
      std::cerr<<ip<<std::endl;
      return;
    }
    bool GetOnlineHost(std::vector<std::string>& list)
    {
      //请求前,清空在线主机链表
      _online_host.clear();
      //多线程并行配对,使用线程数组,利于回收资源
      std::vector<std::thread> thr_list(list.size());
      for(uint16_t i=0;i<thr_list.size();++i)
      {
        //类成员函数传参,不要忽视this指针
        std::thread thr(&MyClient::HostPair,this,std::ref(list[i]));
        thr_list[i] = std::move(thr);
      }
      //线程销毁
      for(uint16_t i=0;i<thr_list.size();++i)
      {
        thr_list[i].join();
      }
      return true;
    }

    //(3)展示在线主机列表,选择指定主机
    bool ShowOnlineHost()
    {
      if(_online_host.size()<1)
      {
        std::cout<<"找不到在线主机\n";
        return false;
      }

      for(uint16_t i=0;i<_online_host.size();++i)
      {
        std::cout<<i<<"."<<_online_host[i]<<"\n"; 
      }
      std::cout<<"请选择要访问的主机:";
      fflush(stdout);
      std::cin>>_host;
      if(_host<0 || _host>=_online_host.size())
      {
        _host = -1;
        std::cout<<"输入错误,请重新输入\n";
        return false;
      }
      return true;
    }
    //(4)获取文件列表
    bool GetFileList()
    {
      //向指定服务器请求文件列表 /list 
      Client client(_online_host[_host].c_str(),_port);
      auto rsp = client.Get("/list");

      if(rsp && rsp->status==200)
      {
        //对响应的body进行切分,并将其放入文件列表中
        boost::split(_file_list,rsp->body,boost::is_any_of("\n")); 
        return true;
      }

      return false;
    }
    //(5)展示文件列表,选择指定文件
    bool ShowFileList(std::string& file_name)
    {
      for(uint16_t i=0;i<_file_list.size();++i)
      {
        std::cout<<i<<"."<<_file_list[i]<<std::endl;
      }
      std::cout<<"请选择指定文件:";
      fflush(stdout);
      uint16_t choice;
      std::cin>>choice;
      if(choice<0 || choice>=_file_list.size())
      {
        std::cout<<"输入错误\n";
        return false;
      }
      file_name = _file_list[choice];

      return true;
    }
    //(6.1)获取文件大小
    int64_t GetFileSize(std::string& host,std::string& file_name)
    {
      int64_t fsize=-1;
      std::string path = "/list/"+file_name;
      //客户端向指定主机发送Head请求
      Client client(host.c_str(),_port);

      auto rsp = client.Head(path.c_str());
      if(rsp && rsp->status==200)
      {
        if(!rsp->has_header("Content-Length"))
        {
          std::cout<<"文件大小请求失败\n";  return -1;
        }
        //解析rsp中的content-length头
        std::string len = rsp->get_header_value("Content-Length");
        //stringsteam类进行数字与string的转换
        std::stringstream tmp;
        tmp<<len;
        tmp>>fsize;
      }

      return fsize;
    }
    //(6.2)线程入口函数: 下载分块
    void RangeDownload(std::string& host,std::string& name,int64_t start,int64_t end,int* res)
    {
      //组织请求路径和下载路径
      std::string uri = "/list/"+name;
      std::string realpath = "Download/"+name;

      //组织请求头 Range: bytes=start-end 
      std::stringstream range;
      range<<"bytes="<<start<<"-"<<end;
      Headers header; 
      header.insert(std::make_pair("Range",range.str().c_str()));

      //建立客户端对服务器访问 
      Client client(host.c_str(),_port);
      auto rsp = client.Get(uri.c_str(),header);

      //解析rsp,文件操作将流放入download中
      if(rsp && rsp->status==206)
      {
        std::cout<<realpath.c_str();
        int fd = open(realpath.c_str(),O_WRONLY|O_CREAT,0664);
        if(fd<0)
        {
          std::cout<<fd<<"Download打开失败\n";
          return;
        }
        //跳转,写入内容
        lseek(fd,start,SEEK_SET);
        int ret = write(fd,&rsp->body[0],rsp->body.size());
        if(ret==-1)
        {
          *res = -1;
          std::cout<<"RANGE:"<<range.str()<<"下载失败\n";
          return;
        }
        std::cout<<"RANGE:"<<range.str()<<"下载成功!\n";
        *res=1;  //返回值
        close(fd);

      }

      return;
    }
    //(6)下载指定文件
    bool DownloadFile(std::string& file_name)
    {
      //GET /list/file_name HTTP/1.1
      //HTTP/1.1 200 OK 
      //Content-Length: fsize

      //6.1获取文件大小
      std::string host = _online_host[_host];
      int64_t fsize = GetFileSize(host,file_name);
      if(fsize==-1)
      {
        return false;
      }

      //6.2对文件分块
      int count = fsize/RANGE_SIZE;

      //6.3多线程下载
      int ret = 1; 
      std::vector<boost::thread> thr_list(count+1);//多线程数组
      std::vector<int> res_list(count+1);//线程回收数组
      for(int i=0;i<=count;++i)
      {
        //组织RANGE区间
        int64_t start,end;
        start = i*RANGE_SIZE;
        end = (i+1)*RANGE_SIZE-1;

        //最后一个包:如果count被整除,说明没有数据;如果count没整除,说明还剩下一些数据,继续下载
        if(i==count)
        {
          if(fsize%RANGE_SIZE==0)
          {
            break;
          }
          end = fsize-1;
        }

        //创建多线程下载,并收入数组回收
        int *res = &res_list[i];
        boost::thread thr(&MyClient::RangeDownload,this,host,file_name,start,end,res);   
        thr_list[i] = std::move(thr);
      }
      //4.线程回收
      for(uint16_t i=0;i<=count;++i)
      {
        //完整包跳出
        if(i==count && fsize%RANGE_SIZE==0) break;

        thr_list[i].join();
        //判断线程是否完成
        if(res_list[i]==0)
        {
          ret = 0;
        }
      }

      if(ret == 0)
      {
        std::cout<<"下载失败!\n";
        return false;
      }
      std::cout<<"下载成功!\n";

      return true;
    }

    uint16_t _port;
    uint16_t _host;
    std::vector<std::string> _online_host; //在线主机
    std::vector<std::string> _file_list;  //文件列表
};
#endif

