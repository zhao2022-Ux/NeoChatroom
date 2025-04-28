#include "../../include/Server.h"
#include <vector>
#include <string>
#include <ctime>
#include <deque>
#include "../../json/json.h"
#include "../../include/datamanage.h"
#include "../../include/tool.h"
using namespace std;
#include <filesystem>
#include "chatroom.h"

// Ensure room initialization logic is robust
vector<chatroom> room(MAXROOM);
bool used[MAXROOM] = { false };

int addroom() {
    for (int i = 1; i < MAXROOM; i++) {
        if (!used[i]) {
            used[i] = true;
            room[i].init();
            room[i].setRoomID(i);
            return i;
        }
    }
    return -1;
}

void delroom(int x) {
    if (x >= 1 && x < MAXROOM && used[x]) {
        used[x] = false;
        room[x].init();
    }
    else {
        Logger& logger = logger.getInstance();
        logger.logWarning("chatroom::roomManager", "未能删除");
    }
}

int editroom(int x, string Roomtittle) {
    if (x >= room.size()) return 1;
    room[x].settittle(Roomtittle);
    return 0;
}

int setroomtype(int x, int type) {
    if (x >= room.size()) return 1;
    room[x].settype(type);
    return 0;
}

/*
 * Modified transCookie to handle cookie strings that may simply be the UID
 * instead of the expected "uid=..." key=value format.
 */
void transCookie(std::string& cid, std::string& uid, const std::string& cookie) {
    // First, attempt to extract clientid if present.
    std::string::size_type pos1 = cookie.find("clientid=");
    if (pos1 != std::string::npos) {
        pos1 += 9; // Skip over "clientid="
        std::string::size_type pos2 = cookie.find(";", pos1);
        if (pos2 == std::string::npos) pos2 = cookie.length();
        cid = cookie.substr(pos1, pos2 - pos1);
    }

    // Attempt to extract uid using "uid=" pattern.
    std::string::size_type pos3 = cookie.find("uid=");
    if (pos3 != std::string::npos) {
        pos3 += 4; // Skip over "uid="
        std::string::size_type pos4 = cookie.find(";", pos3);
        if (pos4 == std::string::npos) pos4 = cookie.length();
        uid = cookie.substr(pos3, pos4 - pos3);
    }
    else {
        // If no "uid=" pattern found and cookie does not contain "=",
        // assume the entire cookie string is the uid.
        if (cookie.find('=') == std::string::npos && !cookie.empty()) {
            uid = cookie;
        }
    }
}

string getRoomName(int roomid) {
    return room[roomid].gettittle();
}

void getRoomList(const httplib::Request& req, httplib::Response& res) {
    // 获取 Cookie 并解析
    std::string cookies = req.get_header_value("Cookie");
    std::string password, uid;
    transCookie(password, uid, cookies);

    // If uid is still empty and cookies is non-empty, try to use cookies as uid.
    if (uid.empty() && !cookies.empty() && cookies.find('=') == std::string::npos) {
        uid = cookies;
    }

    // 转换 uid 为整数
    int uid_;
    if (!str::safeatoi(uid, uid_)) {
        res.status = 400; // Bad Request
        res.set_content("Invalid UID format", "text/plain");
        return;
    }

    // 查找用户
    manager::user* user = manager::FindUser(uid_);
    if (user == nullptr) {
        res.status = 404; // Not Found
        res.set_content("User not found", "text/plain");
        return;
    }

    // 获取用户的聊天室 ID 列表
    std::string List = user->getcookie();
    if (List.empty()) {
        res.status = 200; // OK
        res.set_content("[]", "application/json"); // 返回空的 JSON 数组
        return;
    }

    // 分割 List 字符串，提取每个聊天室 ID
    std::vector<std::string> roomIds;
    std::stringstream ss(List);
    std::string roomId;
    while (std::getline(ss, roomId, '&')) {
        roomIds.push_back(roomId);
    }

    // 创建一个 JSON 响应对象
    Json::Value response(Json::arrayValue);

    // 遍历所有聊天室 ID，获取对应的聊天室名称
    for (const std::string& roomId : roomIds) {
        int room_id;
        if (!str::safeatoi(roomId, room_id)) {
            continue; // 跳过无效的聊天室 ID
        }

        std::string roomName = getRoomName(room_id);

        // 构造每个聊天室的 JSON 对象
        Json::Value room;
        room["id"] = roomId;  // 添加聊天室的 ID
        room["name"] = roomName;  // 添加聊天室的名称

        response.append(room);  // 将聊天室对象添加到 JSON 数组中
    }

    // 设置响应头，支持跨域
    res.set_header("Access-Control-Allow-Origin", "*");
    res.set_header("Content-Type", "application/json");

    // 设置响应内容为 JSON 格式的字符串
    res.set_content(response.toStyledString(), "application/json");
}

void getAllList(const httplib::Request& req, httplib::Response& res) {
    // 创建 JSON 数组对象，用于存储所有要返回的聊天室
    Json::Value response(Json::arrayValue);

    // 遍历所有聊天室（假设聊天室编号从 1 开始）
    for (int i = 1; i < MAXROOM; i++) {
        // 如果该聊天室未被使用，则跳过
        if (!used[i]) continue;
        // 如果聊天室的 getype 返回 3（隐藏聊天室），跳过
        if (room[i].gettype() == 3) continue;

        // 构造聊天室 JSON 对象（这里只返回 id 和 tittle）
        Json::Value roomObj;
        roomObj["id"] = std::to_string(i);
        roomObj["name"] = room[i].gettittle();
        response.append(roomObj);
    }

    // 设置响应头，支持跨域
    res.set_header("Access-Control-Allow-Origin", "*");
    res.set_header("Content-Type", "application/json");
    res.set_content(response.toStyledString(), "application/json");
}

enum class RoomResult {
    Success,
    UserNotFound,
    RoomNotFound,
    PasswordMismatch,
    RoomAlreadyAdded,
    RoomNotJoined
};

enum class RoomOperation {
    JOIN,
    QUIT
};

RoomResult AddRoomToUser(int uid, int roomId, const std::string& passwordHash) {
    // Check if the user exists
    manager::user* user = manager::FindUser(uid);
    if (user == nullptr) {
        return RoomResult::UserNotFound;
    }

    // Check if the room exists and is in use
    if (roomId < 1 || roomId >= MAXROOM || !used[roomId]) {
        return RoomResult::RoomNotFound;
    }

    // Check if the password hash matches
    if (!room[roomId].getPasswordHash().empty() && room[roomId].getPasswordHash() != passwordHash) {
        return RoomResult::PasswordMismatch;
    }

    // Get the current cookie string
    std::string cookie = user->getcookie();
    std::string roomIdStr = std::to_string(roomId);

    // Check if the room is already added
    std::stringstream ss(cookie);
    std::string token;
    while (std::getline(ss, token, '&')) {
        if (token == roomIdStr) {
            return RoomResult::RoomAlreadyAdded;
        }
    }

    // Add the room to the user's cookie
    if (!cookie.empty()) {
        cookie += "&";
    }
    cookie += roomIdStr;
    user->setcookie(cookie);

    return RoomResult::Success;
}

RoomResult QuitRoomToUser(int uid, int roomId) {
    // Check if the user exists
    manager::user* user = manager::FindUser(uid);
    if (user == nullptr) {
        return RoomResult::UserNotFound;
    }

    // Check if the room exists
    if (roomId < 1 || roomId >= MAXROOM) {
        return RoomResult::RoomNotFound;
    }

    // Get the current cookie string
    std::string cookie = user->getcookie();
    std::string roomIdStr = std::to_string(roomId);

    // Split the cookie string and rebuild without the target room
    std::stringstream ss(cookie);
    std::string token;
    std::string newCookie;
    bool found = false;

    while (std::getline(ss, token, '&')) {
        if (token == roomIdStr) {
            found = true;
            continue;
        }
        if (!newCookie.empty()) {
            newCookie += "&";
        }
        newCookie += token;
    }

    if (!found) {
        return RoomResult::RoomNotJoined;
    }

    user->setcookie(newCookie);
    return RoomResult::Success;
}

void editRoomToUserRoute(const httplib::Request& req, httplib::Response& res) {
    // Parse the user ID from the cookie
    std::string cookies = req.get_header_value("Cookie");
    std::string password, uid;
    transCookie(password, uid, cookies);
    int uid_;
    if (!str::safeatoi(uid, uid_)) {
        res.status = 400;
        res.set_content("Invalid UID format", "text/plain");
        return;
    }

    // Parse the room ID from the request parameters
    if (!req.has_param("roomId")) {
        res.status = 400;
        res.set_content("Missing roomId parameter", "text/plain");
        return;
    }
    std::string roomIdStr = req.get_param_value("roomId");
    int roomId;
    if (!str::safeatoi(roomIdStr, roomId)) {
        res.status = 400;
        res.set_content("Invalid roomId format", "text/plain");
        return;
    }

    // Parse operation type
    if (!req.has_param("operation")) {
        res.status = 400;
        res.set_content("Missing operation parameter", "text/plain");
        return;
    }
    std::string op = req.get_param_value("operation");
    RoomOperation operation = (op == "join") ? RoomOperation::JOIN : RoomOperation::QUIT;

    RoomResult result;
    if (operation == RoomOperation::JOIN) {
        std::string passwordHash = req.has_param("passwordHash") ? req.get_param_value("passwordHash") : "";
        result = AddRoomToUser(uid_, roomId, passwordHash);
    } else {
        result = QuitRoomToUser(uid_, roomId);
    }
    Logger& logger = logger.getInstance();
    // Handle the result
    switch (result) {
        case RoomResult::Success:
            res.status = 200;
            res.set_content(operation == RoomOperation::JOIN ? "Joined successfully" : "Quit successfully", "text/plain");
			logger.logInfo("RoomManager", "用户 " + std::to_string(uid_) + " " + (operation == RoomOperation::JOIN ? "joined" : "quit") + " room " + std::to_string(roomId));
            break;
        case RoomResult::UserNotFound:
            res.status = 404;
            res.set_content("User not found", "text/plain");
			logger.logWarning("RoomManager", "用户 " + std::to_string(uid_) + " not found");
            break;
        case RoomResult::RoomNotFound:
            res.status = 404;
            res.set_content("Room not found", "text/plain");
			logger.logWarning("RoomManager", "Room " + std::to_string(roomId) + " not found");
            break;
        case RoomResult::PasswordMismatch:
            res.status = 403;
            res.set_content("Password mismatch", "text/plain");
			logger.logWarning("RoomManager", "密码错误 " + std::to_string(uid_) + " in room " + std::to_string(roomId));
            break;
        case RoomResult::RoomAlreadyAdded:
            res.status = 409;
            res.set_content("Room already added", "text/plain");
            break;
        case RoomResult::RoomNotJoined:
            res.status = 404;
            res.set_content("Room not joined", "text/plain");
            break;
    }
}

std::vector<std::tuple<int, std::string, std::string>> GetRoomListDetails() {
    std::vector<std::tuple<int, std::string, std::string>> roomDetails;
    
    // Iterate through all rooms
    for (int i = 1; i < MAXROOM; i++) {
        if (used[i]) {
            // Get room name and password
            std::string name = room[i].gettittle();
            std::string password = room[i].GetPassword();
            
            // Add tuple to vector
            roomDetails.emplace_back(i, name, password);
        }
    }
    
    return roomDetails;
}

void start_manager() {
    Server& server = Server::getInstance(HOST);
    server.handleRequest("/list", getRoomList);
    server.handleRequest("/allchatlist", getAllList);
    server.handleRequest("/joinquitroom", editRoomToUserRoute); // Updated route
}