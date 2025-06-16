#include "privatechat.h"
#include "roommanager.h"
#include "../server/Server.h"
#include "../tool/tool.h"
#include "../data/datamanage.h"
#include <string>

// 初始化静态实例
PrivateChat* PrivateChat::instance = nullptr;

// 构造函数
PrivateChat::PrivateChat() : initialized(false) {
}

// 析构函数
PrivateChat::~PrivateChat() {
    // 清理工作
}

// 获取单例实例
PrivateChat& PrivateChat::getInstance() {
    if (instance == nullptr) {
        instance = new PrivateChat();
    }
    return *instance;
}

// 初始化私聊系统
bool PrivateChat::init() {
    std::lock_guard<std::mutex> lock(mtx);
    
    if (initialized) {
        return true;  // 已经初始化过
    }
    
    try {
        // 检查room[0]是否已被初始化
        if (used[0]) {
            // 私聊室已经存在，检查配置但不重新初始化内容
        } else {
            used[0] = true;
            room[0].setRoomID(0);  // 设置特殊ID为0
            room[0].settittle("私聊系统");  // 设置标题
            room[0].setFlag(chatroom::ROOM_HIDDEN);  // 设置为隐藏聊天室
            // 注意：不要调用room[0].init()，这会清空消息
        }
        
        // 检查数据库中是否已存在私聊室
        ChatDBManager& dbManager = ChatDBManager::getInstance();
        std::string title, passwordHash, password;
        unsigned int flags;
        
        if (dbManager.getChatRoom(0, title, passwordHash, password, flags)) {
            // 如果数据库中已存在私聊室记录，使用这些设置
            room[0].settittle(title);
            room[0].setPasswordHash(passwordHash);
            room[0].setPassword(password);
            // 修复类型转换问题，使用正确的枚举类型
            room[0].setflag(flags);  // 使用现有标志，保留所有设置
        } else {
            // 如果数据库中不存在记录，创建新记录
            dbManager.createChatRoom(0, "私聊系统", "", "", chatroom::ROOM_HIDDEN);
        }
        
        initialized = true;
        return true;
        
    } catch (const std::exception& e) {
        return false;
    } catch (...) {
        return false;
    }
}

// 启动私聊系统
bool PrivateChat::start() {
    if (!initialized && !init()) {
        return false;
    }
    
    try {
        // 启动私聊室
        if (!room[0].start()) {
            return false;
        }
        
        // 设置私聊API路由
        setupRoutes();
        
        return true;
        
    } catch (const std::exception& e) {
        return false;
    } catch (...) {
        return false;
    }
}

// 发送系统消息到私聊室
void PrivateChat::sendSystemMessage(const std::string& message) {
    if (!initialized) {
        return;
    }
    
    room[0].systemMessage(message);
}

// 获取用户间的私聊消息 - 使用更准确的查询
bool PrivateChat::getUserMessages(const std::string& fromUser, const std::string& toUser, 
                                 std::vector<Json::Value>& messages, long long lastTimestamp) {
    if (!initialized) {
        return false;
    }
    
    // 使用专门的方法获取两个用户之间的私聊消息
    ChatDBManager& dbManager = ChatDBManager::getInstance();
    std::deque<Json::Value> tempMessages;
    
    // 从数据库获取两个用户之间的所有消息
    if (!dbManager.getPrivateMessagesBetweenUsers(0, fromUser, toUser, tempMessages, lastTimestamp)) {
        Logger::getInstance().logError("PrivateChat", "获取私聊消息失败");
        return false;
    }
    
    // 将消息复制到输出向量
    messages.clear();
    for (const auto& msg : tempMessages) {
        messages.push_back(msg);
    }
    
    // 将这些消息标记为已读
    if (!messages.empty()) {
        dbManager.markMessagesAsRead(0, fromUser, toUser);
    }
    
    return true;
}

// 处理私聊消息获取请求 - 修改为允许不带from和to的情况
void handleGetPrivateMessages(const httplib::Request& req, httplib::Response& res) {
    // 验证用户身份
    std::string cookies = req.get_header_value("Cookie");
    std::string password, uid_str; 
    std::string::size_type pos1 = cookies.find("clientid=");
    if (pos1 != std::string::npos) {
        pos1 += 9; // Skip over "clientid="
        std::string::size_type pos2 = cookies.find(";", pos1);
        if (pos2 == std::string::npos) pos2 = cookies.length();
        password = cookies.substr(pos1, pos2 - pos1);
    }

    std::string::size_type pos3 = cookies.find("uid=");
    if (pos3 != std::string::npos) {
        pos3 += 4; // Skip over "uid="
        std::string::size_type pos4 = cookies.find(";", pos3);
        if (pos4 == std::string::npos) pos4 = cookies.length();
        uid_str = cookies.substr(pos3, pos4 - pos3);
    }

    int userId;
    if (!str::safeatoi(uid_str, userId)) {
        res.status = 400;
        res.set_content("Invalid user ID in cookie", "text/plain");
        return;
    }

    manager::user* currentUser = manager::FindUser(userId);
    if (currentUser == nullptr || currentUser->getpassword() != password) {
        res.status = 401;
        res.set_content("Unauthorized", "text/plain");
        return;
    }

    // 如果请求中有from和to参数，就获取这两个用户之间的消息
    if (req.has_param("from") && req.has_param("to")) {
        std::string fromUserParam = req.get_param_value("from");
        std::string toUserParam = req.get_param_value("to");

        // 验证用户是否有权查看此对话
        if (currentUser->getname() != fromUserParam && currentUser->getname() != toUserParam) {
            res.status = 403;
            res.set_content("Access denied: You can only retrieve messages for chats you are part of.", "text/plain");
            return;
        }

        long long lastTimestamp = 0;
        if (req.has_param("lastTimestamp")) {
            try {
                lastTimestamp = std::stoll(req.get_param_value("lastTimestamp"));
            } catch (...) {
                res.status = 400;
                res.set_content("Invalid lastTimestamp format", "text/plain");
                return;
            }
        }

        // 获取两用户之间的私聊消息
        std::vector<Json::Value> messages;
        if (!PrivateChat::getInstance().getUserMessages(fromUserParam, toUserParam, messages, lastTimestamp)) {
            res.status = 500;
            res.set_content("Failed to retrieve messages", "text/plain");
            return;
        }

        // 构造响应
        Json::Value responseJson(Json::arrayValue);
        for (const auto& msg : messages) {
            responseJson.append(msg);
        }

        // 设置响应
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Content-Type", "application/json");
        res.set_content(responseJson.toStyledString(), "application/json");
        return;
    }
    
    // 如果没有from和to参数，返回当前用户的所有相关消息的简单状态
    Json::Value responseJson;
    responseJson["status"] = "ok";
    responseJson["message"] = "请提供from和to参数以获取具体消息";
    
    res.set_header("Access-Control-Allow-Origin", "*");
    res.set_header("Content-Type", "application/json");
    res.set_content(responseJson.toStyledString(), "application/json");
}

// 新增：检查两个用户之间是否存在未读消息
bool PrivateChat::hasUnreadMessages(const std::string& fromUser, const std::string& toUser, long long lastTimestamp) {
    if (!initialized) {
        return false;
    }
    
    ChatDBManager& dbManager = ChatDBManager::getInstance();
    return dbManager.hasUnreadMessages(0, fromUser, toUser, lastTimestamp);
}

// 新增：检查用户是否有未读消息
bool PrivateChat::userHasUnreadMessages(const std::string& toUser, long long lastTimestamp) {
    if (!initialized) {
        return false;
    }
    
    ChatDBManager& dbManager = ChatDBManager::getInstance();
    return dbManager.userHasUnreadMessages(0, toUser, lastTimestamp);
}

// 新增：将两个用户之间的消息标记为已读
bool PrivateChat::markMessagesAsRead(const std::string& fromUser, const std::string& toUser) {
    if (!initialized) {
        return false;
    }
    
    ChatDBManager& dbManager = ChatDBManager::getInstance();
    return dbManager.markMessagesAsRead(0, fromUser, toUser);
}

// 新增：处理检查是否有未读消息的请求
void handleCheckUnreadMessages(const httplib::Request& req, httplib::Response& res) {
    // 解析请求参数
    if (!req.has_param("from") || !req.has_param("to")) {
        res.status = 400;
        res.set_content("Missing required parameters", "text/plain");
        return;
    }
    
    std::string fromUser = req.get_param_value("from");
    std::string toUser = req.get_param_value("to");
    
    // 解析可选的lastTimestamp参数
    long long lastTimestamp = 0;
    if (req.has_param("lastTimestamp")) {
        try {
            lastTimestamp = std::stoll(req.get_param_value("lastTimestamp"));
        } catch (...) {
            res.status = 400;
            res.set_content("Invalid lastTimestamp format", "text/plain");
            return;
        }
    }
    
    // 验证用户身份
    std::string cookies = req.get_header_value("Cookie");
    std::string password, uid;
    std::string::size_type pos1 = cookies.find("clientid=");
    if (pos1 != std::string::npos) {
        pos1 += 9; // Skip over "clientid="
        std::string::size_type pos2 = cookies.find(";", pos1);
        if (pos2 == std::string::npos) pos2 = cookies.length();
        password = cookies.substr(pos1, pos2 - pos1);
    }

    std::string::size_type pos3 = cookies.find("uid=");
    if (pos3 != std::string::npos) {
        pos3 += 4; // Skip over "uid="
        std::string::size_type pos4 = cookies.find(";", pos3);
        if (pos4 == std::string::npos) pos4 = cookies.length();
        uid = cookies.substr(pos3, pos4 - pos3);
    }
    
    int userId;
    if (!str::safeatoi(uid, userId)) {
        res.status = 400;
        res.set_content("Invalid user ID", "text/plain");
        return;
    }
    
    // 验证用户身份
    manager::user* user = manager::FindUser(userId);
    if (user == nullptr || user->getpassword() != password) {
        res.status = 401;
        res.set_content("Unauthorized", "text/plain");
        return;
    }
    
    // 检查未读消息
    bool hasUnread = PrivateChat::getInstance().hasUnreadMessages(fromUser, toUser, lastTimestamp);
    
    // 构造响应
    Json::Value responseJson;
    responseJson["has_unread"] = hasUnread;
    
    // 设置响应
    res.set_header("Access-Control-Allow-Origin", "*");
    res.set_header("Content-Type", "application/json");
    res.set_content(responseJson.toStyledString(), "application/json");
}

// 新增：处理检查用户是否有未读消息的请求
void handleCheckUserUnreadMessages(const httplib::Request& req, httplib::Response& res) {
    // 解析请求参数
    if (!req.has_param("user")) {
        res.status = 400;
        res.set_content("Missing required parameters", "text/plain");
        return;
    }
    
    std::string toUser = req.get_param_value("user");
    
    // 解析可选的lastTimestamp参数
    long long lastTimestamp = 0;
    if (req.has_param("lastTimestamp")) {
        try {
            lastTimestamp = std::stoll(req.get_param_value("lastTimestamp"));
        } catch (...) {
            res.status = 400;
            res.set_content("Invalid lastTimestamp format", "text/plain");
            return;
        }
    }
    
    // 验证用户身份
    std::string cookies = req.get_header_value("Cookie");
    std::string password, uid;
    std::string::size_type pos1 = cookies.find("clientid=");
    if (pos1 != std::string::npos) {
        pos1 += 9; // Skip over "clientid="
        std::string::size_type pos2 = cookies.find(";", pos1);
        if (pos2 == std::string::npos) pos2 = cookies.length();
        password = cookies.substr(pos1, pos2 - pos1);
    }

    std::string::size_type pos3 = cookies.find("uid=");
    if (pos3 != std::string::npos) {
        pos3 += 4; // Skip over "uid="
        std::string::size_type pos4 = cookies.find(";", pos3);
        if (pos4 == std::string::npos) pos4 = cookies.length();
        uid = cookies.substr(pos3, pos4 - pos3);
    }
    
    int userId;
    if (!str::safeatoi(uid, userId)) {
        res.status = 400;
        res.set_content("Invalid user ID", "text/plain");
        return;
    }
    
    // 验证用户身份
    manager::user* user = manager::FindUser(userId);
    if (user == nullptr || user->getpassword() != password) {
        res.status = 401;
        res.set_content("Unauthorized", "text/plain");
        return;
    }
    
    // 用户只能查询自己的未读消息
    if (user->getname() != toUser) {
        res.status = 403;
        res.set_content("Access denied", "text/plain");
        return;
    }
    
    // 检查用户是否有未读消息
    bool hasUnread = PrivateChat::getInstance().userHasUnreadMessages(toUser, lastTimestamp);
    
    // 构造响应
    Json::Value responseJson;
    responseJson["has_unread"] = hasUnread;
    
    // 设置响应
    res.set_header("Access-Control-Allow-Origin", "*");
    res.set_header("Content-Type", "application/json");
    res.set_content(responseJson.toStyledString(), "application/json");
}

// 新增：处理将消息标记为已读的请求
void handleMarkAsRead(const httplib::Request& req, httplib::Response& res, const Json::Value& root) {
    // 验证请求数据
    if (!root.isMember("from") || !root.isMember("to")) {
        res.status = 400;
        res.set_content("Missing required fields", "text/plain");
        return;
    }
    
    std::string fromUser = root["from"].asString();
    std::string toUser = root["to"].asString();
    
    // 验证用户身份
    std::string cookies = req.get_header_value("Cookie");
    std::string password, uid;
    std::string::size_type pos1 = cookies.find("clientid=");
    if (pos1 != std::string::npos) {
        pos1 += 9; // Skip over "clientid="
        std::string::size_type pos2 = cookies.find(";", pos1);
        if (pos2 == std::string::npos) pos2 = cookies.length();
        password = cookies.substr(pos1, pos2 - pos1);
    }

    std::string::size_type pos3 = cookies.find("uid=");
    if (pos3 != std::string::npos) {
        pos3 += 4; // Skip over "uid="
        std::string::size_type pos4 = cookies.find(";", pos3);
        if (pos4 == std::string::npos) pos4 = cookies.length();
        uid = cookies.substr(pos3, pos4 - pos3);
    }
    
    int userId;
    if (!str::safeatoi(uid, userId)) {
        res.status = 400;
        res.set_content("Invalid user ID", "text/plain");
        return;
    }
    
    // 验证用户身份
    manager::user* user = manager::FindUser(userId);
    if (user == nullptr || user->getpassword() != password) {
        res.status = 401;
        res.set_content("Unauthorized", "text/plain");
        return;
    }
    
    // 用户只能标记自己的消息为已读
    if (user->getname() != fromUser && user->getname() != toUser) {
        res.status = 403;
        res.set_content("Access denied", "text/plain");
        return;
    }
    
    // 标记消息为已读
    if (!PrivateChat::getInstance().markMessagesAsRead(fromUser, toUser)) {
        res.status = 500;
        res.set_content("Failed to mark messages as read", "text/plain");
        return;
    }
    
    // 设置成功响应
    res.status = 200;
    res.set_header("Access-Control-Allow-Origin", "*");
    res.set_content("Messages marked as read", "text/plain");
}

// 添加私聊消息
bool PrivateChat::addPrivateMessage(const std::string& fromUser, const std::string& toUser, 
                                  const std::string& message, const std::string& imageUrl) {
    if (!initialized) {
        return false;
    }
    
    // 创建消息对象
    Json::Value msg;
    msg["user"] = fromUser;
    msg["message"] = message; // 保持Base64编码状态
    msg["timestamp"] = static_cast<Json::Int64>(time(nullptr));
    msg["is_read"] = 0;  // 默认设置为未读
    
    // 设置元数据，包含接收者信息
    Json::Value metadata;
    metadata["to"] = toUser;
    msg["metadata"] = metadata;
    
    // 如果有图片URL，添加到消息中
    if (!imageUrl.empty()) {
        msg["imageUrl"] = imageUrl;
    }
    
    // 添加到私聊室
    {
        std::lock_guard<std::mutex> lock(mtx);
        room[0].chatMessages.push_back(msg);
        if (room[0].chatMessages.size() > room[0].MAXSIZE) {
            room[0].chatMessages.pop_front();
        }
    }
    
    // 保存到数据库
    ChatDBManager& dbManager = ChatDBManager::getInstance();
    bool success = dbManager.addMessage(0, msg);
    
    return success;
}

// 处理发送私聊消息的请求
void handleSendPrivateMessage(const httplib::Request& req, httplib::Response& res, const Json::Value& root) {
    // 验证请求数据
    if (!root.isMember("to") || !root.isMember("message")) {
        res.status = 400;
        res.set_content("Missing required fields", "text/plain");
        return;
    }
    
    std::string toUser = root["to"].asString();
    std::string messageContent = root["message"].asString(); // 已经是Base64编码
    std::string imageUrl = root.isMember("imageUrl") ? root["imageUrl"].asString() : "";
    
    // 验证用户身份
    std::string cookies = req.get_header_value("Cookie");
    std::string password, uid;
    std::string::size_type pos1 = cookies.find("clientid=");
    if (pos1 != std::string::npos) {
        pos1 += 9; // Skip over "clientid="
        std::string::size_type pos2 = cookies.find(";", pos1);
        if (pos2 == std::string::npos) pos2 = cookies.length();
        password = cookies.substr(pos1, pos2 - pos1);
    }

    std::string::size_type pos3 = cookies.find("uid=");
    if (pos3 != std::string::npos) {
        pos3 += 4; // Skip over "uid="
        std::string::size_type pos4 = cookies.find(";", pos3);
        if (pos4 == std::string::npos) pos4 = cookies.length();
        uid = cookies.substr(pos3, pos4 - pos3);
    }
    
    int userId;
    if (!str::safeatoi(uid, userId)) {
        res.status = 400;
        res.set_content("Invalid user ID", "text/plain");
        return;
    }
    
    // 验证用户身份
    manager::user* user = manager::FindUser(userId);
    if (user == nullptr || user->getpassword() != password) {
        res.status = 401;
        res.set_content("Unauthorized", "text/plain");
        return;
    }
    
    // 检查发送者是否被封禁
    if (user->getlabei() == "BAN") {
        res.status = 403;
        res.set_content("User is banned", "text/plain");
        return;
    }
    
    // 检查接收者是否存在
    if (manager::FindUser(toUser) == nullptr) {
        res.status = 404;
        res.set_content("Recipient not found", "text/plain");
        return;
    }
    
    // 添加私聊消息
    if (!PrivateChat::getInstance().addPrivateMessage(user->getname(), toUser, messageContent, imageUrl)) {
        res.status = 500;
        res.set_content("Failed to send message", "text/plain");
        return;
    }
    
    // 设置成功响应
    res.status = 200;
    res.set_header("Access-Control-Allow-Origin", "*");
    res.set_content("Message sent", "text/plain");
}

// 设置私聊API路由
void PrivateChat::setupRoutes() {
    Server& server = Server::getInstance(HOST);
    
    // 获取私聊消息的路由 - 使用恢复的处理函数
    server.getInstance().handleRequest("/private/messages", handleGetPrivateMessages);
    
    // 发送私聊消息的路由
    server.getInstance().handlePostRequest("/private/send", handleSendPrivateMessage);
    
    // 新增：检查是否有未读消息的路由
    server.getInstance().handleRequest("/private/check-unread", handleCheckUnreadMessages);
    
    // 新增：检查用户是否有未读消息的路由
    server.getInstance().handleRequest("/private/user-unread", handleCheckUserUnreadMessages);
    
    // 新增：标记消息为已读的路由
    server.getInstance().handlePostRequest("/private/mark-read", handleMarkAsRead);
    
    // 提供私聊页面
    server.getInstance().serveFile("/private-chat", "html/private-chat.html");
    
    // 提供私聊页面的JS脚本
    server.getInstance().handleRequest("/private-chat/js", [](const httplib::Request& req, httplib::Response& res) {
        std::ifstream jsFile("html/private-chat.js", std::ios::binary);
        if (jsFile) {
            std::stringstream buffer;
            buffer << jsFile.rdbuf();
            res.set_content(buffer.str(), "application/javascript; charset=gbk");
        } else {
            res.status = 404;
            res.set_content("private-chat.js not found", "text/plain");
        }
    });
}