#include <unordered_map>
#include <iostream>
#include <vector>
#include <string>
#include <thread>
#include <sstream>
#include <boost/filesystem.hpp>
#include <boost/algorithm/string.hpp>
#include <fstream>
#include <sys/stat.h>
#define UNGZIPFILE_PATH "www/list"
#define GZIPFILE_PATH "www/zip"
#define RECORD_FILE "record.list"
#define HEAT_TIME 3
namespace bf = boost::filesystem;
//对热度低的文件进行压缩并进行后续处理
class CompressStore
{                                                          
  private:
    //文件名与压缩包名的映射
    std::unordered_map<std::string, std::string> _file_list;
    //文件存储位置
    std::string _file_dir;
  private:
    //每次压缩存储线程启动的时候从文件中读取列表信息
    bool GetListRecord()
    {
      //filename gzipfilename\n
      bf::path name(RECORD_FILE);
      //文件不存在
      if(!bf::exists(name))
      {
        std::cerr << "record file is not exists" << std::endl;
        return false;
      }
      std::ifstream file(RECORD_FILE, std::ios::binary);
      //读取失败
      if(!file.is_open())
      {
        std::cerr << "record file open error" << std::endl;
        return false;
      }
      int64_t fsize = bf::file_size(name);
      std::string body;
      body.resize(fsize);
      file.read(&body[0], fsize);
      //读取失败
      if(!file.good())
      {
        std::cerr << "record file body read error" << std::endl;
        return false;
      }
      file.close();
      std::vector<std::string> list;
      boost::split(list, body, boost::is_any_of("\n"));
      for(auto i : list)
      {
        //分割出原文件名和压缩文件名
        //filename gzipname
        size_t pos = i.find(" ");
        //不合法
        if(pos == std::string::npos)
        {
          continue;
        }
        //原文件名
        std::string key = i.substr(0, pos);
        //压缩文件文件名
        std::string value = i.substr(pos + 1);
        //添加到内存中
        _file_list[key] = value;
      }
      return true;
    }
    //每次压缩完毕都要将列表信息存储到文件中
    bool SetListRecord()
    {
      std::stringstream tmp;
      for(auto i : _file_list)
      {
        tmp << i.first << " " << i.second << "\n";
      }
      std::ofstream file(RECORD_FILE, std::ios::binary);
      //打开文件失败
      if(!file.is_open())
      {
        std::cerr << "record file open error" << std::endl;
        return false;
      }
      file.write(tmp.str().c_str(), tmp.str().size());
      //写入文件失败
      if(!file.good())
      {
        std::cerr << "record file write body error" << std::endl;
        return false;
      }
      file.close();
      return true;
    }
    //获取list目录下非压缩文件名称
    bool GetUnGZipFile()
    {
      if(!bf::exists(UNGZIPFILE_PATH))
      {
        bf::create_directory(UNGZIPFILE_PATH);
      }
      bf::directory_iterator item_begin(UNGZIPFILE_PATH);
      bf::directory_iterator item_end;
      for(; item_begin != item_end; ++item_begin)
      {
        if(bf::is_directory(item_begin->status()))
        {
          continue;
        }
        std::string name = item_begin->path().string();
        //不需要压缩
        if(!IsNeedCompress(name))
        {
          continue;
        }
        //压缩成功
        if(CompressFile(name))
        {
          std::cout << "file:[" << name << "] store success" << std::endl;
        }
        else //压缩失败
        {
          std::cerr << "file:[" << name << "] store error" << std::endl;
        }
      }
      return true;
    }
    //判断是否需要压缩
    bool IsNeedCompress(std::string& file)
    {
      //根据最后一次访问时间进行判断
      //最后一次访问超过三秒以上则进行压缩
      struct stat st;
      if(stat(file.c_str(), &st) < 0)
      {
        std::cerr << "get file:[" << file << "] stat error" << std::endl;
        return false;
      }
      time_t cur_time = time(NULL);
      time_t acc_time = st.st_atime;
      if(cur_time - acc_time > HEAT_TIME)
      {

      }
      return true;
    }
    //对文件进行压缩存储
    bool CompressFile(std::string& file);
    //判断文件是否已经压缩
    bool IsCompressed(std::string& file);
  public:
    //获取文件列表功能
    bool GetFileList(std::vector<std::string>& list);
    //获取文件数据功能
    bool GetFileData(std::string& file, std::string& body)
    {
      //1、非压缩文件数据获取
      //2、压缩文件数据获取
    }
    //开启线程循环监听目录，对目录下热度低文件进行压缩存储
    bool LowHeatFileStore()
    {
      while(1)
      {
        //1、获取list目录下文件名称
        //2、判断文件是否需要进行压缩存储
        //3、对文件进行压缩存储
      }
    }
};

