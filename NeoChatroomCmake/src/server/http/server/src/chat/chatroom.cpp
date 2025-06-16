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
#include "chatDBmanager.h"
#include "../tool/log.h"

//---------chatroom
    void chatroom::initializeChatRoom() {
        Logger& logger = Logger::getInstance();
        logger.logInfo("chatroom", "初始化聊天室 ID: " + std::to_string(roomid));

        // 从数据库加载消息
        ChatDBManager& dbManager = ChatDBManager::getInstance();
        bool loadSuccess = dbManager.getMessages(roomid, chatMessages);

        logger.logInfo("chatroom", "从数据库加载了 " + std::to_string(chatMessages.size()) + " 条消息");

        // 如果没有消息，添加初始系统消息
        if (chatMessages.empty()) {
            Json::Value initialMessage;
            initialMessage["user"] = "system";
            initialMessage["labei"] = "GM";
            initialMessage["timestamp"] = static_cast<Json::Int64>(time(0));
            initialMessage["message"] = Base64::base64_encode("欢迎来到聊天室");

            chatMessages.push_back(initialMessage);

            // 保存到数据库
            if (!dbManager.addMessage(roomid, initialMessage)) {
                logger.logError("chatroom", "无法保存初始系统消息到数据库");
            }
        }
    }

    string chatroom::transJsonMessage(Json::Value m) {
        string message = Base64::base64_decode(m["message"].asString());
        // Convert GBK message back to UTF-8 for logging
        string utf8Message = message.c_str()
            ;
        return "[" + m["user"].asString() + "][" + utf8Message + "]";
    }

    void chatroom::systemMessage(string message) {
        Json::Value initialMessage;
        initialMessage["user"] = "system";
        initialMessage["labei"] = "GM";
        initialMessage["timestamp"] = time(0);

        string gbkMessage = (message.c_str());
        initialMessage["message"] = Base64::base64_encode(gbkMessage);

        // 添加到内存中
        chatMessages.push_back(initialMessage);

        // 同时添加到数据库
        ChatDBManager& dbManager = ChatDBManager::getInstance();
        if (!dbManager.addMessage(roomid, initialMessage)) {
            Logger& logger = Logger::getInstance();
            logger.logError("chatroom", "无法保存系统消息到数据库");
        }

        // Log the message
        Logger& logger = Logger::getInstance();
        logger.logInfo("chatroom::message", transJsonMessage(initialMessage));
    }

    bool chatroom::checkAllowId(const int ID) {
        for (auto x : allowID) {
            if (x == ID) return true;
        }
        return false;
    }

    bool chatroom::checkAllowId(const string name) {
        int ID = manager::FindUser(name)->getuid();
        for (auto x : allowID) {
            if (x == ID) return true;
        }
        return false;
    }

    void chatroom::getChatMessages(const httplib::Request& req, httplib::Response& res) {
        std::string requested_path = req.path;
        std::string expected_path = "/chat/" + std::to_string(roomid) + "/messages";
        if (requested_path != expected_path) {
            res.status = 403;
            res.set_content("Invalid room access", "text/plain");
            return;
        }

        long long lastTimestamp = 0;
        if (req.has_param("lastTimestamp")) {
            try {
                lastTimestamp = std::stoll(req.get_param_value("lastTimestamp"));
            } catch (const std::exception&) {
                res.status = 400;
                res.set_content("Invalid lastTimestamp parameter", "text/plain");
                return;
            }
        }

        Json::Value response;
        try {
            for (const auto& msg : chatMessages) {
                if (msg["timestamp"].asInt64() > lastTimestamp) {
                    response.append(msg);
                }
            }
        } catch (const std::exception&) {
            res.status = 500;
            res.set_content("Internal Server Error", "text/plain");
            return;
        }

        if (response.empty()) {
            res.status = 200;
            res.set_header("Access-Control-Allow-Origin", "*");
            res.set_header("Content-Type", "application/json");
            res.set_content(response.toStyledString(), "application/json");
            return;
        }

        if (!checkCookies(req)) {
            res.status = 400;
            res.set_content("Invalid Cookie", "text/plain");
            return;
        }

        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Content-Type", "application/json");
        res.set_content(response.toStyledString(), "application/json");
    }

    void chatroom::getAllChatMessages(const httplib::Request& req, httplib::Response& res) {
        Json::Value response;
        for (const auto& msg : chatMessages) {
            response.append(msg);
        }

        if (response.empty()) {
            res.status = 200;
            res.set_header("Access-Control-Allow-Origin", "*");
            res.set_header("Content-Type", "application/json");
            res.set_content(response.toStyledString(), "application/json");
            return;
        }

        if (!checkCookies(req)) {
            res.status = 400;
            res.set_content("Invalid Cookie", "text/plain");
            return;
        }

        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Content-Type", "application/json");
        res.set_content(response.toStyledString(), "application/json");
    }

    // 解析 Cookie
    void chatroom::transCookie(std::string& cid, std::string& uid, std::string cookie) {
        std::string::size_type pos1 = cookie.find("clientid=");
        if (pos1 != std::string::npos) {
            pos1 += 9; // Skip over "clientid="
            std::string::size_type pos2 = cookie.find(";", pos1);
            if (pos2 == std::string::npos) pos2 = cookie.length();
            cid = cookie.substr(pos1, pos2 - pos1);
        }

        std::string::size_type pos3 = cookie.find("uid=");
        if (pos3 != std::string::npos) {
            pos3 += 4; // Skip over "uid="
            std::string::size_type pos4 = cookie.find(";", pos3);
            if (pos4 == std::string::npos) pos4 = cookie.length();
            uid = cookie.substr(pos3, pos4 - pos3);
        }
    }

    bool chatroom::checkCookies(const httplib::Request& req) {
        std::string cookies = req.get_header_value("Cookie");

        std::string password, uid;
        transCookie(password, uid, cookies);

        int uid_;
        str::safeatoi(uid, uid_);
        if (manager::FindUser(uid_) == nullptr) {
            return false;
        }
        manager::user nowuser = *manager::FindUser(uid_);
        if (nowuser.getpassword() != password) {
            return false;
        }
        if (!manager::checkInRoom(roomid, uid_)) {
            return false;
        }
        return true;
    }


    //路由发送消息请求
    void chatroom::postChatMessage(const httplib::Request& req, httplib::Response& res, const Json::Value& root) {

        res.set_header("Access-Control-Allow-Origin", "*"); // 允许所有来源访问
        res.set_header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE"); // 允许的 HTTP 方法
        res.set_header("Access-Control-Allow-Headers", "Content-Type"); // 允许的头部字段


        Logger& logger = Logger::getInstance();
        std::string cookies = req.get_header_value("Cookie");
        std::string password, uid;
        transCookie(password, uid, cookies);

        if (!root.isMember("message") || !root.isMember("uid")) {
            res.status = 400;
            res.set_content("Invalid data format", "text/plain");
            logger.logInfo("ChatSys", req.remote_addr + "发送了一条不合法请求 ");
            return;
        }

        // Decode base64 message and process it
        string decodedMsg = Base64::base64_decode(root["message"].asString());
        if (decodedMsg.empty()) {
            res.status = 400;
            res.set_content("Message content is empty", "text/plain");
            return;
        }

        if (!checkCookies(req)) {
            res.status = 400;
            res.set_content("Invalid Cookie", "text/plain");
            logger.logInfo("ChatSys", req.remote_addr + "的cookie无效 ");
            return;
        }

        // 验证 uid 和密码
        int uid_;
        str::safeatoi(uid, uid_);
        if (manager::FindUser(uid_) == nullptr) {
            res.status = 400;
            res.set_content("Invalid Cookie", "text/plain");
            logger.logInfo("ChatSys", req.remote_addr + "的cookie无效 ");
            return;
        }
        manager::user nowuser = *manager::FindUser(uid_);
        if (nowuser.getpassword() != password) {
            res.status = 400;
            res.set_content("Invalid Cookie", "text/plain");
            logger.logInfo("ChatSys", req.remote_addr + "的cookie无效 ");
            return;
        }

        // Check if the user is banned
        if (nowuser.getlabei() == "BAN") {
            res.status = 403;
            res.set_content("User is banned", "text/plain");
            logger.logInfo("ChatSys", req.remote_addr + "尝试发送消息，但用户已被封禁 ");
            return;
        }

        if (!manager::checkInRoom(roomid, uid_)) {
            res.status = 403;
            res.set_content("Haven't joined the room yet", "text/plain");
            return;
        }

        // Validate that the requested room matches this chatroom instance
        std::string requested_path = req.path;
        std::string expected_path = "/chat/" + std::to_string(roomid) + "/messages";
        if (requested_path != expected_path) {
            res.status = 403;
            res.set_content("Invalid room access", "text/plain");
            return;
        }

        // 保存聊天消息
        Json::Value newMessage;
        newMessage["user"] = nowuser.getname();
        newMessage["labei"] = nowuser.getlabei();

        string gbkMsg = decodedMsg.c_str();
        string msgSafe = Keyword::process_string(gbkMsg);
        string codedmsg = Base64::base64_encode(msgSafe);

        if (codedmsg.length() > 50000) {
            res.status = 401;
            res.set_content("Message too long", "text/plain");
            return;
        }
        newMessage["message"] = codedmsg;
        if (root.isMember("imageUrl")) {
            newMessage["imageUrl"] = root["imageUrl"];
        }
        newMessage["timestamp"] = static_cast<Json::Int64>(time(0));

        lock_guard<mutex> lock(mtx);

        // 添加到内存中的消息队列
        chatMessages.push_back(newMessage);
        if (chatMessages.size() >= MAXSIZE) {
            chatMessages.pop_front();
        }

        // 添加到数据库
        ChatDBManager& dbManager = ChatDBManager::getInstance();
        bool success = dbManager.addMessage(roomid, newMessage);

        if (!success) {
            logger.logError("chatroom", "无法保存消息到数据库");
        }

        // Log message in UTF-8 for proper display
        logger.logInfo("chatroom::message",
            ("[roomID:" + to_string(roomid) + "][" +
             nowuser.getname() + "][" +
             (msgSafe.c_str()) + "]"));

        res.status = 200;
        res.set_content("Message received", "text/plain");
    }


    void chatroom::setupStaticRoutes() {
        Server& server = server.getInstance(HOST);


    }


    bool chatroom::isValidImage(const std::string& filename) {
        // 获取文件扩展名
        std::string ext = filename.substr(filename.find_last_of("."));
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

        // 检查文件扩展名是否在白名单中
        return std::find(allowedImageTypes.begin(), allowedImageTypes.end(), ext) != allowedImageTypes.end();
    }


    void chatroom::uploadImage(const httplib::Request& req, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE");
        res.set_header("Access-Control-Allow-Headers", "Content-Type");

        // Validate that the requested room matches this chatroom instance
        std::string requested_path = req.path;
        std::string expected_path = "/chat/" + std::to_string(roomid) + "/upload";
        if (requested_path != expected_path) {
            res.status = 403;
            res.set_content("Invalid room access", "text/plain");
            return;
        }

        if (req.has_file("file")) {
            auto file = req.get_file_value("file");

            // 检查文件类型是否合法
            if (!isValidImage(file.filename)) {
                res.status = 400;
                res.set_content("{\"error\": \"Invalid file type\"}", "application/json");
                return;
            }

            std::string uploadDir = "html/images/";
            filesystem::create_directories(uploadDir);

            std::string filePath = uploadDir + file.filename;

            std::ofstream outFile(filePath, std::ios::binary);
            if (outFile) {
                outFile.write(file.content.c_str(), file.content.size());
                outFile.close();

                std::string fileUrl = "/images/" + file.filename;

                Json::Value jsonResponse;
                jsonResponse["imageUrl"] = fileUrl;

                res.set_header("Content-Type", "application/json");
                res.status = 200;
                res.set_content(jsonResponse.toStyledString(), "application/json");
            }
            else {
                res.status = 500;
                res.set_content("{\"error\": \"Failed to save the file\"}", "application/json");
            }
        }
        else {
            res.status = 400;
            res.set_content("{\"error\": \"No file uploaded\"}", "application/json");
        }
    }


    void chatroom::setupChatRoutes() {
        Server& server = server.getInstance(HOST);
        // 提供聊天记录的 GET 请求
        server.getInstance().handleRequest("/chat/" + to_string(roomid) + "/messages", [this](const httplib::Request& req, httplib::Response& res) {
            getChatMessages(req, res);
            });

        server.getInstance().handleRequest("/chat/" + to_string(roomid) + "/all-messages", [this](const httplib::Request& req, httplib::Response& res) {
            getAllChatMessages(req, res);
            });

        // 处理 POST 请求，接收并保存新的聊天消息
        server.getInstance().handlePostRequest("/chat/" + to_string(roomid) + "/messages", [this](const httplib::Request& req, httplib::Response& res , const Json::Value& root) {
                postChatMessage(req, res, root);
        });

        // 获取用户名的 GET 请求已移至全局路由 (在 Server.cpp 中处理)
        // 不再在这里注册

        // 图片上传路由
        server.getInstance().handlePostRequest("/chat/" + to_string(roomid) + "/upload", [this](const httplib::Request& req, httplib::Response& res) {
            uploadImage(req, res);
        });
        //server.getInstance().handlePostRequest("/chat/upload", uploadImage);


        // 设置静态文件路由
        setupStaticRoutes();


    }

    bool chatroom::start() {
        Logger& logger = Logger::getInstance();

        if (roomid == -1) {
            logger.logError("chatroom", "无法启动聊天室：无效的聊天室ID");
            return false;
        }

        // 如果已经激活，仅更新访问时间
        if (isActive) {
            updateAccessTime();
            return true;
        }

        try {
            initializeChatRoom();
            setupChatRoutes();
            Server& server = server.getInstance(HOST);

            server.serveFile("/chat" + to_string(roomid), "html/index.html");
            logger.logInfo("chatroom", "聊天室 ID:" + std::to_string(roomid) + " 启动成功");
            
            isActive = true;
            updateAccessTime();
            return true;
        } catch (const std::exception& e) {
            logger.logError("chatroom", "启动聊天室时发生异常: " + std::string(e.what()));
            return false;
        } catch (...) {
            logger.logError("chatroom", "启动聊天室时发生未知异常");
            return false;
        }
    }

    void chatroom::clearMessage() {
        chatMessages.clear();

        // 清空数据库中的消息
        ChatDBManager& dbManager = ChatDBManager::getInstance();
        dbManager.clearMessages(roomid);

        return;
    }
    
    string chatroom::gettittle() {
        // 即使聊天室未加载也能安全返回标题
        return chatTitle;
    }
    
    void chatroom::setRoomID(int id) {
        roomid = id;
    }
    
    void chatroom::init() {
        // 不再自动清空消息，避免丢失已有消息
        // clearMessage();  // 移除此行
        allowID.clear();
        isActive = false;
        lastAccessTime = 0;
    }
    void chatroom::setflag(int x) {
        flags = x;
        
        // 更新数据库
        ChatDBManager& dbManager = ChatDBManager::getInstance();
        dbManager.updateChatRoom(roomid, chatTitle, passwordHash, password, flags);
    }
    void chatroom::settittle(string Tittle) {
        chatTitle = Tittle;
        
        // 更新数据库
        ChatDBManager& dbManager = ChatDBManager::getInstance();
        dbManager.updateChatRoom(roomid, chatTitle, passwordHash, password, flags);
    }

    void chatroom::setPasswordHash(const std::string& hash) {
        passwordHash = hash;
        
        // 更新数据库
        ChatDBManager& dbManager = ChatDBManager::getInstance();
        dbManager.updateChatRoom(roomid, chatTitle, passwordHash, password, flags);
    }

    void chatroom::setPassword(const std::string& Newpassword) {
        password = Newpassword;
        this->setPasswordHash(SHA256_::sha256(Newpassword));
        
        // 数据库更新已在setPasswordHash中完成
    }

    string chatroom::GetPassword() {
        // 即使聊天室未加载也能安全返回密码
        return password;
    }

    std::string chatroom::getPasswordHash() const {
        // 即使聊天室未加载也能安全返回密码哈希
        return passwordHash;
    }

    // 设置标志
    void chatroom::setFlag(RoomFlags flag) {
        flags |= flag;
    }

    // 清除标志
    void chatroom::clearFlag(RoomFlags flag) {
        flags &= ~flag;
    }

    // 检查标志
    bool chatroom::hasFlag(RoomFlags flag) const {
        // 即使聊天室未加载也能安全检查标志
        return flags & flag;
    }

    // 懒加载相关方法实现
    void chatroom::updateAccessTime() {
        lastAccessTime = time(nullptr);
    }

    time_t chatroom::getLastAccessTime() const {
        return lastAccessTime;
    }

    bool chatroom::isActivated() const {
        return isActive;
    }

    bool chatroom::deactivate() {
        if (!isActive) return true;  // 已经是非活跃状态
        
        // 记录日志
        Logger& logger = Logger::getInstance();
        logger.logInfo("chatroom", "卸载聊天室 ID: " + std::to_string(roomid));
        
        // 这里不清除消息，只是卸载聊天室状态
        isActive = false;
        
        return true;
    }
    
//---------chatroom