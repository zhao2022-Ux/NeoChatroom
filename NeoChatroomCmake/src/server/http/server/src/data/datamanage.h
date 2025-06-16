#pragma once
#ifndef DATAMANAGE_H
#define DATAMANAGE_H

#include "../tool/tool.h"
#include "../config/config.h"
#include<map>
#include <string>
#include <vector>
#include <tuple>
#include <string>
#include "../../../../../lib/httplib.h" // 添加 httplib 头文件

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

		void setcookie(string new_cookie); // Updates cookie in memory and database
		void ban(); // Updates labei in memory and database

		bool setname(string str);
		int getuid();
		bool operator <(user x);
		user(string name_ = "NULL", string password_ = "", string cookie_ = "", string labei_ = "NULL");
		void setuid(int value = -1);
	};
	//向数据中新增用户
	bool AddUser(const std::string& name, const std::string& psw, const std::string& cookie, const std::string& labei); // Updated signature
	//通过uid查找用户
	user* FindUser(int uid);
	//通过id查找用户
	user* FindUser(string name);
	//移除用户
	bool RemoveUser(int uid);
	bool BanUser(int uid);
	//检查是否在聊天室中
	bool checkInRoom(int number, int uid);

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

	// Database management functions
	void InitDatabase(const std::string& dbPath);
	void CloseDatabase();

	// Function to invalidate user cache
	void InvalidateUserCache(int uid, const std::string& name);
	
	// 解析 Cookie (从 chatroom 移动过来)
	void transCookie(std::string& password, std::string& uid, std::string cookie);
	
	// 获取用户名API (从 chatroom 移动过来)
	void getUsername(const httplib::Request& req, httplib::Response& res);
}

#endif
