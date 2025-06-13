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
#include <sqlite3.h>
#include "chatDBmanager.h"
#include "serverconfig.h"
// Ensure room initialization logic is robust
vector<chatroom> room(MAXROOM);
bool used[MAXROOM] = { false };

// 缓存管理线程函数
void cacheManagerThread() {
    Logger& logger = Logger::getInstance();
    logger.logInfo("CacheManager", "聊天室缓存管理器已启动");
    
    while (true) {
        // 每分钟检查一次
        std::this_thread::sleep_for(std::chrono::minutes(1));
        
        try {
            time_t currentTime = time(0);
            int unloadedCount = 0;
            
            for (int i = 1; i < MAXROOM; i++) {
                if (used[i]) {
                    // 如果聊天室的消息已加载且最后访问时间超过设定阈值
                    if (room[i].isMessagesLoaded() && (currentTime - room[i].getLastAccessTime() > ROOMDATA_CACHE_SECONDS)) {
                        // 使用内部方法卸载消息
                        room[i].unloadMessages();
                        unloadedCount++;
                    }
                }
            }
            
            if (unloadedCount > 0) {
                logger.logInfo("CacheManager", "已卸载 " + std::to_string(unloadedCount) + " 个聊天室的消息缓存");
            }
        } catch (const std::exception& e) {
            logger.logError("CacheManager", "缓存管理异常: " + std::string(e.what()));
        } catch (...) {
            logger.logError("CacheManager", "缓存管理未知异常");
        }
    }
}

void startCacheManager() {
    static std::thread cacheThread(cacheManagerThread);
    cacheThread.detach();
}

// 从数据库加载聊天室信息
void loadChatroomsFromDB() {
    Logger& logger = Logger::getInstance();
    ChatDBManager& dbManager = ChatDBManager::getInstance();
    
    logger.logInfo("roommanager", "开始从数据库加载聊天室...");
    
    std::vector<int> roomIds = dbManager.getAllChatRoomIds();
    logger.logInfo("roommanager", "找到 " + std::to_string(roomIds.size()) + " 个聊天室记录");
    
    for (int id : roomIds) {
        if (id >= 1 && id < MAXROOM) {
            std::string title, passwordHash, password;
            unsigned int flags;
            
            if (dbManager.getChatRoom(id, title, passwordHash, password, flags)) {
                if (!used[id]) {
                    used[id] = true;
                    room[id].init();
                    room[id].setRoomID(id);
                    room[id].settittle(title);
                    room[id].setPasswordHash(passwordHash);
                    room[id].setPassword(password);
                    room[id].setflag(flags);
                    
                    // 启动聊天室
                    if (!room[id].start()) {
                        logger.logError("roommanager", "无法启动聊天室 ID: " + std::to_string(id));
                    } else {
                        logger.logInfo("roommanager", "已从数据库加载聊天室 ID: " + std::to_string(id) + ", 标题: " + title);
                    }
                }
            } else {
                logger.logWarning("roommanager", "无法从数据库获取聊天室信息，ID: " + std::to_string(id));
            }
        }
    }
    
    logger.logInfo("roommanager", "聊天室加载完成");
}

int addroom() {
    ChatDBManager& dbManager = ChatDBManager::getInstance();
    
    for (int i = 1; i < MAXROOM; i++) {
        if (!used[i]) {
            used[i] = true;
            room[i].init();
            room[i].setRoomID(i);
            
            // 将新聊天室添加到数据库
            dbManager.createChatRoom(i, "", "", "", 0);
            
            return i;
        }
    }
    return -1;
}

void delroom(int x) {
    if (x >= 1 && x < MAXROOM && used[x]) {
        ChatDBManager& dbManager = ChatDBManager::getInstance();
        dbManager.deleteChatRoom(x);
        
        used[x] = false;
        room[x].init();
    }
    else {
        Logger& logger = Logger::getInstance();
        logger.logWarning("chatroom::roomManager", "未能删除");
    }
}

int editroom(int x, string Roomtittle) {
    if (x >= room.size()) return 1;
    room[x].settittle(Roomtittle);
    
    // 更新数据库
    ChatDBManager& dbManager = ChatDBManager::getInstance();
    std::string title = room[x].gettittle();
    std::string passwordHash = room[x].getPasswordHash();
    std::string password = room[x].GetPassword();
    unsigned int flags = 0;
    if (room[x].hasFlag(chatroom::ROOM_HIDDEN)) flags |= chatroom::ROOM_HIDDEN;
    if (room[x].hasFlag(chatroom::ROOM_NO_JOIN)) flags |= chatroom::ROOM_NO_JOIN;
    
    dbManager.updateChatRoom(x, title, passwordHash, password, flags);
    
    return 0;
}

int setroomtype(int x, int type) {
    if (x >= room.size()) return 1;
    room[x].setflag(type);
    
    // 更新数据库
    ChatDBManager& dbManager = ChatDBManager::getInstance();
    std::string title = room[x].gettittle();
    std::string passwordHash = room[x].getPasswordHash();
    std::string password = room[x].GetPassword();
    unsigned int flags = type; // 直接使用传入的flags
    
    dbManager.updateChatRoom(x, title, passwordHash, password, flags);
    
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
    Json::Value response(Json::arrayValue);

    for (int i = 1; i < MAXROOM; i++) {
        if (!used[i]) continue;

        // 跳过隐藏的聊天室
        if (room[i].hasFlag(chatroom::RoomFlags::ROOM_HIDDEN)) continue;

        Json::Value roomObj;
        roomObj["id"] = std::to_string(i);
        roomObj["name"] = room[i].gettittle();
        response.append(roomObj);
    }

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
    RoomNotJoined,
    RoomUnableJoin
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
    // 跳过隐藏的聊天室
    if (room[roomId].hasFlag(chatroom::RoomFlags::ROOM_NO_JOIN)) {
        return RoomResult::RoomUnableJoin;
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

bool verifyCookiePassword(int uid, string password) {
    Logger& logger = logger.getInstance();
    manager::user nowuser = *manager::FindUser(uid);
    if (nowuser.getpassword() != password) {
        return false;
    }
    return true;
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

    if (!verifyCookiePassword(uid_, password)) {
        res.status = 403;
        res.set_content("Invalid cookie authentication", "text/plain");
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
    }
    else {
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
    case RoomResult::RoomUnableJoin:
        res.status = 403;
		res.set_content("Room Unable to Join", "text/plain");
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

void getChatroomName(const httplib::Request& req, httplib::Response& res) {
    // Validate if the roomId parameter exists
    if (!req.has_param("roomId")) {
        res.status = 400; // Bad Request
        res.set_content("Missing roomId parameter", "text/plain");
        return;
    }

    // Parse the roomId parameter
    std::string roomIdStr = req.get_param_value("roomId");
    int roomId;
    if (!str::safeatoi(roomIdStr, roomId)) {
        res.status = 400; // Bad Request
        res.set_content("Invalid roomId format", "text/plain");
        return;
    }

    // Check if the roomId is valid and in use
    if (roomId < 1 || roomId >= MAXROOM || !used[roomId]) {
        res.status = 404; // Not Found
        res.set_content("Room not found", "text/plain");
        return;
    }

    // Retrieve the room name
    std::string roomName = room[roomId].gettittle();

    // Respond with the room name
    Json::Value response;
    response["id"] = roomIdStr;
    response["name"] = roomName;

    res.set_header("Access-Control-Allow-Origin", "*");
    res.set_header("Content-Type", "application/json");
    res.set_content(response.toStyledString(), "application/json");
}

// 获取组合的聊天室数据（新增API端点实现）
void getCombinedRoomData(const httplib::Request& req, httplib::Response& res) {
    Logger& logger = Logger::getInstance();
    logger.logInfo("roommanager", "获取组合聊天室数据");
    
    // 获取用户ID
    std::string cookies = req.get_header_value("Cookie");
    std::string password, uid;
    transCookie(password, uid, cookies);
    int uid_;
    if (!str::safeatoi(uid, uid_)) {
        logger.logWarning("roommanager", "获取组合聊天室数据 - 无效的UID格式");
        res.status = 401;
        res.set_content("未授权访问", "text/plain");
        return;
    }

    // 验证用户密码
    if (!verifyCookiePassword(uid_, password)) {
        logger.logWarning("roommanager", "获取组合聊天室数据 - 无效的密码验证");
        res.status = 403;
        res.set_content("无效的认证信息", "text/plain");
        return;
    }

    // 构建响应数据
    Json::Value response;
    
    // 获取用户已加入的聊天室
    manager::user* user = manager::FindUser(uid_);
    if (user != nullptr) {
        std::string cookie = user->getcookie();
        std::vector<int> joinedRoomIds;
        
        std::stringstream ss(cookie);
        std::string token;
        while (std::getline(ss, token, '&')) {
            int roomId;
            if (str::safeatoi(token, roomId)) {
                joinedRoomIds.push_back(roomId);
            }
        }
        
        // 添加用户已加入的聊天室信息
        Json::Value joinedRooms(Json::arrayValue);
        for (int roomId : joinedRoomIds) {
            if (roomId >= 1 && roomId < MAXROOM && used[roomId]) {
                Json::Value roomData;
                roomData["id"] = roomId;
                roomData["name"] = room[roomId].gettittle();
                joinedRooms.append(roomData);
            }
        }
        response["joinedRooms"] = joinedRooms;
    }
    
    // 获取所有可用的聊天室
    Json::Value allRooms(Json::arrayValue);
    for (int i = 1; i < MAXROOM; i++) {
        if (used[i]) {
            // 根据flags判断是否显示
            bool isHidden = room[i].hasFlag(chatroom::RoomFlags::ROOM_HIDDEN);
            bool isNoJoin = room[i].hasFlag(chatroom::RoomFlags::ROOM_NO_JOIN);
            
            if (!isHidden) {
                Json::Value roomData;
                roomData["id"] = i;
                roomData["name"] = room[i].gettittle();
                roomData["flags"] = (int)room[i].getFlags();
                roomData["hasPassword"] = !room[i].getPasswordHash().empty();
                allRooms.append(roomData);
            }
        }
    }
    response["allRooms"] = allRooms;
    
    // 设置响应头和内容
    res.set_header("Content-Type", "application/json");
    res.set_header("Cache-Control", "private, max-age=" + std::to_string(ROOMDATA_CACHE_SECONDS));
    res.set_content(response.toStyledString(), "application/json");
    
    logger.logInfo("roommanager", "组合聊天室数据获取成功，用户ID: " + uid);
}

void start_manager() {
    Logger& logger = Logger::getInstance();
    
    try {
        // 从数据库加载聊天室
        logger.logInfo("roommanager", "开始初始化聊天室管理器...");
        loadChatroomsFromDB();
        
        // 启动缓存管理器
        startCacheManager();
        logger.logInfo("roommanager", "已启动聊天室缓存管理器");
    } catch (const std::exception& e) {
        logger.logError("roommanager", "加载聊天室时发生异常: " + std::string(e.what()));
    } catch (...) {
        logger.logError("roommanager", "加载聊天室时发生未知异常");
    }
    
    Server& server = Server::getInstance(HOST);
    server.handleRequest("/list", getRoomList);
    server.handleRequest("/allchatlist", getAllList);
    server.handleRequest("/joinquitroom", editRoomToUserRoute); // Updated route
    server.handleRequest("/chatroomname", getChatroomName); // New route
    server.handleRequest("/roomdata", getCombinedRoomData); // 新增的组合数据API


        // 提供图片文件 /logo.png
        server.getInstance().handleRequest("/logo.png", [](const httplib::Request& req, httplib::Response& res) {
            std::ifstream logoFile("html/logo.png", std::ios::binary);
            if (logoFile) {
                std::stringstream buffer;
                buffer << logoFile.rdbuf();
                res.set_content(buffer.str(), "image/png");
            }
            else {
                res.status = 404;
                res.set_content("Logo not found", "text/plain");
            }
            });

        // 提供 JS 文件 /chat/js
        server.getInstance().handleRequest("/chat/js", [](const httplib::Request& req, httplib::Response& res) {
            std::ifstream jsFile("html/chatroom.js", std::ios::binary);
            if (jsFile) {
                std::stringstream buffer;
                buffer << jsFile.rdbuf();
                res.set_content(buffer.str(), "application/javascript; charset=gbk"); // 修改编码
            }
            else {
                res.status = 404;
                res.set_content("chatroom.js not found", "text/plain");
            }
            });

        // 提供 JS 文件 /chatlist/js
        server.getInstance().handleRequest("/chatlist/js", [](const httplib::Request& req, httplib::Response& res) {
            std::ifstream jsFile("html/chatlist.js", std::ios::binary);
            if (jsFile) {
                std::stringstream buffer;
                buffer << jsFile.rdbuf();
                res.set_content(buffer.str(), "application/javascript; charset=gbk"); // 修改编码
            }
            else {
                res.status = 404;
                res.set_content("chatlist.js not found", "text/plain");
            }
            });


        // 添加 chatlist.html 的路由
        server.getInstance().serveFile("/chatlist", "html/chatlist.html"); // 确保路径正确

        // 提供图片文件 /images/*，动态路由
        server.getInstance().handleRequest(R"(/images/([^/]+))", [](const httplib::Request& req, httplib::Response& res) {
            std::string imagePath = "html/images/" + req.matches[1].str();  // 获取图片文件名
            std::ifstream imageFile(imagePath, std::ios::binary);

            if (imageFile) {
                std::stringstream buffer;
                buffer << imageFile.rdbuf();

                // 自动推测图片的 MIME 类型
                std::string extension = imagePath.substr(imagePath.find_last_of('.') + 1);
                std::string mimeType = "images/" + extension;

                res.set_content(buffer.str(), mimeType);
            }
            else {
                res.status = 404;
                res.set_content("Image not found", "text/plain");
            }
            });

        // 提供文件 /files/*，动态路由
        // 提供文件 /files/*，动态路由
        server.getInstance().handleRequest(R"(/files/([^/]+))", [](const httplib::Request& req, httplib::Response& res) {
            std::string filePath = "html/files/" + req.matches[1].str();  // 获取文件名
            std::ifstream file(filePath, std::ios::binary);

            if (file) {
                std::stringstream buffer;
                buffer << file.rdbuf();

                // 自动推测文件的 MIME 类型
                std::string mimeType = Server::detectMimeType(filePath);

                res.set_content(buffer.str(), mimeType);
            }
            else {
                res.status = 404;
                res.set_content("File not found", "text/plain");
            }
            });
}