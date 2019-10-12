/**
 * 服务端程序
 **/
#include "httplib.h"
#include <boost/filesystem.hpp>
#include <string>
#include <iostream>
#include <fstream>
#include <unistd.h>
#include <fcntl.h>
#include "compress.hpp"
#define SERVER_BASE_DIR "www"
#define SERVER_ADDR "0.0.0.0"
#define SERVER_PORT 9000
#define SERVER_BACKUP_DIR SERVER_BASE_DIR"/list/"
namespace bf = boost::filesystem;
using namespace httplib;
CompressStore cstor;
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
      std::vector<std::string> list;
      cstor.GetFileList(list);
      std::string body;
      body = "<html><body><ol><hr />";
      for(auto i : list)
      {
        bf::path path(i);
        std::string file = path.filename().string();
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
      std::cout << "backup file:[" << req.path << "] range:[" << range << "]" <<  std::endl;
      std::string realpath = SERVER_BASE_DIR + req.path;
      cstor.SetFileData(realpath, req.body, range_start);
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
      std::string body;
      std::string realpath = SERVER_BASE_DIR + req.path;
      cstor.GetFileData(realpath, body);
      rsp.set_content(body, "application/octet-stream");
    }
    static void BackupFile(const Request& req, Response& rsp)
    {

    }
};
void thr_start()
{
  cstor.LowHeatFileStore();
}
int main()
{
  std::thread thr(thr_start);
  thr.detach();
  CloudServer srv;
  srv.Start();
}
