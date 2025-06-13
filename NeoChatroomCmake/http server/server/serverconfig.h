#ifndef SERVERCONFIG_H
#define SERVERCONFIG_H

#include <string>

// 服务器配置全局变量
extern std::string CURRENT_HOST;          // 服务器主机地址
extern int CURRENT_PORT;                  // 服务器端口号
extern std::string ControlLoginPassword;  // 管理员登录密码
extern int ROOMDATA_CACHE_SECONDS;        // 聊天室数据缓存时间（单位：秒）

// 加载配置文件，初始化服务器配置
void loadConfig();

#endif // SERVERCONFIG_H
