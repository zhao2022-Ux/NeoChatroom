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

extern vector<chatroom> room;

extern bool used[MAXROOM];
int addroom();

void start_manager();

int editroom(int x, string Roomtittle = "");

int setroomtype(int x, int type);

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
    RoomNotJoined
};

RoomResult QuitRoomToUser(int uid, int roomId);
RoomResult AddRoomToUser(int uid, int roomId, const std::string& passwordHash);
void editRoomToUserRoute(const httplib::Request& req, httplib::Response& res);

// Add this function declaration to the header file
std::vector<std::tuple<int, std::string, std::string>> GetRoomListDetails();

