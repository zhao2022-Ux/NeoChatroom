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
    
    Logger& logger = Logger::getInstance();
    logger.logInfo("PrivateChat", "初始化私聊系统");
    
    try {
        // 检查room[0]是否已被初始化
        if (used[0]) {
            logger.logInfo("PrivateChat", "私聊室已经存在，检查配置");
        } else {
            logger.logInfo("PrivateChat", "初始化私聊室");
            used[0] = true;
            room[0].init();
            room[0].setRoomID(0);  // 设置特殊ID为0
            room[0].settittle("私聊系统");  // 设置标题
            room[0].setFlag(chatroom::ROOM_HIDDEN);  // 设置为隐藏聊天室
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
            // 修复类型转换问题，使用正确���枚举类型
            if (flags & chatroom::ROOM_HIDDEN) {
                room[0].setFlag(chatroom::ROOM_HIDDEN);
            }
            if (flags & chatroom::ROOM_NO_JOIN) {
                room[0].setFlag(chatroom::ROOM_NO_JOIN);
            }
        } else {
            // 如果数据库中不存在记录，创建新记录
            dbManager.createChatRoom(0, "私聊系统", "", "", chatroom::ROOM_HIDDEN);
        }
        
        initialized = true;
        logger.logInfo("PrivateChat", "私聊系统初始化成功");
        return true;
        
    } catch (const std::exception& e) {
        logger.logError("PrivateChat", "初始化私聊系统时发生异常: " + std::string(e.what()));
        return false;
    } catch (...) {
        logger.logError("PrivateChat", "初始化私聊系统时发生未知异常");
        return false;
    }
}

// 启动私聊系统
bool PrivateChat::start() {
    if (!initialized && !init()) {
        return false;
    }
    
    Logger& logger = Logger::getInstance();
    logger.logInfo("PrivateChat", "启动私聊系统");
    
    try {
        // 启动私聊室
        if (!room[0].start()) {
            logger.logError("PrivateChat", "无法启动私聊室");
            return false;
        }
        
        // 设置私聊API路由
        setupRoutes();
        
        logger.logInfo("PrivateChat", "私聊系统启动成功");
        return true;
        
    } catch (const std::exception& e) {
        logger.logError("PrivateChat", "启动私聊系统时发生异常: " + std::string(e.what()));
        return false;
    } catch (...) {
        logger.logError("PrivateChat", "启动私聊系统时发生未知异常");
        return false;
    }
}

// 发送系统消息到私聊室
void PrivateChat::sendSystemMessage(const std::string& message) {
    if (!initialized) {
        Logger& logger = Logger::getInstance();
        logger.logWarning("PrivateChat", "尝试发送系统消息，但私聊系统尚未初始化");
        return;
    }
    
    room[0].systemMessage(message);
}

// 获取用户间的私聊消息
bool PrivateChat::getUserMessages(const std::string& fromUser, const std::string& toUser, 
                                 std::vector<Json::Value>& messages, long long lastTimestamp) {
    if (!initialized) {
        return false;
    }
    
    // 从私聊室获取所有消息
    for (const auto& msg : room[0].chatMessages) {
        // 检查消息时间戳
        if (lastTimestamp > 0 && msg["timestamp"].asInt64() <= lastTimestamp) {
            continue;
        }
        
        // 检查消息是否是这两个用户之间的
        std::string sender = msg["user"].asString();
        
        // 检查消息中是否有metadata字段，包含接收者信息
        if (msg.isMember("metadata") && msg["metadata"].isMember("to")) {
            std::string receiver = msg["metadata"]["to"].asString();
            
            // 只收集��两个用户之间的消息
            if ((sender == fromUser && receiver == toUser) || 
                (sender == toUser && receiver == fromUser)) {
                messages.push_back(msg);
            }
        }
    }
    
    return true;
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
    msg["message"] = message;
    msg["timestamp"] = static_cast<Json::Int64>(time(nullptr));
    
    // 设置元数据，包含接收者信息
    Json::Value metadata;
    metadata["to"] = toUser;
    msg["metadata"] = metadata;
    
    // 如果有图片URL，添加到消息中
    if (!imageUrl.empty()) {
        msg["imageUrl"] = imageUrl;
    }
    
    // 添加到私聊室
    room[0].chatMessages.push_back(msg);
    if (room[0].chatMessages.size() > room[0].MAXSIZE) {
        room[0].chatMessages.pop_front();
    }
    
    // 保存到数据库
    ChatDBManager& dbManager = ChatDBManager::getInstance();
    return dbManager.addMessage(0, msg);
}

// 处理私聊消息获取请求
void handleGetPrivateMessages(const httplib::Request& req, httplib::Response& res) {
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
    
    // 确保当前用户是通信的一方
    if (user->getname() != fromUser) {
        res.status = 403;
        res.set_content("Access denied", "text/plain");
        return;
    }
    
    // 获取私聊消息
    std::vector<Json::Value> messages;
    if (!PrivateChat::getInstance().getUserMessages(fromUser, toUser, messages, lastTimestamp)) {
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
}

// 处理发送私聊消息请求
void handleSendPrivateMessage(const httplib::Request& req, httplib::Response& res, const Json::Value& root) {
    // 验证请求数据
    if (!root.isMember("to") || !root.isMember("message")) {
        res.status = 400;
        res.set_content("Missing required fields", "text/plain");
        return;
    }
    
    std::string toUser = root["to"].asString();
    std::string message = Base64::base64_decode(root["message"].asString());
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
    
    // 验证接收者是否存在
    if (manager::FindUser(toUser) == nullptr) {
        res.status = 404;
        res.set_content("Recipient not found", "text/plain");
        return;
    }
    
    // 处理消息内容（例如过滤关键词）
    std::string processedMessage = Keyword::process_string(message);
    
    // 消息太长则拒绝
    if (processedMessage.length() > 5000) {
        res.status = 413;
        res.set_content("Message too long", "text/plain");
        return;
    }
    
    // 添加私聊消息
    if (!PrivateChat::getInstance().addPrivateMessage(user->getname(), toUser, 
                                                  Base64::base64_encode(processedMessage), imageUrl)) {
        res.status = 500;
        res.set_content("Failed to send message", "text/plain");
        return;
    }
    
    // 记录日志
    Logger& logger = Logger::getInstance();
    logger.logInfo("PrivateChat", "[从: " + user->getname() + "][至: " + toUser + "][" + processedMessage + "]");
    
    // 设置成功响应
    res.status = 200;
    res.set_header("Access-Control-Allow-Origin", "*");
    res.set_content("Message sent", "text/plain");
}

// 设置私聊API路由
void PrivateChat::setupRoutes() {
    Server& server = Server::getInstance(HOST);
    
    // 获取私聊消息的路由
    server.getInstance().handleRequest("/private/messages", handleGetPrivateMessages);
    
    // 发送私聊消息的路由
    server.getInstance().handlePostRequest("/private/send", handleSendPrivateMessage);
    
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