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
    // 初始化缓存
    messageCache.clear();
}

// 析构函数
PrivateChat::~PrivateChat() {
    // 清理工作
    std::lock_guard<std::mutex> lock(cacheMutex);
    messageCache.clear();
}

// 清理过期缓存
void PrivateChat::cleanupCache() {
    const size_t MAX_CACHE_ENTRIES = 1000; // 最大缓存条目数

    // 清理消息缓存
    {
        std::lock_guard<std::mutex> lock(cacheMutex);
        if (messageCache.size() > MAX_CACHE_ENTRIES) {
            // 简单策略：当缓存过大时，直接清空
            messageCache.clear();
        }
    }
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

// 安全转义SQL字符串的辅助函数
std::string escapeSqlString(const std::string& s) {
    std::string escaped_s = "";
    for (char c : s) {
        if (c == '\'') {
            escaped_s += "''";
        } else {
            escaped_s += c;
        }
    }
    return escaped_s;
}

// 重构: 获取用户的私聊消息，并自动标记为已读
bool PrivateChat::getUserMessages(const std::string& currentUserName, const std::string& partner,
    std::vector<Json::Value>& messages, long long lastTimestamp, int page, int pageSize) {
    

    
    if (!initialized) {
        Logger::getInstance().logError("PrivateChat", "系统未初始化");
        return false;
    }

    if (!room[0].isActivated() && !room[0].start()) {
        Logger::getInstance().logError("PrivateChat", "无法启动私聊室");
        return false;
    }

    // 生成缓存键时结合发送者和对话对象
    std::string cacheKey = "messages_" + currentUserName + "_" + partner + "_" + std::to_string(lastTimestamp) +
                          "_" + std::to_string(page) + "_" + std::to_string(pageSize);
    

    {
        std::lock_guard<std::mutex> lock(cacheMutex);
        auto it = messageCache.find(cacheKey);
        if (it != messageCache.end()) {
            for (const auto& msg : it->second) {
                messages.push_back(msg);
            }
            return true;
        }
    }

    ChatDBManager& dbManager = ChatDBManager::getInstance();
    std::deque<Json::Value> tempMessages;

    // 调用已经实现的获取两个用户之间消息的函数
    bool querySuccess = dbManager.getPrivateMessagesBetweenUsers(0, currentUserName, partner, tempMessages, lastTimestamp);
    if (!querySuccess) {
        Logger::getInstance().logError("PrivateChat", "从数据库加载消息失败");
        return false;
    }


    // 应用分页：先将结果转换为升序，再取指定页
    std::vector<Json::Value> tempVector(tempMessages.begin(), tempMessages.end());
    std::sort(tempVector.begin(), tempVector.end(),
        [](const Json::Value& a, const Json::Value& b) {
            return a["timestamp"].asInt64() < b["timestamp"].asInt64();
    });
    
    int startIndex = page * pageSize;
    int endIndex = std::min(startIndex + pageSize, static_cast<int>(tempVector.size()));
    messages.clear();
    if (startIndex < static_cast<int>(tempVector.size())) {
        for (int i = startIndex; i < endIndex; i++) {
            messages.push_back(tempVector[i]);
        }
    }


    {
        std::lock_guard<std::mutex> lock(cacheMutex);
        // 缓存整个查询结果（未分页的tempVector），前端可取指定分页数据
        std::deque<Json::Value> cacheData(tempVector.begin(), tempVector.end());
        messageCache[cacheKey] = cacheData;
    }

    // 标记未读：标记除当前发送者外的消息为已读
    if (!tempMessages.empty()) {
        std::set<std::string> senders;
        for (const auto& msg : tempMessages) {
            if (msg.isMember("user") && msg["user"].asString() != currentUserName) {
                senders.insert(msg["user"].asString());
            }
        }
        for (const auto& sender : senders) {
            if (!dbManager.batchMarkMessagesAsRead(0, sender, partner)) {
                Logger::getInstance().logWarning("PrivateChat", "标记消息为已读失败，但继续处理");
            }
        }
    }

    cleanupCache();

    return true;
}

// 优化：处理获取私聊消息的请求
void handleGetPrivateMessages(const httplib::Request& req, httplib::Response& res) {

    std::string debugInfo = "请求参���: ";
    for (const auto& param : req.params) {
        debugInfo += param.first + "=" + param.second + ", ";
    }

    // 用户验证
    std::string cookies = req.get_header_value("Cookie");
    std::string password, uid_str;
    std::string::size_type pos1 = cookies.find("clientid=");
    if (pos1 != std::string::npos) {
        pos1 += 9;
        std::string::size_type pos2 = cookies.find(";", pos1);
        if (pos2 == std::string::npos) pos2 = cookies.length();
        password = cookies.substr(pos1, pos2 - pos1);
    }
    std::string::size_type pos3 = cookies.find("uid=");
    if (pos3 != std::string::npos) {
        pos3 += 4;
        std::string::size_type pos4 = cookies.find(";", pos3);
        if (pos4 == std::string::npos) pos4 = cookies.length();
        uid_str = cookies.substr(pos3, pos4 - pos3);
    }
    int userId;
    if (!str::safeatoi(uid_str, userId)) {
        Logger::getInstance().logError("PrivateChat:API", "无效的用户ID: " + uid_str);
        res.status = 400;
        res.set_content("Invalid user ID in cookie", "text/plain");
        return;
    }
    manager::user* currentUser = manager::FindUser(userId);
    if (currentUser == nullptr || currentUser->getpassword() != password) {
        Logger::getInstance().logError("PrivateChat:API", "用户验证失败: userId=" + uid_str);
        res.status = 401;
        res.set_content("Unauthorized", "text/plain");
        return;
    }

    if (!req.has_param("to")) {
        Logger::getInstance().logError("PrivateChat:API", "缺少to参数");
        res.status = 400;
        res.set_content("Missing 'to' parameter", "text/plain");
        return;
    }
    std::string partner = req.get_param_value("to");
    // 如果请求中包含from参数则使用，否则以当前用户为发送者
    std::string fromParam = currentUser->getname();
    if (req.has_param("from")) {
        fromParam = req.get_param_value("from");
    }

    long long lastTimestamp = 0;
    if (req.has_param("lastTimestamp")) {
        try {
            lastTimestamp = std::stoll(req.get_param_value("lastTimestamp"));
        } catch (...) {
            Logger::getInstance().logError("PrivateChat:API", "无效的时间戳格式: " + req.get_param_value("lastTimestamp"));
            res.status = 400;
            res.set_content("Invalid lastTimestamp format", "text/plain");
            return;
        }
    }

    int page = 0;
    int pageSize = 50;
    if (req.has_param("page")) {
        try {
            page = std::stoi(req.get_param_value("page"));
            if (page < 0) page = 0;
        } catch (...) {
            Logger::getInstance().logWarning("PrivateChat:API", "无效的页码参数，使用默认值 0");
        }
    }
    if (req.has_param("pageSize")) {
        try {
            pageSize = std::stoi(req.get_param_value("pageSize"));
            if (pageSize > 100) pageSize = 100;
            if (pageSize < 1) pageSize = 1;
        } catch (...) {
            Logger::getInstance().logWarning("PrivateChat:API", "无效的页面大小参数，使用默认值 50");
        }
    }

    std::vector<Json::Value> messages;
    if (!PrivateChat::getInstance().getUserMessages(fromParam, partner, messages, lastTimestamp, page, pageSize)) {
        Logger::getInstance().logError("PrivateChat:API", "获取消息失败");
        Json::Value emptyArray(Json::arrayValue);
        res.status = 200;
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Content-Type", "application/json");
        res.set_content(emptyArray.toStyledString(), "application/json");
        Logger::getInstance().logInfo("PrivateChat:API", "返回空消息数组");
        return;
    }

    Json::Value messageArray(Json::arrayValue);
    for (const auto& msg : messages) {
        messageArray.append(msg);
    }

    res.set_header("Access-Control-Allow-Origin", "*");
    res.set_header("Content-Type", "application/json");
    res.set_content(messageArray.toStyledString(), "application/json");
}

// 修复：添加私聊消息函数，确保消息在数据库和缓存中都正确保存
bool PrivateChat::addPrivateMessage(const std::string& fromUser, const std::string& toUser,
                                  const std::string& message, const std::string& imageUrl) {

    if (!initialized) {
        Logger::getInstance().logError("PrivateChat", "系统未初始化");
        return false;
    }

    // 创建消息对象
    Json::Value msg;
    msg["user"] = fromUser;
    msg["message"] = message; // 保持Base64编码状态
    msg["timestamp"] = static_cast<Json::Int64>(time(nullptr));
    msg["is_read"] = 0;  // ��认设置为未读

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

    if (!success) {
        Logger::getInstance().logError("PrivateChat", "无法保存消息到数据库");
        return false;
    }
    

    // 清除相关缓存，确保下次获取消息时能读取到新消息
    {
        std::lock_guard<std::mutex> lock(cacheMutex);
        // 清除与这两个���户相关的缓存
        for (auto it = messageCache.begin(); it != messageCache.end();) {
            if (it->first.find(toUser) != std::string::npos || it->first.find(fromUser) != std::string::npos) {
                it = messageCache.erase(it);
            } else {
                ++it;
            }
        }
    }

    return true;
}

// 处理发送私聊消息的请求
void handleSendPrivateMessage(const httplib::Request& req, httplib::Response& res, const Json::Value& root) {

    // 验证请求数据
    if (!root.isMember("to") || !root.isMember("message")) {
        Logger::getInstance().logError("PrivateChat:API", "缺少必要字段");
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
        Logger::getInstance().logError("PrivateChat:API", "无效的用户ID: " + uid);
        res.status = 400;
        res.set_content("Invalid user ID", "text/plain");
        return;
    }

    // 验证用户身份
    manager::user* user = manager::FindUser(userId);
    if (user == nullptr || user->getpassword() != password) {
        Logger::getInstance().logError("PrivateChat:API", "用户验证失败: userId=" + uid);
        res.status = 401;
        res.set_content("Unauthorized", "text/plain");
        return;
    }


    // 检查发送者是否被禁
    if (user->getlabei() == "BAN") {
        Logger::getInstance().logError("PrivateChat:API", "发送者已被禁止: " + user->getname());
        res.status = 403;
        res.set_content("User is banned", "text/plain");
        return;
    }

    // 检查接收者是否存在
    if (manager::FindUser(toUser) == nullptr) {
        Logger::getInstance().logError("PrivateChat:API", "接收者不存在: " + toUser);
        res.status = 404;
        res.set_content("Recipient not found", "text/plain");
        return;
    }

    // 添加私聊消息
    if (!PrivateChat::getInstance().addPrivateMessage(user->getname(), toUser, messageContent, imageUrl)) {
        Logger::getInstance().logError("PrivateChat:API", "发送消息失败");
        res.status = 500;
        res.set_content("Failed to send message", "text/plain");
        return;
    }

    // 设置成功响应
    res.status = 200;
    res.set_header("Access-Control-Allow-Origin", "*");
    res.set_content("Message sent", "text/plain");
}

// 新增：处理标记已读请求
void handleMarkMessagesAsRead(const httplib::Request& req, httplib::Response& res, const Json::Value& root) {
    if (!root.isMember("from") || !root.isMember("to")) {
        res.status = 400;
        Json::Value jsonRes;
        jsonRes["status"] = "error";
        jsonRes["message"] = "Missing required fields";
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_content(jsonRes.toStyledString(), "application/json");
        return;
    }
    std::string fromUser = root["from"].asString();
    std::string toUser = root["to"].asString();

    // 调用数据库标记已读函数（room_id为0）
    ChatDBManager& dbManager = ChatDBManager::getInstance();
    if (dbManager.markMessagesAsRead(0, fromUser, toUser)) {
        res.status = 200;
        Json::Value jsonRes;
        jsonRes["status"] = "ok";
        jsonRes["message"] = "Marked as read";
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_content(jsonRes.toStyledString(), "application/json");
    } else {
        res.status = 500;
        Json::Value jsonRes;
        jsonRes["status"] = "error";
        jsonRes["message"] = "Failed to mark messages as read";
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_content(jsonRes.toStyledString(), "application/json");
    }
}

// 添加：检查用户是否有未读消息的函数实现
bool PrivateChat::checkUnreadMessages(const std::string& username, Json::Value& response) {
    if (!initialized) {
        Logger::getInstance().logError("PrivateChat", "系统未初始化");
        return false;
    }

    // 使用数据库直接查询，不加载消息内容
    ChatDBManager& dbManager = ChatDBManager::getInstance();
    bool hasUnreadMessages = dbManager.userHasUnreadMessages(0, username);
    
    // 如果需要更详细的未读消息信息，可以获取未读消息计数
    int unreadCount = dbManager.getUserUnreadCount(0, username);
    
    // 准备响应数据
    response["hasUnread"] = hasUnreadMessages;
    response["unreadCount"] = unreadCount;
    
    return true;
}

// 处理检查未读消息的请求
void handleCheckUnreadMessages(const httplib::Request& req, httplib::Response& res) {
    // 获取并验证用户凭证
    std::string cookies = req.get_header_value("Cookie");
    std::string password, uid_str;
    std::string::size_type pos1 = cookies.find("clientid=");
    if (pos1 != std::string::npos) {
        pos1 += 9;
        std::string::size_type pos2 = cookies.find(";", pos1);
        if (pos2 == std::string::npos) pos2 = cookies.length();
        password = cookies.substr(pos1, pos2 - pos1);
    }
    std::string::size_type pos3 = cookies.find("uid=");
    if (pos3 != std::string::npos) {
        pos3 += 4;
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
    
    // 获取当前用户名
    std::string username = currentUser->getname();
    
    // 检查未读消息
    Json::Value response;
    if (!PrivateChat::getInstance().checkUnreadMessages(username, response)) {
        res.status = 500;
        res.set_content("Failed to check unread messages", "text/plain");
        return;
    }
    
    // 设置响应
    res.set_header("Access-Control-Allow-Origin", "*");
    res.set_header("Content-Type", "application/json");
    res.set_content(response.toStyledString(), "application/json");
}

// 获取单例实例
PrivateChat& PrivateChat::getInstance() {
    if (instance == nullptr) {
        instance = new PrivateChat();
    }
    return *instance;
}

// 设置私聊API路由
void PrivateChat::setupRoutes() {
    Server& server = Server::getInstance(HOST);

    // 获取私聊消息的路由
    server.getInstance().handleRequest("/private/messages", handleGetPrivateMessages);

    // 发送私聊消息的路由
    server.getInstance().handlePostRequest("/private/send", handleSendPrivateMessage);
    
    // 修改：使用 JSON 处理版本注册标记已读接口
    server.getInstance().handlePostRequest("/private/mark-read", handleMarkMessagesAsRead);
    
    // 添加：检查未读消息的路由
    server.getInstance().handleRequest("/private/check-unread", handleCheckUnreadMessages);

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