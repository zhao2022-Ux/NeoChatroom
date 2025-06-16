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

// 获取用户间的私聊消息
bool PrivateChat::getUserMessages(const std::string& fromUser, const std::string& toUser, 
                                 std::vector<Json::Value>& messages, long long lastTimestamp) {
    if (!initialized) {
        return false;
    }
    
    // 从数据库获取私聊消息，确保我们能获取所有消息
    ChatDBManager& dbManager = ChatDBManager::getInstance();
    std::deque<Json::Value> allMessages;
    if (!dbManager.getMessages(0, allMessages, 0)) {
        return false;
    }
    
    // 从获取的消息中筛选两个用户之间的消息
    for (const auto& msg : allMessages) {
        // 跳过系统消息
        if (msg["user"].asString() == "system") {
            continue;
        }
        
        // 检查消息时间戳
        if (lastTimestamp > 0 && msg["timestamp"].asInt64() <= lastTimestamp) {
            continue;
        }
        
        // 检查消息是否是这两个用户之间的
        std::string sender = msg["user"].asString();
        
        // 检查消息中是否有metadata字段，包含接收者信息
        bool isMatch = false;
        std::string receiver = "";
        
        // 检查metadata字段
        if (msg.isMember("metadata")) {
            // 对于字符串形式的metadata，尝试解析
            if (msg["metadata"].isString()) {
                std::string metadataStr = msg["metadata"].asString();
                
                Json::Value parsedMetadata;
                Json::Reader reader;
                if (reader.parse(metadataStr, parsedMetadata) && 
                    parsedMetadata.isMember("to")) {
                    receiver = parsedMetadata["to"].asString();
                    
                    if ((sender == fromUser && receiver == toUser) || 
                        (sender == toUser && receiver == fromUser)) {
                        isMatch = true;
                    }
                }
            }
            // 对于对象形式的metadata
            else if (msg["metadata"].isObject()) {
                if (msg["metadata"].isMember("to")) {
                    receiver = msg["metadata"]["to"].asString();
                    
                    // 双向匹配，A->B 或 B->A 的消息都应该显示
                    if ((sender == fromUser && receiver == toUser) || 
                        (sender == toUser && receiver == fromUser)) {
                        isMatch = true;
                    }
                }
            }
        } else {
            // 即使没有metadata，也尝试通过其他方式匹配
            if (sender == fromUser || sender == toUser) {
                // 提高匹配率：将所有在私聊室中的，由fromUser或toUser发送的消息都视为他们之间的私聊
                isMatch = true;
            }
        }
        
        if (isMatch) {
            messages.push_back(msg);
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
    
    // 修改：允许用户获取自己作为发送者或接收者的消息
    if (user->getname() != fromUser && user->getname() != toUser) {
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
    std::string message = root["message"].asString();
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
    std::string processedMessage;
    try {
        std::string decodedMsg = Base64::base64_decode(message);
        processedMessage = Keyword::process_string(decodedMsg);
    } catch (const std::exception& e) {
        // 如果解码失败，直接使用原始消息
        processedMessage = Keyword::process_string(message);
    }
    
    // 消息太长则拒绝
    if (processedMessage.length() > 5000) {
        res.status = 413;
        res.set_content("Message too long", "text/plain");
        return;
    }
    
    // 重新编码处理后的消息
    std::string encodedMessage = Base64::base64_encode(processedMessage);
    
    // 添加私聊消息
    if (!PrivateChat::getInstance().addPrivateMessage(user->getname(), toUser, encodedMessage, imageUrl)) {
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
