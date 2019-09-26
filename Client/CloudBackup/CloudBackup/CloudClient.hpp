#pragma once
#include <iostream>
#include <boost/filesystem.hpp>
#include <boost/algorithm/string.hpp>
#include <unordered_map>
#include <string>
#include <fstream>
#define CLIENT_BACKUP_DIR "backup"
#define CLIENT_BACKUP_INFO_FILE "back.list"
namespace bf = boost::filesystem;
class CloudCLient
{
private:
	std::unordered_map<std::string, std::string> _backup_list;//存放备份信息
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
		boost::split(list, body, "\n");
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
	}
	//监听目录获取目录文件信息，并且刷新备份信息
	bool BackupDirListen()
	{

	}
	//备份file文件，上传文件数据
	bool PutFileData(const std::string& file)
	{

	}
	//判断文件是否需要备份
	bool FilesNeedBackUp(const std::string& file)
	{

	}
public:
	bool Start()
	{
		//1、获取备份信息
		GetBackupInfo();
		while (1)
		{
			//2、监听文件信息
			BackupDirListen();
			//3、备份文件信息
			SetBackupInfo();
		}
		return true;
	}
};
