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


//---------chatroom
    void chatroom::initializeChatRoom() {
        Json::Value initialMessage;
        initialMessage["user"] = "system";
        initialMessage["labei"] = "GM";
        initialMessage["timestamp"] = static_cast<Json::Int64>(time(0)); // Ensure timestamp is stored as Int64
        initialMessage["message"] = Base64::base64_encode("wellcome!");
        chatMessages.push_back(initialMessage);
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
        
        // Convert UTF-8 message to GBK for frontend display
        string gbkMessage = (message.c_str());
        initialMessage["message"] = Base64::base64_encode(gbkMessage);

        chatMessages.push_back(initialMessage);

        // Log the message in UTF-8 for proper log display
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
        if (!checkCookies(req)) {
            res.status = 400;
            res.set_content("Invalid Cookie", "text/plain");
            return;
        }

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

        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Content-Type", "application/json");
        res.set_content(response.toStyledString(), "application/json");
    }

    void chatroom::getAllChatMessages(const httplib::Request& req, httplib::Response& res) {
        if (!checkCookies(req)) {
            res.status = 400;
            res.set_content("Invalid Cookie", "text/plain");
            return;
        }

        Json::Value response;
        for (const auto& msg : chatMessages) {
            response.append(msg);
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
        
        // Decode base64 message and process it
        string decodedMsg = Base64::base64_decode(root["message"].asString());
        // Convert UTF-8 input to GBK for storage and frontend display
        string gbkMsg = decodedMsg.c_str();
        string msgSafe = Keyword::process_string(gbkMsg);
        string codedmsg = Base64::base64_encode(msgSafe);
        
        if (codedmsg.length() > 50000) {
            res.status = 401;
            res.set_content("Message too long", "text/plain");
            return;
        }
        newMessage["message"] = codedmsg;
        newMessage["imageUrl"] = root["imageUrl"];
        newMessage["timestamp"] = static_cast<Json::Int64>(time(0)); // Ensure timestamp is stored as Int64

        lock_guard<mutex> lock(mtx);
        chatMessages.push_back(newMessage);
        if (chatMessages.size() >= MAXSIZE) chatMessages.pop_front();

        // Log message in UTF-8 for proper display
        logger.logInfo("chatroom::message", 
            ("[roomID:" + to_string(roomid) + "][" + 
             nowuser.getname() + "][" + 
             (msgSafe.c_str()) + "]"));

        res.status = 200;
        res.set_content("Message received", "text/plain");
    }

    // 获取用户名接口
    void chatroom::getUsername(const httplib::Request& req, httplib::Response& res) {

        res.set_header("Access-Control-Allow-Origin", "*"); // 允许所有来源访问
        res.set_header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE"); // 允许的 HTTP 方法
        res.set_header("Access-Control-Allow-Headers", "Content-Type"); // 允许的头部字段


        std::string cookies = req.get_header_value("Cookie");
        std::string uid;
        transCookie(uid, uid, cookies);

        // 获取用户名
        int uid_;
        str::safeatoi(uid, uid_);
        if (manager::FindUser(uid_) == nullptr) {
            res.status = 400;
            res.set_content("Invalid uid or user not found", "text/plain");
            return;
        }
        manager::user goaluser = *manager::FindUser(uid_);
        
        std::string username = goaluser.getname();

        if (username.empty()) {
            res.status = 400;
            res.set_content("Invalid uid or user not found", "text/plain");
        }
        else {
            Json::Value response;
            response["username"] = username;
            res.set_header("Access-Control-Allow-Origin", "*");
            res.set_header("Content-Type", "application/json");
            res.set_content(response.toStyledString(), "application/json");
        }
    }

    void chatroom::setupStaticRoutes() {
        Server& server = server.getInstance(HOST);

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
        server.getInstance().handleRequest(R"(/files/([^/]+))", [](const httplib::Request& req, httplib::Response& res) {
            std::string filePath = "html/files/" + req.matches[1].str();  // 获取文件名
            std::ifstream file(filePath, std::ios::binary);

            if (file) {
                std::stringstream buffer;
                buffer << file.rdbuf();

                // 自动推测文件的 MIME 类型
                std::string extension = filePath.substr(filePath.find_last_of('.') + 1);
                std::string mimeType = "text/html";

                res.set_content(buffer.str(), mimeType);
            }
            else {
                res.status = 404;
                res.set_content("File not found", "text/plain");
            }
            });
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
        //server.getInstance().handlePostRequest("/chat/messages", postChatMessage);


        // 获取用户名的 GET 请求
        server.getInstance().handleRequest("/user/username", [this](const httplib::Request& req, httplib::Response& res) {
            getUsername(req, res);
            });
        //server.getInstance().handleRequest("/user/username", getUsername);

        // 图片上传路由
        server.getInstance().handlePostRequest("/chat/" + to_string(roomid) + "/upload", [this](const httplib::Request& req, httplib::Response& res) {
            uploadImage(req, res);
        });
        //server.getInstance().handlePostRequest("/chat/upload", uploadImage);
        

        // 设置静态文件路由
        setupStaticRoutes();
    
        
    }

    bool chatroom::start() {
        if (roomid == -1) {
            return false;
        }
        initializeChatRoom();
        setupChatRoutes();
        Server& server = server.getInstance(HOST);



        server.serveFile("/chat" + to_string(roomid), "html/index.html");
        return true;
    }
    void chatroom::clearMessage() {
        chatMessages.clear();
        return;
    }
    string chatroom::gettittle() {
        return chatTitle;
    }
    void chatroom::setRoomID(int id) {
        roomid = id;
    }
    void chatroom::init() {
        clearMessage();
        allowID.clear();
    }
    void chatroom::setflag(int x) {
        flags = x;
    }
    void chatroom::settittle(string Tittle) {
        chatTitle = Tittle;
    }
    void chatroom::setPasswordHash(const std::string& hash) {
        passwordHash = hash;
    }

    std::string chatroom::getPasswordHash() const {
        return passwordHash;
    }

    void chatroom::setPassword(const std::string& Newpassword) {
        password = Newpassword;
        this->setPasswordHash(SHA256::sha256(Newpassword));
    }

    string chatroom::GetPassword() {
        return password;
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
        return flags & flag;
    }
//---------chatroom

