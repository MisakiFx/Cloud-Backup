#include <unordered_map>
#include <iostream>
#include <sys/file.h>
#include <vector>
#include <string>
#include <thread>
#include <sstream>
#include <pthread.h>
#include <boost/filesystem.hpp>
#include <boost/algorithm/string.hpp>
#include <fstream>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <zlib.h>
#define UNGZIPFILE_PATH "www/list/"
#define GZIPFILE_PATH "www/zip/"
#define RECORD_FILE "record.list"
#define HEAT_TIME 100
namespace bf = boost::filesystem;
//对热度低的文件进行压缩并进行后续处理
class CompressStore
{                                                          
  private:
    //文件名与压缩包名的映射
    std::unordered_map<std::string, std::string> _file_list;
    //文件存储位置
    std::string _file_dir;
    //定义读写锁
    pthread_rwlock_t _rwlock;
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
      std::ofstream file(RECORD_FILE, std::ios::binary | std::ios::trunc);
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
      //不需要压缩存储
      if((cur_time - acc_time) < HEAT_TIME)
      {
        return false;
      }
      return true;
    }
    //对文件进行压缩存储
    bool CompressFile(std::string& file, std::string& gzip)
    {
      int fd = open(file.c_str(), O_RDONLY);
      //普通文件打开失败
      if(fd < 0)
      {
        std::cerr << "com open file:[" << file << "] error" << std::endl;
        return false;
      }
      //压缩文件打开失败
      gzFile gf = gzopen(gzip.c_str(), "wb");
      if(gf == NULL)
      {
        std::cerr << "com open gzip:[" << gzip << "] error" << std::endl;
        return false;
      }
      int ret;
      char buf[1024];
      //读文件数据，加文件锁
      flock(fd, LOCK_SH);
      while((ret = read(fd, buf, 1024)) > 0)
      {
        gzwrite(gf, buf, 1024);
      }
      flock(fd, LOCK_UN);
      close(fd);
      gzclose(gf);
      //压缩成功删除原文件 unlink接口
      unlink(file.c_str());
      ////添加到压缩信息中
      //_file_list[file] = gzip;
      return true;
    }
    //解压缩文件
    bool UnCompressFile(std::string& gzip, std::string& file)
    {
      int fd = open(file.c_str(), O_CREAT | O_WRONLY, 0664);
      if(fd < 0)
      {
        std::cerr << "open file " << file << " failed" << std::endl;
        return false;
      }
      gzFile gf = gzopen(gzip.c_str(), "rb");
      if(gf == NULL)
      {
        std::cerr << "open gzip " << gzip << " failed" << std::endl;
        return false;
      }
      int ret;
      char buf[1024];
      //写文件，加文件写锁
      flock(fd, LOCK_EX);
      while((ret = gzread(gf, buf, 1024)) > 0)
      {
        int len = write(fd, buf, 1024);
        if(len < 0)
        {
          std::cerr << "get gzip data failed" << std::endl;
          gzclose(gf);
          close(fd);
          flock(fd, LOCK_UN);
          return false;
        }
      }
      flock(fd, LOCK_UN);
      gzclose(gf);
      close(fd);
      unlink(gzip.c_str());
      return true;
    }
    //获取普通文件的数据放到body中
    bool GetNormalFile(std::string& name, std::string& body)
    {
      int64_t fsize = bf::file_size(name);
      body.resize(fsize);
      int fd = open(name.c_str(), O_RDONLY);
      //打开失败
      if(fd < 0)
      {
        std::cerr << "GetNormalFile(): open file " << name << " failed" << std::endl;
        return false;
      }
      flock(fd, LOCK_SH);
      int ret = read(fd, &body[0], fsize);
      flock(fd, LOCK_UN);
      //读取失败
      if(ret != fsize)
      {
        std::cerr << "GetNormalFile(): get file " << name << " body failed" << std::endl;
        close(fd);
        return false;
      }
      close(fd);
      return true;
    }
    //目录检测，获取目录中的文件名
    //1、判断文件是否需要压缩存储
    //2、文件压缩存储
    bool DirectoryCheck()
    {
      if(bf::exists(UNGZIPFILE_PATH))
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
        if(IsNeedCompress(name))
        {
          std::string gzip = GZIPFILE_PATH + item_begin->path().filename().string() + ".gz";
          CompressFile(name, gzip);
          AddFileRecord(name, gzip);
        }
      }
      return true;
    }
  public:
    CompressStore()
    {
      //初始化读写锁
      pthread_rwlock_init(&_rwlock, NULL);
      //目录不存在创建压缩目录
      if(!bf::exists(GZIPFILE_PATH))
      {
        bf::create_directory(GZIPFILE_PATH);
      }
    }
    ~CompressStore()
    {
      pthread_rwlock_destroy(&_rwlock);
    }
    //获取文件列表功能
    bool GetFileList(std::vector<std::string>& list)
    {
      //_file_list有可能多个线程都在操作，因此是临界资源
      //加读写锁
      pthread_rwlock_rdlock(&_rwlock);
      for(auto i : _file_list)
      {
        list.push_back(i.first);
      }
      //解锁
      pthread_rwlock_unlock(&_rwlock);
      return true;
    }
    //通过文件名称获取文件对应的压缩包名称
    bool GetFileGzip(std::string& file, std::string& gzip)
    {
      pthread_rwlock_rdlock(&_rwlock);
      auto it = _file_list.find(file);
      if(it == _file_list.end())
      {
        pthread_rwlock_unlock(&_rwlock);
        std::cerr << "GetFileData(): dont have the file " << file << std::endl;
        return false;
      }
      gzip = it->second;
      pthread_rwlock_unlock(&_rwlock);
      return true;
    }
    //获取文件数据功能
    bool GetFileData(std::string& file, std::string& body)
    {
      if(bf::exists(file))
      {
        //1、非压缩文件数据获取
        GetNormalFile(file, body);
      }
      else 
      {
        //2、压缩文件数据获取
        //获取压缩包名称 gzip
        std::string gzip;
        GetFileGzip(file, gzip);
        UnCompressFile(gzip, file);
        GetNormalFile(file, body);
      }
      return true;
    }
    //添加文件记录信息
    bool AddFileRecord(const std::string& file, const std::string& gzip)
    {
      pthread_rwlock_wrlock(&_rwlock);
      _file_list[file] = gzip;
      pthread_rwlock_unlock(&_rwlock);
    }
    //数据写入文件指定位置，外部分块写入接口
    bool SetFileData(const std::string& file, const std::string& body, const int64_t offset)
    {
      int fd = open(file.c_str(), O_CREAT | O_WRONLY, 0664);
      //文件打开失败
      if(fd < 0)
      {
        std::cerr << "open file " << file << " error" << std::endl;
        return true;
      }
      flock(fd, LOCK_EX);
      lseek(fd, offset, SEEK_SET);
      int ret = write(fd, &body[0], body.size());
      //写入失败
      if(ret < 0)
      {
        std::cerr << "store file " << file << " data error" << std::endl;
        flock(fd, LOCK_UN);
        return false;
      }
      flock(fd, LOCK_UN);
      close(fd);
      AddFileRecord(file, "");
      return true;
    }
    //开启线程循环监听目录，对目录下热度低文件进行压缩存储，线程入口函数
    bool LowHeatFileStore()
    {
      GetListRecord();
      while(1)
      {
        //1、获取list目录下文件名称
        //2、判断文件是否需要进行压缩存储
        //3、对文件进行压缩存储
        DirectoryCheck();
        SetListRecord();
        sleep(3);
      }
    }
};

