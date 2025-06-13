//
// Created by seve on 25-6-4.
//

#ifndef SERVERCONFIG_H
#define SERVERCONFIG_H

#include <string>

// 服务器配置变量
extern std::string CURRENT_HOST;
extern int CURRENT_PORT;
extern std::string ControlLoginPassword;
extern int ROOMDATA_CACHE_SECONDS; // 聊天室数据API缓存时间（秒）

// 配置文件相关函数声明
void loadConfig();

#endif // SERVERCONFIG_H
