#include "../include/Server.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include "../json/json.h"
#include "../include/log.h"

std::string HOST = "0.0.0.0";
int PORT = 80;


Server& Server::getInstance(const std::string& host, int port) {
    static Server instance(host, port);
    return instance;
}

Server::Server(const std::string& host, int port)
    : server_host(host), server_port(port) {
}

// 提供 HTML 内容响应
void Server::serveHtml(const std::string& endpoint, const std::string& htmlContent) {
    server.Get(endpoint.c_str(), [htmlContent](const httplib::Request&, httplib::Response& res) {
        res.set_content(htmlContent, "text/html");
        });
}

// 提供文件响应
void Server::serveFile(const std::string& endpoint, const std::string& filePath) {
    server.Get(endpoint.c_str(), [filePath](const httplib::Request&, httplib::Response& res) {
        std::ifstream file(filePath, std::ios::binary);
        if (file) {
            std::ostringstream content;
            content << file.rdbuf();
            res.set_content(content.str(), detectMimeType(filePath));
        }
        else {
            res.status = 404;
            res.set_content("File not found", "text/plain");
        }
        });
}

// 处理 GET 请求
void Server::handleRequest(const std::string& endpoint, const std::function<void(const httplib::Request&, httplib::Response&)>& handler) {
    server.Get(endpoint.c_str(), handler);
}

// 处理带 JSON 的 POST 请求
void Server::handlePostRequest(const std::string& endpoint, const std::function<void(const httplib::Request&, httplib::Response&, const Json::Value&)>& jsonHandler) {
    server.Post(endpoint.c_str(), [jsonHandler](const httplib::Request& req, httplib::Response& res) {
        Json::Value root;
        Json::Reader reader;

        // 尝试解析请求体为 JSON
        if (reader.parse(req.body, root)) {
            jsonHandler(req, res, root);  // 如果是有效的 JSON，调用 JSON 处理函数
        }
        else {
            res.status = 400;
            res.set_content("Invalid JSON", "text/plain");
        }
        });
}

// 处理带文件上传的 POST 请求
void Server::handlePostRequest(const std::string& endpoint, const std::function<void(const httplib::Request&, httplib::Response&)>& fileHandler) {
    server.Post(endpoint.c_str(), [fileHandler](const httplib::Request& req, httplib::Response& res) {
        if (req.has_file("file")) {
            fileHandler(req, res);  // 如果请求中包含文件，调用文件处理函数
        }
        else {
            res.status = 400;
            res.set_content("No file uploaded", "text/plain");
        }
        });
}

// 启动服务器
void Server::start() {
    Logger& logger = Logger::getInstance();
    logger.logInfo("Server", "服务器在" + server_host + "上监听，端口为" + std::to_string(server_port));

    // 注册预路由处理器，检查请求来源 IP 是否被封禁
    server.set_pre_routing_handler([this](const httplib::Request& req, httplib::Response& res) {
        if (this->isBanned(req.remote_addr)) {
            res.status = 403;
            res.set_content("Forbidden: Your IP is banned", "text/plain");
            return httplib::Server::HandlerResponse::Handled;
        }
        return httplib::Server::HandlerResponse::Unhandled;
        });

    // 根路径重定向到 /chatlist
    server.Get("/", [](const httplib::Request&, httplib::Response& res) {
        res.set_redirect("/chatlist");
        });

    server.listen(server_host.c_str(), server_port);
}

// 获取 MIME 类型
std::string Server::detectMimeType(const std::string& filePath) {
    auto pos = filePath.rfind('.');
    if (pos != std::string::npos) {
        std::string extension = filePath.substr(pos);
        if (extension == ".html") return "text/html";
        if (extension == ".css") return "text/css";
        if (extension == ".js") return "application/javascript";
        if (extension == ".png") return "image/png";
        if (extension == ".jpg" || extension == ".jpeg") return "image/jpeg";
        if (extension == ".gif") return "image/gif";
        if (extension == ".txt") return "text/plain";
    }
    return "application/octet-stream";
}

// 设置响应的 Cookie
void Server::setCookie(httplib::Response& res, const std::string& cookieName, const std::string& cookieValue) {
    res.set_header("Set-Cookie", cookieName + "=" + cookieValue);
}

// 发送 JSON 数据作为响应
void Server::sendJson(const Json::Value& jsonResponse, httplib::Response& res) {
    res.set_content(jsonResponse.toStyledString(), "application/json");
}

// 设置跨域请求的 CORS 头信息
void Server::setCorsHeaders(httplib::Response& res) {
    res.set_header("Access-Control-Allow-Origin", "*");
    res.set_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    res.set_header("Access-Control-Allow-Headers", "Content-Type, X-Custom-Header");
}

void Server::setToken(httplib::Response& res, const std::string& uid, const std::string& clientid) {
    // 将 uid 和 clientid 设置为两个 Cookie
    res.set_header("Set-Cookie", "uid=" + uid + "; Path=/; HttpOnly");
    res.set_header("Set-Cookie", "clientid=" + clientid + "; Path=/; HttpOnly");
}

// 添加封禁 IP 的方法实现
void Server::banIP(const std::string& ipAddress) {
    bannedIPs.insert(ipAddress);
}

// 添加封禁 IP 的方法实现
void Server::debanIP(const std::string& ipAddress) {
    bannedIPs.erase(ipAddress);
}

// 判断 IP 是否被封禁（辅助方法）
bool Server::isBanned(const std::string& ipAddress) const {
    return bannedIPs.find(ipAddress) != bannedIPs.end();
}

void Server::setHOST(std::string x) {
    HOST = x;
}

void Server::setPORT(int x) {
    PORT = x;
}

string Server::getHOST() {
    return HOST;
}

int Server::getPORT() {
    return PORT;
}