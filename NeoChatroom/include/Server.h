#ifndef SIMPLE_SERVER_H
#define SIMPLE_SERVER_H
//如果遇到byte冲突，检查一下include顺序
#include "../include/httplib.h"
#include <string>
#include <functional>
#include <unordered_set>  // 添加 unordered_set，用于存储被封禁的 IP
#include "../json/json.h"  // 引入 json 库
const std::string HOST = "0.0.0.0";
const int PORT = 80;
const std::string ROOTURL = "/chat";
class Server {
public:
    // 获取 Server 实例，使用单例模式
    static Server& getInstance(const std::string& host = HOST, int port = PORT);

    // 为指定的 endpoint 提供 HTML 内容响应
    void serveHtml(const std::string& endpoint, const std::string& htmlContent);

    // 为指定的 endpoint 提供文件响应
    void serveFile(const std::string& endpoint, const std::string& filePath);

    // 设置处理 GET 请求的方法
    void handleRequest(const std::string& endpoint, const std::function<void(const httplib::Request&, httplib::Response&)>& handler);

    // 设置处理 POST 请求的方法
    void handlePostRequest(const std::string& endpoint, const std::function<void(const httplib::Request&, httplib::Response&, const Json::Value&)>& handler);
    void handlePostRequest(const std::string& endpoint, const std::function<void(const httplib::Request&, httplib::Response&)>& fileHandler);

    // 启动服务器
    void start();

    // 设置响应 Cookie
    void setCookie(httplib::Response& res, const std::string& cookieName, const std::string& cookieValue);

    // 发送 JSON 数据作为响应
    void sendJson(const Json::Value& jsonResponse, httplib::Response& res);

    // 获取和设置跨域请求的功能
    void setCorsHeaders(httplib::Response& res);

    // 设置用户登录凭据
    void setToken(httplib::Response& res, const std::string& uid, const std::string& clientid);

    // 添加封禁 IP 的方法
    void banIP(const std::string& ipAddress);

    void debanIP(const std::string& ipAddress);

private:
    Server(const std::string& host, int port);
    ~Server() = default;

    Server(const Server&) = delete;
    Server& operator=(const Server&) = delete;

    std::string server_host;
    int server_port;
    httplib::Server server;

    // 根据文件后缀自动推测 MIME 类型
    static std::string detectMimeType(const std::string& filePath);

    // 存储被封禁的 IP 地址
    std::unordered_set<std::string> bannedIPs;

    // 判断 IP 是否被封禁
    bool isBanned(const std::string& ipAddress) const;
};

#endif