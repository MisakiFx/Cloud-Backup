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
#define CLIENT_BACKUP_DIR ".\\backup"
#define CLIENT_BACKUP_INFO_FILE ".\\back.list"
#define RANGE_MAX_SIZE (10 << 20)//�ֿ��С
#define SERVER_IP "192.168.239.128"
#define SERVER_PORT 9000
#define BACKUP_URI "/list/"
namespace bf = boost::filesystem;


class ThrBackUp
{
private:
	std::string _file;
	int64_t _range_start;//������ʼλ��
	int64_t _range_len;//���ݳ���
public:
	bool _res;
public:
	ThrBackUp(const std::string& file, int64_t start, int64_t len)
		:_res(true)
		,_file(file)
		,_range_start(start)
		,_range_len(len)
	{

	}
	 void Start()
	{
		std::ifstream path(_file, std::ios::binary);
		if (!path.is_open())
		{
			std::cerr << "range backup file " << _file << " error" << std::endl;
			_res = false;
			return;
		}
		path.seekg(_range_start, std::ios::beg);
		std::string body;
		body.resize(_range_len);
		path.read(&body[0], _range_len);
		if (!path.good())
		{
			std::cerr << "read file " << _file << " range data error" << std::endl;
			_res = false;
			return;
		}
		path.close();
		bf::path name(_file);
		std::string uri = BACKUP_URI + name.filename().string();
		httplib::Client cli(SERVER_IP, SERVER_PORT);
		httplib::Headers hdr;
		hdr.insert(std::make_pair("Content-Length", std::to_string(_range_len)));
		std::stringstream tmp;
		tmp << "bytes=" << _range_start << "-" << (_range_start + _range_len - 1);
		hdr.insert(std::make_pair("Range", tmp.str()));
		auto rsp = cli.Put(uri.c_str(), hdr, body, "text/plain");
		if (!rsp || rsp->status != 200)
		{
			_res = false;
			std::stringstream ss;
			ss << "backup file [" << _file << "] range:[" << _range_start << "-" << _range_len << "] backup failed!" << std::endl;
			std::cout << ss.str();
			return;
		}
		std::stringstream ss;
		ss << "backup file [" << _file << "] range:[" << _range_start << "-" << _range_len << "] backup success!" << std::endl;
		std::cout << ss.str();
		return;
	}
};
class CloudClient
{
private:
	//filename etag
	std::unordered_map<std::string, std::string> _backup_list;//��ű�����Ϣ
	//˽�г�Ա����
private:
	//��ȡ�ļ�������Ϣ�������ж���Щ�ļ���Ҫ������Щ���豸��
	bool GetBackupInfo()
	{
		bf::path path(CLIENT_BACKUP_INFO_FILE);
		//�ļ�������
		if (!bf::exists(path))
		{
			std::cerr << "list file " << path.string() << " is not exist" << std::endl;
			return false;
		}
		int64_t fsize = bf::file_size(path);
		//�ļ�Ϊ��
		if (fsize == 0)
		{
			std::cerr << "have no backup info" << std::endl;
			return false;
		}
		//filename1 etag\n
		//��ȡ�ļ�
		std::string body;
		body.resize(fsize);
		std::ifstream file(path.string(), std::ios::binary);
		//�ļ���ʧ��
		if (!file.is_open())
		{
			std::cerr << "list file open error" << std::endl;
			return false;
		}
		file.read(&body[0], fsize);
		//�ļ���ȡʧ��
		if (!file.good())
		{
			std::cerr << "read list file error" << std::endl;
			return false;
		}
		file.close();
		//�ָ��ַ���ȡ��һ��һ���ļ�������
		//���ݸ�ʽ����:filename etag\n
		std::vector<std::string> list;
		boost::split(list, body, boost::is_any_of("\n"));
		//��ȡ�ļ���Ϣ����_backup_list��
		for (auto i : list)
		{
			size_t pos = i.find(" ");
			//��ʽ����ֱ������
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
	//���ļ��б����ļ�������Ϣ
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
	//����Ŀ¼��ȡĿ¼�ļ���Ϣ������ˢ�±�����Ϣ
	bool BackupDirListen(const std::string& path)
	{
		bf::path file(path);
		bf::directory_iterator item_begin(file);
		bf::directory_iterator item_end;
		//����Ŀ¼
		for (; item_begin != item_end; item_begin++)
		{
			//�����Ŀ¼��ݹ����
			if (bf::is_directory(item_begin->status()))
			{
				BackupDirListen(item_begin->path().string());
				continue;
			}
			//�ж��Ƿ���Ҫ����
			if (FilesIsNeedBackUp(item_begin->path().string()) == false)
			{
				continue;
			}
			std::cout << "file:[" << item_begin->path().string() << "] need backup" << std::endl;
			//�ж��ϴ��Ƿ�ɹ�
			if (PutFileData(item_begin->path().string()) == false)
			{
				continue;
			}
			
			//���±�����Ϣ��backup_list
			AddBackupInfo(item_begin->path().string());
		}
		return true;
	}

	//���±�����Ϣetag��backup_list
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
	//����file�ļ����ϴ��ļ�����
	//���̷ֿ߳��ϴ��ļ�
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
		std::vector<ThrBackUp> thr_res;
		std::vector<std::thread> thr_list;
		//�ֿ鴴���̴߳����ļ�
		std::cout << "file:[" << file << "] fsize:[" << fsize << "] count:[" << count + 1 << "]" << std::endl;
		for (int i = 0; i <= count; i++)
		{
			int64_t range_start = i * RANGE_MAX_SIZE;
			int64_t range_end = ((i + 1) * RANGE_MAX_SIZE) - 1;
			if (i == count)
			{
				range_end = fsize - 1;
			}
			int64_t range_len = range_end - range_start + 1;
			std::cout << "file:[" << file << "] range:[" << range_start << "-" << range_end << "]-" << range_len << std::endl;
			ThrBackUp backip_info(file, range_start, range_len);
			thr_res.push_back(backip_info);
			//thr_list.push_back(std::thread(thr_start, &thr_res[i]));
		}
		for (int i = 0; i <= count; i++)
		{
			thr_list.push_back(std::thread(thr_start, &thr_res[i]));
		}
		//�ȴ�ÿ���߳̽��������ݽ���ж��Ƿ���ɹ�
		bool ret = true;
		for (int i = 0; i <= count; i++)
		{
			thr_list[i].join();
			if (thr_res[i]._res == true)
			{
				continue;
			}
			ret = false;
		}
		if (ret == false)
		{
			//AddBackupInfo(file);
			std::cout << "file:[" << file << "] backup failed" << std::endl;
			return false;
		}
		std::cout << "file:[" << file << "] backup success" << std::endl;
		return true;
	}
	//�߳���ں����������洫��ÿһ���ļ�����
	static void thr_start(ThrBackUp* backup_info)
	{
		backup_info->Start();
		return;
	}
	//�ж��ļ��Ƿ���Ҫ����

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
		//1����ȡ������Ϣ
		GetBackupInfo();
		while (1)
		{
			//2�������ļ���Ϣ
			BackupDirListen(CLIENT_BACKUP_DIR);
			//3�������ļ���Ϣ
			SetBackupInfo();
			Sleep(3000);
		}
		return true;
	}
	CloudClient()
	{
		//�����ļ��в����ڣ������ļ���
		bf::path file(CLIENT_BACKUP_DIR);
		if (!bf::exists(file))
		{
			bf::create_directory(file);
		}
	}
};
