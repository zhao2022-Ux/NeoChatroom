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
    userChatPartnersCache.clear();
}

// 析构函数
PrivateChat::~PrivateChat() {
    // 清理工作
    std::lock_guard<std::mutex> lock(cacheMutex);
    messageCache.clear();

    std::lock_guard<std::mutex> lock2(partnersCacheMutex);
    userChatPartnersCache.clear();
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

    // 清理用户聊天伙伴缓存
    {
        std::lock_guard<std::mutex> lock(partnersCacheMutex);
        if (userChatPartnersCache.size() > MAX_CACHE_ENTRIES) {
            userChatPartnersCache.clear();
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

// 优化：获取用户间的私聊消息
bool PrivateChat::getUserMessages(const std::string& fromUser, const std::string& toUser,
                                 std::vector<Json::Value>& messages, long long lastTimestamp,
                                 int page, int pageSize) {
    if (!initialized) {
        return false;
    }

    // 生成缓存键
    std::string cacheKey = fromUser + "_" + toUser + "_" + std::to_string(lastTimestamp) +
                          "_" + std::to_string(page) + "_" + std::to_string(pageSize);

    // 先检查缓存
    {
        std::lock_guard<std::mutex> lock(cacheMutex);
        auto it = messageCache.find(cacheKey);
        if (it != messageCache.end()) {
            // 命中缓存
            for (const auto& msg : it->second) {
                messages.push_back(msg);
            }
            return true;
        }
    }

    // 缓存未命中，从数据库获取
    ChatDBManager& dbManager = ChatDBManager::getInstance();
    std::deque<Json::Value> tempMessages;

    // 使用分页获取消息
    if (!dbManager.getPagedMessages(0, fromUser, toUser, tempMessages, lastTimestamp, page, pageSize)) {
        Logger::getInstance().logError("PrivateChat", "获取私聊消息失败");
        return false;
    }

    // 将消息复制到输出向量
    messages.clear();
    for (const auto& msg : tempMessages) {
        messages.push_back(msg);
    }

    // 更新缓存
    {
        std::lock_guard<std::mutex> lock(cacheMutex);
        // 只缓存非空结果
        if (!tempMessages.empty()) {
            messageCache[cacheKey] = tempMessages;
        }
    }

    // 定期清理缓存
    cleanupCache();

    return true;
}

// 新增：获取用户的最近聊天列表
bool PrivateChat::getUserRecentChats(const std::string& username, std::vector<std::string>& chatUsers, int limit) {
    if (!initialized) {
        return false;
    }

    // 生成缓存键
    std::string cacheKey = username + "_recent_" + std::to_string(limit);

    // 先检查缓存
    {
        std::lock_guard<std::mutex> lock(partnersCacheMutex);
        auto it = userChatPartnersCache.find(cacheKey);
        if (it != userChatPartnersCache.end()) {
            // 命中缓存
            chatUsers = it->second;
            return true;
        }
    }

    // 缓存未命中，从数据库获取
    ChatDBManager& dbManager = ChatDBManager::getInstance();
    if (!dbManager.getUserRecentChats(0, username, chatUsers, limit)) {
        return false;
    }

    // 更新缓存
    {
        std::lock_guard<std::mutex> lock(partnersCacheMutex);
        userChatPartnersCache[cacheKey] = chatUsers;
    }

    return true;
}

// 新增：获取用户未读消息计数
int PrivateChat::getUserUnreadCount(const std::string& username) {
    if (!initialized) {
        return 0;
    }

    ChatDBManager& dbManager = ChatDBManager::getInstance();
    return dbManager.getUserUnreadCount(0, username);
}

// 优化：批量标记消息为已读
bool PrivateChat::batchMarkMessagesAsRead(const std::string& fromUser, const std::string& toUser) {
    if (!initialized) {
        return false;
    }

    ChatDBManager& dbManager = ChatDBManager::getInstance();
    bool success = dbManager.batchMarkMessagesAsRead(0, fromUser, toUser);

    // 如果成功标记，清除相关缓存
    if (success) {
        std::lock_guard<std::mutex> lock(cacheMutex);
        // 查找和清除包��这两个用户的所有缓存条目
        for (auto it = messageCache.begin(); it != messageCache.end();) {
            if (it->first.find(fromUser) != std::string::npos &&
                it->first.find(toUser) != std::string::npos) {
                it = messageCache.erase(it);
            } else {
                ++it;
            }
        }
    }

    return success;
}

// 新增：处理获取用户最近聊天列表的请求
void handleGetRecentChats(const httplib::Request& req, httplib::Response& res) {
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

    // 获取参数
    int limit = 20; // 默认值
    if (req.has_param("limit")) {
        try {
            limit = std::stoi(req.get_param_value("limit"));
            // 限制上限，防止滥用
            if (limit > 100) limit = 100;
        } catch (...) {
            // 出错时使用默认值
        }
    }

    // 获取用户的最近聊天列表
    std::vector<std::string> chatUsers;
    if (!PrivateChat::getInstance().getUserRecentChats(currentUser->getname(), chatUsers, limit)) {
        res.status = 500;
        res.set_content("Failed to retrieve recent chats", "text/plain");
        return;
    }

    // 构造响应
    Json::Value responseJson(Json::arrayValue);
    for (const auto& user : chatUsers) {
        responseJson.append(user);
    }

    // 设置响应
    res.set_header("Access-Control-Allow-Origin", "*");
    res.set_header("Content-Type", "application/json");
    res.set_content(responseJson.toStyledString(), "application/json");
}

// 新增：处理获取用户未读消息计数的请求
void handleGetUnreadCount(const httplib::Request& req, httplib::Response& res) {
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

    // 获取用户未读消息计数
    int unreadCount = PrivateChat::getInstance().getUserUnreadCount(currentUser->getname());

    // 构造响应
    Json::Value responseJson;
    responseJson["unread_count"] = unreadCount;

    // 设置响应
    res.set_header("Access-Control-Allow-Origin", "*");
    res.set_header("Content-Type", "application/json");
    res.set_content(responseJson.toStyledString(), "application/json");
}

// 优化：处理获取私聊消息的请求以支持分页
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

        // 获取分页参数
        int page = 0;
        int pageSize = 50; // 默认值

        if (req.has_param("page")) {
            try {
                page = std::stoi(req.get_param_value("page"));
                if (page < 0) page = 0;
            } catch (...) {
                // 使用默认值
            }
        }

        if (req.has_param("pageSize")) {
            try {
                pageSize = std::stoi(req.get_param_value("pageSize"));
                // 限制页面大小以防止过大的请求
                if (pageSize > 100) pageSize = 100;
                if (pageSize < 1) pageSize = 1;
            } catch (...) {
                // 使用默认值
            }
        }

        // 获取两用户之间的私聊消息
        std::vector<Json::Value> messages;
        if (!PrivateChat::getInstance().getUserMessages(fromUserParam, toUserParam, messages, lastTimestamp, page, pageSize)) {
            res.status = 500;
            res.set_content("Failed to retrieve messages", "text/plain");
            return;
        }

        // 构造响应
        Json::Value responseJson(Json::objectValue);
        Json::Value messageArray(Json::arrayValue);

        for (const auto& msg : messages) {
            messageArray.append(msg);
        }

        // 注意：这里前端代码期望直接返回消息数组，而不是包含messages字段的对象
        // 修改为直接返回消息数组，而不是包装在messages字段中
        
        // 设置响应
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Content-Type", "application/json");
        res.set_content(messageArray.toStyledString(), "application/json");
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

// 修复添加私聊消息函数，确保消息在数据库和缓存中都正确保存
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

    // 清除相关缓存，确保下次获取消息时能读取到新消息
    if (success) {
        std::lock_guard<std::mutex> lock(cacheMutex);
        // 清除与这两个用户相关的缓存
        for (auto it = messageCache.begin(); it != messageCache.end();) {
            if (it->first.find(fromUser) != std::string::npos || 
                it->first.find(toUser) != std::string::npos) {
                it = messageCache.erase(it);
            } else {
                ++it;
            }
        }
    }

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

// 获取单��实例
PrivateChat& PrivateChat::getInstance() {
    if (instance == nullptr) {
        instance = new PrivateChat();
    }
    return *instance;
}

// 设置私聊API路由
void PrivateChat::setupRoutes() {
    Server& server = Server::getInstance(HOST);

    // 获取私聊消息的路由 - 使用恢复的处理函数
    server.getInstance().handleRequest("/private/messages", handleGetPrivateMessages);

    // 发送私聊消息的路由
    server.getInstance().handlePostRequest("/private/send", handleSendPrivateMessage);

    // 新增���检查是否有未读消息的路由
    server.getInstance().handleRequest("/private/check-unread", handleCheckUnreadMessages);

    // 新增：检查用户是否有未读消息的路由
    server.getInstance().handleRequest("/private/user-unread", handleCheckUserUnreadMessages);

    // 新增：标记消息为已读的路由
    server.getInstance().handlePostRequest("/private/mark-read", handleMarkAsRead);

    // 新增：获取用户最近聊天列表的路由
    server.getInstance().handleRequest("/private/recent-chats", handleGetRecentChats);

    // 新增：获取用户未读消息计数的路由
    server.getInstance().handleRequest("/private/unread-count", handleGetUnreadCount);

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