#pragma once
#include <iostream>
#include <boost/filesystem.hpp>
#include <boost/algorithm/string.hpp>
#include <unordered_map>
#include <string>
#include <fstream>
#include <sstream>
#include <thread>
#include "httplib.h"
#define CLIENT_BACKUP_DIR "backup"
#define CLIENT_BACKUP_INFO_FILE "back.list"
#define RANGE_MAX_SIZE (10 << 20)//分块大小
#define SERVER_IP "192.168.61.128"
#define SERVER_PORT 9000
#define BACKUP_URI "/list/"
namespace bf = boost::filesystem;
class CloudCLient
{
private:
	//filename etag
	std::unordered_map<std::string, std::string> _backup_list;//存放备份信息
	//私有成员函数
private:
	//获取文件备份信息，用于判断哪些文件需要备份哪些无需备份
	bool GetBackupInfo()
	{
		bf::path path(CLIENT_BACKUP_INFO_FILE);
		//文件不存在
		if (!bf::exists(path))
		{
			std::cerr << "list file " << path.string() << " is not exist" << std::endl;
			return false;
		}
		int64_t fsize = bf::file_size(path);
		//文件为空
		if (fsize == 0)
		{
			std::cerr << "have no backup info" << std::endl;
			return false;
		}
		//filename1 etag\n
		//读取文件
		std::string body;
		body.resize(fsize);
		std::ifstream file(path.string(), std::ios::binary);
		//文件打开失败
		if (!file.is_open())
		{
			std::cerr << "list file open error" << std::endl;
			return false;
		}
		file.read(&body[0], fsize);
		//文件读取失败
		if (!file.good())
		{
			std::cerr << "read list file error" << std::endl;
			return false;
		}
		file.close();
		//分割字符串取出一个一个文件的数据
		//数据格式类型:filename etag\n
		std::vector<std::string> list;
		boost::split(list, body, boost::is_any_of("\n"));
		//获取文件信息存入_backup_list中
		for (auto i : list)
		{
			size_t pos = i.find(" ");
			//格式不对直接跳过
			if (pos == std::string::npos)
			{
				continue;
			}
			std::string key = i.substr(0, pos);
			std::string value = i.substr(pos + 1);
			_backup_list[key] = value;
		}
		return true;
	}
	//保存文件备份信息
	bool SetBackupInfo()
	{
		std::string body;
		for (auto i : _backup_list)
		{
			body += i.first + " " + i.second + "\n";
		}
		std::ofstream file(CLIENT_BACKUP_INFO_FILE, std::ios::binary);
		if (!file.is_open())
		{
			std::cerr << "open list file error" << std::endl;
			return false;
		}
		file.write(&body[0], body.size());
		if (!file.good())
		{
			std::cerr << "set backup info error" << std::endl;
			return false;
		}
		file.close();
		return true;
	}
	//监听目录获取目录文件信息，并且刷新备份信息
	bool BackupDirListen(const std::string& path)
	{
		bf::directory_iterator item_begin(CLIENT_BACKUP_DIR);
		bf::directory_iterator item_end;
		//遍历目录
		for (; item_begin != item_end; item_begin++)
		{
			//如果是目录则递归遍历
			if (bf::is_directory(item_begin->status()))
			{
				BackupDirListen(item_begin->path().string());
				continue;
			}
			//判断是否需要备份
			if (FilesIsNeedBackUp(item_begin->path().string()) == false)
			{
				continue;
			}
			//判断上传是否成功
			
			if (PutFileData(item_begin->path().string()) == false)
			{
				continue;
			}
			
			//更新备份信息到backup_list
			AddBackupInfo(item_begin->path().string());
		}
		return true;
	}
	//更新备份信息etag到backup_list
	bool AddBackupInfo(const std::string& file)
	{
		//etag = mtime-fsize
		std::string etag;
		if (GetFileEtag(file, etag) == false)
		{
			return false;
		}
		_backup_list[file] = etag;
		return true;
	}
	//备份file文件，上传文件数据
	//多线程分块上传文件
	bool PutFileData(const std::string& file)
	{
		bf::path path(file);
		int64_t fsize = bf::file_size(path);
		if (fsize <= 0)
		{
			std::cerr << "file " << file << " dont need to backup" << std::endl;
			return false;
		}
		int count = fsize / RANGE_MAX_SIZE;
		std::vector<bool> thr_res(count);
		std::vector<std::thread> thr_list;
		//分块创建线程传输文件
		for (int i = 0; i < count; i++)
		{
			int64_t range_start = i * RANGE_MAX_SIZE;
			int64_t range_end = (i + 1) * RANGE_MAX_SIZE;
			if (i == (count - 1))
			{
				range_end += (fsize % RANGE_MAX_SIZE);
			}
			int64_t range_len = range_end - range_start + 1;
			thr_list.push_back(std::thread(thr_start, file));
		}
		//等待每个线程结束，根据结果判断是否传输成功
		bool ret = true;
		for (int i = 0; i < count; i++)
		{
			thr_list[i].join();
			if (thr_res[i] == false)
			{
				ret = false;
			}
		}
		if (ret == false)
		{
			//AddBackupInfo(file);
			return false;
		}
		std::cout << "file:[" << file << "] backup success" << std::endl;
		return true;
	}
	//线程入口函数，在里面传输每一块文件数据
	static void thr_start(const std::string& file)
	{
		/*
		std::ifstream path(file, std::ios::binary);
		if (!path.is_open())
		{
			std::cerr << "range backup file " << file << " error" << std::endl;
			*res = false;
			return;
		}
		path.seekg(start, std::ios::beg);
		std::string body;
		body.resize(len);
		path.read(&body[0], len);
		if (!path.good())
		{
			std::cerr << "read file " << file << " range data error" << std::endl;
			*res = false;
			return;
		}
		path.close();
		bf::path name(file);
		std::string uri = BACKUP_URI + name.filename().string();
		httplib::Client cli(SERVER_IP, SERVER_PORT);
		httplib::Headers hdr;
		hdr.insert(std::make_pair("Content-Length", std::to_string(len)));
		std::stringstream tmp;
		tmp << "bytes=" << start << "-" << start + len - 1;
		hdr.insert(std::make_pair("Range", tmp.str()));
		auto rsp = cli.Put(uri.c_str(), hdr, body, "text/plain");
		if (rsp->status == 200)
		{
			*res = true;
		}
		else
		{
			*res = false;
		}
		*/
		return;
	}
	//判断文件是否需要备份

	bool FilesIsNeedBackUp(const std::string& file)
	{
		std::string etag;
		if (GetFileEtag(file, etag) == false)
		{
			return false;
		}
		std::unordered_map<std::string, std::string>::const_iterator it = _backup_list.find(file);
		if (it != _backup_list.end() && it->second == etag)
		{
			return false;
		}
		return true;
	}
	bool GetFileEtag(const std::string file, std::string& etag)
	{
		bf::path path(file);
		if (!bf::exists(path))
		{
			std::cerr << "get file " << file << " etag error" << std::endl;
			return false;
		}	
		int64_t fsize = bf::file_size(path);
		int64_t mtime = bf::last_write_time(path);
		std::stringstream ss;
		ss << std::hex << mtime << "-" << std::hex << fsize;
		etag = ss.str();
		return true;
	}
public:
	bool Start()
	{
		//1、获取备份信息
		GetBackupInfo();
		while (1)
		{
			//2、监听文件信息
			BackupDirListen(CLIENT_BACKUP_DIR);
			//3、备份文件信息
			SetBackupInfo();
		}
		return true;
	}
};
