#pragma once

#include "../server/Server.h"
#include <vector>
#include <string>
#include <ctime>
#include <deque>
#include "json/json.h"
#include "../data/datamanage.h"
#include "../tool/tool.h"
using namespace std;
#include <filesystem>
#include "chatroom.h"
#include "privatechat.h"
#include <thread>
#include <chrono>
#include <atomic>
#include <mutex>

extern vector<chatroom> room;

extern bool used[MAXROOM];
int addroom();

void start_manager();

int editroom(int x, string Roomtittle = "");

int setroomtype(int x, int type);

// 缓存相关函数声明
void updateRoomListCache();
void invalidateRoomCache();

// 懒加载相关函数声明
void startRoomMonitor();
bool activateRoom(int roomId);
void checkInactiveRooms();
void monitorRooms();

// 懒加载配置参数
const int ROOM_INACTIVITY_TIMEOUT = 3600; // 聊天室闲置超时时间（秒）
const int ROOM_CHECK_INTERVAL = 300;     // 检查闲置聊天室的间隔（秒）

// 懒加载监控线程控制
extern std::atomic<bool> monitorThreadRunning;
extern std::mutex monitorThreadMutex;

// Enum to represent the result of AddRoomToUser
enum class AddRoomResult {
    Success,
    UserNotFound,
    RoomNotFound,
    PasswordMismatch,
    RoomAlreadyAdded
};

// Add these enums and function declarations
enum class RoomOperation {
    JOIN,
    QUIT
};

enum class RoomResult {
    Success,
    UserNotFound,
    RoomNotFound,
    PasswordMismatch,
    RoomAlreadyAdded,
    RoomNotJoined,
    RoomUnableJoin
};

RoomResult QuitRoomToUser(int uid, int roomId);
RoomResult AddRoomToUser(int uid, int roomId, const std::string& passwordHash);
void editRoomToUserRoute(const httplib::Request& req, httplib::Response& res);

// Add this function declaration to the header file
std::vector<std::tuple<int, std::string, std::string>> GetRoomListDetails();