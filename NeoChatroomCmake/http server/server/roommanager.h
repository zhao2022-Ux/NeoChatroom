#pragma once

#include "../include/Server.h"
#include <vector>
#include <string>
#include <ctime>
#include <deque>
#include "../../json/json.h"
#include "../include/datamanage.h"
#include "../include/tool.h"
using namespace std;
#include <filesystem>
#include "chatroom.h"
#include <thread>
#include <chrono>

// 全局变量声明
extern vector<chatroom> room;        // 聊天室数组
extern bool used[MAXROOM];          // 聊天室使用状态标记

// 基本房间操作函数
int addroom();                      // 添加新聊天室
void start_manager();               // 启动聊天室管理器

// 缓存管理相关函数
void startCacheManager();           // 启动缓存管理器
void cacheManagerThread();          // 缓存管理线程函数

// 聊天室编辑函数
int editroom(int x, string Roomtittle = "");    // 编辑聊天室标题
int setroomtype(int x, int type);               // 设置聊天室类型

// 房间操作结果枚举
enum class RoomOperation {
    JOIN,   // 加入房间
    QUIT    // 退出房间
};

// 房间操作结果状态枚举
enum class RoomResult {
    Success,            // 操作成功
    UserNotFound,       // 用户不存在
    RoomNotFound,       // 房间不存在
    PasswordMismatch,   // 密码不匹配
    RoomAlreadyAdded,   // 已在房间中
    RoomNotJoined      // 未加入房间
};

// 房间操作函数
RoomResult QuitRoomToUser(int uid, int roomId);     // 用户退出房间
RoomResult AddRoomToUser(int uid, int roomId, const std::string& passwordHash);  // 用户加入房间
void editRoomToUserRoute(const httplib::Request& req, httplib::Response& res);   // 处理房间编辑请求

// 获取房间详细信息
std::vector<std::tuple<int, std::string, std::string>> GetRoomListDetails();

// API端点函数
void getCombinedRoomData(const httplib::Request& req, httplib::Response& res);  // 获取组合房间数据
bool verifyCookiePassword(int uid, string password);                            // 验证Cookie密码