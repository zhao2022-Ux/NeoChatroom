#pragma once
#ifndef DATAMANAGE_H
#define DATAMANAGE_H

#include "tool.h"
#include "config.h"
#include<map>
namespace manager {
	const string banedName = "封禁用户";
	const string AluserType = "AllUser";
	const string GMlabei = "GM";//管理员
	const string Usuallabei = "U";//普通用户
	const string BanedLabei = "BAN";//封禁用户
	//检查char是否安全
	bool SafeWord(char word);
	//检查用户名是否合法
	bool CheckUserName(string name);
	//存储数据的文件的路径
	const string datafile = "data.txt";
	extern int usernum;

	class user {
		string name, password;
		string cookie, labei;
		int uid;
	public:
		string getname();
		string getcookie();
		string getlabei();
		string getpassword();

		void setcookie(string new_cookie);
		void ban();

		bool setname(string str);
		int getuid();
		bool operator <(user x);
		user(string name_ = "NULL", string password_ = "", string cookie_ = "", string labei_ = "NULL");
		void setuid(int value = -1);
	};
	//向数据中新增用户
	bool AddUser(string name, string psw, string cookie, string labei, int uuid = -1);
	//通过uid查找用户
	user* FindUser(int uid);
	//通过id查找用户
	user* FindUser(string name);
	//移除用户
	bool RemoveUser(int uid);
	//读取时的缓冲区
	extern config DataFile;
	extern std::vector<item> list;
	//打印异常
	void LogError(string path, string filename, int line);
	//读取整个文件
	void ReadUserData(string path, string filename);
	//保存当前用户列表
	void WriteUserData(string path, string filename);

	// Declaration of the function to get user details
	std::vector<std::tuple<std::string, std::string, int>> GetUserDetails();
}
#endif