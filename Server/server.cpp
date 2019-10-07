/**
 * 服务端程序
 **/
#include "httplib.h"
#include <boost/filesystem.hpp>
#include <string>
#include <iostream>
#include <fstream>
#define SERVER_BASE_DIR "www"
#define SERVER_ADDR "0.0.0.0"
#define SERVER_PORT 9000
#define SERVER_BACKUP_DIR SERVER_BASE_DIR"/list/"
namespace bf = boost::filesystem;
using namespace httplib;
class CloudServer
{
  public:
    CloudServer()
    {
      bf::path base_path(SERVER_BASE_DIR); 
      if(!bf::exists(base_path))
      {
        bf::create_directory(base_path);
      }
      bf::path list_path(SERVER_BACKUP_DIR);
      if(!bf::exists(list_path))
      {
        bf::create_directory(list_path);
      }
    }
    bool Start()
    {
      //srv.set_base_dir(SERVER_BASE_DIR);
      srv.Get("/(list(/){0,1}){0,1}", GetFileList);
      srv.Get("/list/(.*)", GetFileData);
      srv.Put("/list/(.*)", PutFileData);
      srv.listen(SERVER_ADDR, SERVER_PORT);
      return true;
    }
  private:
    Server srv;
    //获取文件列表
    static void GetFileList(const Request& req, Response& rsp)
    {
      bf::path list(SERVER_BACKUP_DIR);
      bf::directory_iterator item_begin(list);
      bf::directory_iterator item_end;
      std::string body;
      body = "<html><body><ol><hr />";
      for(; item_begin != item_end; ++item_begin)
      {
        if(bf::is_directory(item_begin->status()))
        {
          continue;
        }
        std::string file = item_begin->path().filename().string();
        std::string uri = "/list/" + file;
        body += "<h4><li>";
        body += "<a href='";
        body += uri;
        body += "'>";
        body += file;
        body += "</a></li></h4>";
        //std::string file = item_begin->path().string();
        //std::cout << file << std::endl;
      }
      body += "<hr /></ol></body></html>";
      rsp.set_content(&body[0], "text/html");
    }
    //分块上传数据
    static void PutFileData(const Request& req, Response& rsp)
    {
      if(!req.has_header("Range"))
      {
        rsp.status = 400;
        return;
      }
      std::string range = req.get_header_value("Range");
      int64_t range_start;
      //解析起始位置
      if(RangeParse(range, range_start) == false)
      {
        rsp.status = 400;
        return;
      }
      std::string realpath = SERVER_BASE_DIR + req.path;
      std::ofstream file(realpath, std::ios::binary | std::ios::trunc);
      //文件未成功打开
      if(!file.is_open())
      {
        std::cerr << "open file " << realpath << " error" << std::endl;
        rsp.status = 500;
        return;
      }
      file.seekp(range_start, std::ios::beg);
      file.write(&req.body[0], req.body.size());
      if(!file.good())
      {
        std::cerr << "file write body error" << std::endl;
        rsp.status = 500;
        return;
      }
      file.close();
    }
    //起始位置解析
    static bool RangeParse(std::string& range, int64_t& start)
    {
      //Range:bytes=start-end
      size_t pos1 = range.find("=");
      size_t pos2 = range.find("-");
      if(pos1 == std::string::npos || pos2 == std::string::npos)
      {
        std::cerr << "range:[" << range << "] format error" << std::endl;
        return false;
      }
      //bytes=start-end
      std::stringstream rs;
      rs << range.substr(pos1 + 1, pos2 - pos1 - 1);
      rs >> start;
      return true;
    }
    //获取文件数据
    static void GetFileData(const Request& req, Response& rsp)
    {
      std::string file = SERVER_BASE_DIR + req.path;
      //文件不存在
      if(!bf::exists(file))
      {
        std::cerr << "file not exists" << std::endl;
        rsp.status = 404;
        return;
      }
      std::ifstream ifile(file, std::ios::binary);
      //打开文件失败
      if(!ifile.is_open())
      {
        std::cerr << "open file " << file << " error" << std::endl;
        rsp.status = 500;
        return;
      }
      std::string body;
      int64_t fsize = bf::file_size(file);
      body.resize(fsize);
      ifile.read(&body[0], fsize);
      if(!ifile.good())
      {
        std::cerr << "read file " << file << "body error" << std::endl;
        rsp.status = 500;
        return;
      }
      rsp.set_content(body, "text/plain");
    }
    static void BackupFile(const Request& req, Response& rsp)
    {

    }
};
int main()
{
  CloudServer srv;
  srv.Start();
}
