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
	std::unordered_map<std::string, std::string> _backup_list;//��ű�����Ϣ
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
		boost::split(list, body, "\n");
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
	//�����ļ�������Ϣ
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
	//����Ŀ¼��ȡĿ¼�ļ���Ϣ������ˢ�±�����Ϣ
	bool BackupDirListen()
	{

	}
	//����file�ļ����ϴ��ļ�����
	bool PutFileData(const std::string& file)
	{

	}
	//�ж��ļ��Ƿ���Ҫ����
	bool FilesNeedBackUp(const std::string& file)
	{

	}
public:
	bool Start()
	{
		//1����ȡ������Ϣ
		GetBackupInfo();
		while (1)
		{
			//2�������ļ���Ϣ
			BackupDirListen();
			//3�������ļ���Ϣ
			SetBackupInfo();
		}
		return true;
	}
};
