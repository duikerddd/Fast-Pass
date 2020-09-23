
#include<stdio.h>
#include<iostream>
#include<fstream>
#include<boost/filesystem.hpp>
#include"httplib.h"
using namespace httplib;
namespace bf = boost::filesystem;

#define SHARED_PATH "Shared"

class MyServer{
  public:
    //1.服务器创建---创建共享文件
    MyServer()
    {
      //不存在则创建
      if(!bf::exists(SHARED_PATH))
      {
        bf::create_directory(SHARED_PATH);
      }
    }
    //2.服务器启动 --- 注册回调函数,开始监听
    void ServerStart(uint16_t port)
    {
      _server.Get("/hostpair",GetHostPair);
      _server.Get("/list",GetFileList);
      _server.Get("/list/(.*)",GetFileData);
      
      _server.listen("0.0.0.0",port);
    }
  private:
    //3.响应配对请求
    static void GetHostPair(const Request& req,Response& rsp)
    {
        rsp.status=200;
    }

    //4.响应文件列表请求
    static void GetFileList(const Request& req,Response& rsp)
    {
        //扫描目录 --- bf库
        bf::directory_iterator item_begin(SHARED_PATH);
        bf::directory_iterator item_end;
        for(;item_begin!=item_end;++item_begin)
        {
          //文件夹跳过
          if(bf::is_directory(*item_begin)) continue;

          //文件则放入body中,用\n分割标记
          bf::path pathname = item_begin->path();
          std::string path = pathname.filename().string();
          rsp.body += path+'\n';
        }
        //设置body格式--文本文件 和状态码
        rsp.set_header("Content-Type","text/html");
        rsp.status = 200;

    }


    //5.1计算分块大小
    static bool RangeParse(std::string& range,int64_t& start,int64_t& len)
    {
        //RANGE: bytes=start-end 
        //检查格式
        size_t pos1 = range.find('=');
        size_t pos2 = range.find('-');
        if(pos1==std::string::npos || pos2==std::string::npos)
        {
              std::cout<<"range格式错误\n";
              return false;
        }

        //解析start,end 
        int64_t end;
        std::string sstart,send;
        sstart=range.substr(pos1+1,pos2-pos1-1);
        send=range.substr(pos2+1);
        std::stringstream tmp;
        tmp<<sstart;
        tmp>>start;
        tmp.clear();
        tmp<<send;
        tmp>>end;
        len=end-start+1;
        
        return true;
    }
    //5.响应下载请求
    static void GetFileData(const Request& req,Response& rsp)
    {
        //5.1解析下载路径--- req: /list/a.txt ---> Download/a.txt
        bf::path path(req.path);
        //stringstream拼接字符串 , filename是文件名
        std::stringstream name;
        name<<SHARED_PATH<<"/" + path.filename().string();  

        //没有请求的文件,返回404
        if(!bf::exists(name.str()))
        {
            rsp.status = 404;return;
        }
        //请求的是目录,返回403
        if(bf::is_directory(name.str()))
        {
          rsp.status = 403;return;
        }
        
        //使用bf::file_size获取文件大小,返回HEAD响应
        int64_t fsize = bf::file_size(name.str());
        if(req.method == "HEAD")
        {
          rsp.status = 200;
          std::string len = std::to_string(fsize);
          rsp.set_header("Content-Length",len.c_str());
          
          return; 
        }

        //分块下载
        //解析req,看没有有分块下载
        if(!req.has_header("Range"))
        {
          rsp.status=400; return;
        }
        //计算分块大小 
        std::string range; 
        range = req.get_header_value("Range");
        int64_t start,len;
        bool ret = RangeParse(range,start,len);
        if(ret==false) return;
        
        //file读入文件
        std::ifstream file(name.str(),std::ios::binary);
        //文件没打开
        if(!file.is_open())
        {
          std::cout<<"文件打开失败\n";
          return;
        }
        //跳转到下载位置,写入rsp
        file.seekg(start,std::ios::beg);
        rsp.body.resize(len);
        file.read(&rsp.body[0],len);
        //文件出错
        if(!file.good())
        {
          std::cout<<"文件出错\n";
          rsp.status = 500;
          return;
        }
        file.close();

        //组织rsp
        rsp.status=206;
        rsp.set_header("Content-Type","application/octet-stream");
        std::cout<<range<<"文件传输成功!\n";
        
        return;
        
    }
  
    Server _server;
};
