#include "../include/Server.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <filesystem>
#include <thread>
#include <chrono>
#include "../json/json.h"
#include "../include/log.h"

namespace fs = std::filesystem;

// 全局变量定义（保持原有接口不变）
std::string HOST = "0.0.0.0";
int PORT = 443;
bool startflag = false;

/**
 * 获取服务器单例实例
 * @param host 服务器主机地址
 * @param port 服务器端口
 * @return Server实例引用
 */
Server& Server::getInstance(const std::string& host, int port) {
    static Server instance(host, port);
    return instance;
}

/**
 * 构造函数 - 初始化服务器基本配置
 * @param host 服务器主机地址
 * @param port 服务器端口
 */
Server::Server(const std::string& host, int port)
    : server_host(host),
      server_port(port),
      is_running(false),
      ssl_enabled(false) {
    // 初始化日志记录器
    Logger& logger = Logger::getInstance();
    logger.logInfo("Server", "服务器实例已创建，主机: " + host + "，端口: " + std::to_string(port));
}

/**
 * 析构函数 - 确保服务器正确关闭
 */
Server::~Server() {
    stop();
}

/**
 * 提供 HTML 内容响应
 * @param endpoint 端点路径
 * @param htmlContent HTML内容字符串
 */
void Server::serveHtml(const std::string& endpoint, const std::string& htmlContent) {
    if (!ensureServerInitialized()) {
        Logger::getInstance().logError("Server", "服务器未初始化，无法设置HTML端点: " + endpoint);
        return;
    }

    try {
        server->Get(endpoint.c_str(), [htmlContent](const httplib::Request&, httplib::Response& res) {
            res.set_content(htmlContent, "text/html"); // 保持原有的MIME类型格式
        });
        Logger::getInstance().logInfo("Server", "HTML端点注册成功: " + endpoint);
    }
    catch (const std::exception& e) {
        Logger::getInstance().logError("Server", "注册HTML端点失败 " + endpoint + ": " + e.what());
    }
}

/**
 * 提供文件响应
 * @param endpoint 端点路径
 * @param filePath 文件路径
 */
void Server::serveFile(const std::string& endpoint, const std::string& filePath) {
    if (!ensureServerInitialized()) {
        Logger::getInstance().logError("Server", "服务器未初始化，无法设置文件端点: " + endpoint);
        return;
    }

    try {
        server->Get(endpoint.c_str(), [filePath, this](const httplib::Request&, httplib::Response& res) {
            if (!this->serveFileContent(filePath, res)) {
                res.status = 404;
                res.set_content("File not found", "text/plain"); // 保持原有的简洁错误消息
                Logger::getInstance().logWarning("Server", "请求的文件不存在: " + filePath);
            }
        });
        Logger::getInstance().logInfo("Server", "文件端点注册成功: " + endpoint + " -> " + filePath);
    }
    catch (const std::exception& e) {
        Logger::getInstance().logError("Server", "注册文件端点失败 " + endpoint + ": " + e.what());
    }
}

/**
 * 处理 GET 请求
 * @param endpoint 端点路径
 * @param handler 请求处理函数
 */
void Server::handleRequest(const std::string& endpoint, const std::function<void(const httplib::Request&, httplib::Response&)>& handler) {
    if (!handler) {
        Logger::getInstance().logError("Server", "GET请求处理器为空: " + endpoint);
        throw std::invalid_argument("请求处理器不能为空");
    }

    if (!ensureServerInitialized()) {
        Logger::getInstance().logError("Server", "服务器未初始化，无法注册GET端点: " + endpoint);
        throw std::runtime_error("服务器实例未初始化");
    }

    try {
        server->Get(endpoint.c_str(), [handler, endpoint](const httplib::Request& req, httplib::Response& res) {
            try {
                handler(req, res);
            }
            catch (const std::exception& e) {
                Logger::getInstance().logError("Server", "处理GET请求时发生异常 " + endpoint + ": " + e.what());
                // 不要设置通用错误响应，让原始处理器的错误响应保持不变
                // 只记录日志，不覆盖已设置的响应内容
                if (res.status == 0) { // 只有在没有设置状态码时才设置默认错误
                    res.status = 500;
                    res.set_content("Internal Server Error", "text/plain");
                }
            }
        });
        Logger::getInstance().logInfo("Server", "GET端点注册成功: " + endpoint);
    }
    catch (const std::exception& e) {
        Logger::getInstance().logError("Server", "注册GET端点失败 " + endpoint + ": " + e.what());
        throw;
    }
}

/**
 * 处理带 JSON 的 POST 请求
 * @param endpoint 端点路径
 * @param jsonHandler JSON处理函数
 */
void Server::handlePostRequest(const std::string& endpoint, const std::function<void(const httplib::Request&, httplib::Response&, const Json::Value&)>& jsonHandler) {
    if (!jsonHandler) {
        Logger::getInstance().logError("Server", "POST JSON请求处理器为空: " + endpoint);
        throw std::invalid_argument("请求处理器不能为空");
    }

    if (!ensureServerInitialized()) {
        Logger::getInstance().logError("Server", "服务器未初始化，无法注册POST JSON端点: " + endpoint);
        throw std::runtime_error("服务器实例未初始化");
    }

    try {
        server->Post(endpoint.c_str(), [jsonHandler, endpoint](const httplib::Request& req, httplib::Response& res) {
            Json::Value root;
            Json::Reader reader;

            try {
                if (reader.parse(req.body, root)) {
                    jsonHandler(req, res, root);
                }
                else {
                    Logger::getInstance().logWarning("Server", "无效的JSON数据在端点 " + endpoint + ": " + reader.getFormattedErrorMessages());
                    res.status = 400;
                    res.set_content("无效的JSON格式", "text/plain; charset=utf-8");
                }
            }
            catch (const std::exception& e) {
                Logger::getInstance().logError("Server", "处理POST JSON请求时发生异常 " + endpoint + ": " + e.what());
                res.status = 500;
                res.set_content("服务器内部错误", "text/plain; charset=utf-8");
            }
        });
        Logger::getInstance().logInfo("Server", "POST JSON端点注册成功: " + endpoint);
    }
    catch (const std::exception& e) {
        Logger::getInstance().logError("Server", "注册POST JSON端点失败 " + endpoint + ": " + e.what());
        throw;
    }
}

/**
 * 处理带文件上传的 POST 请求
 * @param endpoint 端点路径
 * @param fileHandler 文件处理函数
 */
void Server::handlePostRequest(const std::string& endpoint, const std::function<void(const httplib::Request&, httplib::Response&)>& fileHandler) {
    if (!fileHandler) {
        Logger::getInstance().logError("Server", "POST文件请求处理器为空: " + endpoint);
        throw std::invalid_argument("请求处理器不能为空");
    }

    if (!ensureServerInitialized()) {
        Logger::getInstance().logError("Server", "服务器未初始化，无法注册POST文件端点: " + endpoint);
        throw std::runtime_error("服务器实例未初始化");
    }

    try {
        server->Post(endpoint.c_str(), [fileHandler, endpoint](const httplib::Request& req, httplib::Response& res) {
            try {
                if (req.has_file("file")) {
                    fileHandler(req, res);
                }
                else {
                    Logger::getInstance().logWarning("Server", "POST请求中缺少文件上传在端点: " + endpoint);
                    // 只有在处理器没有设置响应时才设置默认错误
                    if (res.status == 0) {
                        res.status = 400;
                        res.set_content("No file uploaded", "text/plain");
                    }
                }
            }
            catch (const std::exception& e) {
                Logger::getInstance().logError("Server", "处理POST文件请求时发生异常 " + endpoint + ": " + e.what());
                // 不要覆盖已设置的错误响应
                if (res.status == 0) {
                    res.status = 500;
                    res.set_content("Internal Server Error", "text/plain");
                }
            }
        });
        Logger::getInstance().logInfo("Server", "POST文件端点注册成功: " + endpoint);
    }
    catch (const std::exception& e) {
        Logger::getInstance().logError("Server", "注册POST文件端点失败 " + endpoint + ": " + e.what());
        throw;
    }
}

/**
 * 启动服务器
 */
void Server::start() {
    if (is_running) {
        Logger::getInstance().logWarning("Server", "服务器已在运行中");
        return;
    }

    // 设置默认SSL证书
    setSSLCredentials("server.crt", "server.key");

    startflag = true;
    Logger& logger = Logger::getInstance();
    logger.logInfo("Server", "准备启动服务器，主机: " + server_host + "，端口: " + std::to_string(server_port));

    try {
        // 初始化服务器实例
        initializeServer();

        // 配置服务器设置
        configureServer();

        // 注册默认路由
        registerDefaultRoutes();

        // 标记服务器为运行状态
        is_running = true;

        logger.logInfo("Server", "服务器启动成功，监听地址: " + server_host + ":" + std::to_string(server_port));

        // 启动服务器监听
        if (!server->listen(server_host.c_str(), server_port)) {
            is_running = false;
            throw std::runtime_error("服务器启动失败，可能端口已被占用");
        }
    }
    catch (const std::exception& e) {
        is_running = false;
        logger.logError("Server", "服务器启动失败: " + std::string(e.what()));
        throw;
    }
}

/**
 * 停止服务器
 */
void Server::stop() {
    if (!is_running || !server) {
        return;
    }

    try {
        server->stop(); // 停止服务器
        is_running = false;
        Logger::getInstance().logInfo("Server", "服务器已停止");
    }
    catch (const std::exception& e) {
        Logger::getInstance().logError("Server", "停止服务器时发生错误: " + std::string(e.what()));
    }
}

/**
 * 获取 MIME 类型
 * @param filePath 文件路径
 * @return MIME类型字符串
 */
std::string Server::detectMimeType(const std::string& filePath) {
    auto pos = filePath.rfind('.');
    if (pos == std::string::npos) {
        return "application/octet-stream";
    }

    std::string extension = filePath.substr(pos);

    // 保持原有的简单检测逻辑，避免过度优化导致兼容性问题
    if (extension == ".html") return "text/html";
    if (extension == ".css") return "text/css";
    if (extension == ".js") return "application/javascript";
    if (extension == ".png") return "image/png";
    if (extension == ".jpg" || extension == ".jpeg") return "image/jpeg";
    if (extension == ".gif") return "image/gif";
    if (extension == ".txt") return "text/plain";

    return "application/octet-stream";
}

/**
 * 设置响应的 Cookie
 * @param res HTTP响应对象
 * @param cookieName Cookie名称
 * @param cookieValue Cookie值
 */
void Server::setCookie(httplib::Response& res, const std::string& cookieName, const std::string& cookieValue) {
    if (cookieName.empty()) {
        Logger::getInstance().logWarning("Server", "Cookie名称为空，跳过设置");
        return;
    }

    try {
        std::string cookieHeader = cookieName + "=" + cookieValue + "; Path=/; HttpOnly; SameSite=Strict";
        res.set_header("Set-Cookie", cookieHeader);
    }
    catch (const std::exception& e) {
        Logger::getInstance().logError("Server", "设置Cookie失败: " + std::string(e.what()));
    }
}

/**
 * 发送 JSON 数据作为响应
 * @param jsonResponse JSON响应对象
 * @param res HTTP响应对象
 */
void Server::sendJson(const Json::Value& jsonResponse, httplib::Response& res) {
    try {
        std::string jsonString = jsonResponse.toStyledString();
        res.set_content(jsonString, "application/json; charset=utf-8");
    }
    catch (const std::exception& e) {
        Logger::getInstance().logError("Server", "发送JSON响应失败: " + std::string(e.what()));
        res.status = 500;
        res.set_content("{\"error\":\"内部服务器错误\"}", "application/json; charset=utf-8");
    }
}

/**
 * 设置跨域请求的 CORS 头信息
 * @param res HTTP响应对象
 */
void Server::setCorsHeaders(httplib::Response& res) {
    res.set_header("Access-Control-Allow-Origin", "*");
    res.set_header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
    res.set_header("Access-Control-Allow-Headers", "Content-Type, Authorization, X-Custom-Header");
    res.set_header("Access-Control-Max-Age", "86400"); // 24小时
}

/**
 * 设置用户登录凭据
 * @param res HTTP响应对象
 * @param uid 用户ID
 * @param clientid 客户端ID
 */
void Server::setToken(httplib::Response& res, const std::string& uid, const std::string& clientid) {
    if (uid.empty() || clientid.empty()) {
        Logger::getInstance().logWarning("Server", "UID或ClientID为空，跳过设置令牌");
        return;
    }

    try {
        std::string uidCookie = "uid=" + uid + "; Path=/; HttpOnly; SameSite=Strict";
        std::string clientCookie = "clientid=" + clientid + "; Path=/; HttpOnly; SameSite=Strict";

        res.set_header("Set-Cookie", uidCookie);
        res.set_header("Set-Cookie", clientCookie);
    }
    catch (const std::exception& e) {
        Logger::getInstance().logError("Server", "设置令牌失败: " + std::string(e.what()));
    }
}

/**
 * 添加封禁 IP
 * @param ipAddress IP地址
 */
void Server::banIP(const std::string& ipAddress) {
    if (ipAddress.empty()) {
        Logger::getInstance().logWarning("Server", "尝试封禁空IP地址");
        return;
    }

    std::lock_guard<std::mutex> lock(banned_ips_mutex);
    bool inserted = bannedIPs.insert(ipAddress).second;

    if (inserted) {
        Logger::getInstance().logInfo("Server", "IP地址已封禁: " + ipAddress);
    } else {
        Logger::getInstance().logInfo("Server", "IP地址已在封禁列表中: " + ipAddress);
    }
}

/**
 * 取消封禁 IP
 * @param ipAddress IP地址
 */
void Server::debanIP(const std::string& ipAddress) {
    if (ipAddress.empty()) {
        Logger::getInstance().logWarning("Server", "尝试解封空IP地址");
        return;
    }

    std::lock_guard<std::mutex> lock(banned_ips_mutex);
    size_t removed = bannedIPs.erase(ipAddress);

    if (removed > 0) {
        Logger::getInstance().logInfo("Server", "IP地址已解封: " + ipAddress);
    } else {
        Logger::getInstance().logInfo("Server", "IP地址不在封禁列表中: " + ipAddress);
    }
}

/**
 * 判断 IP 是否被封禁
 * @param ipAddress IP地址
 * @return 是否被封禁
 */
bool Server::isBanned(const std::string& ipAddress) const {
    if (ipAddress.empty()) {
        return false;
    }

    std::lock_guard<std::mutex> lock(banned_ips_mutex);
    return bannedIPs.find(ipAddress) != bannedIPs.end();
}

/**
 * 设置主机地址
 * @param x 主机地址
 */
void Server::setHOST(std::string x) {
    HOST = x;
    Logger::getInstance().logInfo("Server", "全局主机地址已设置为: " + x);
}

/**
 * 设置端口
 * @param x 端口号
 */
void Server::setPORT(int x) {
    if (x <= 0 || x > 65535) {
        Logger::getInstance().logError("Server", "无效端口号: " + std::to_string(x));
        throw std::invalid_argument("端口号必须在1-65535范围内");
    }

    PORT = x;
    Logger::getInstance().logInfo("Server", "全局端口已设置为: " + std::to_string(x));
}

/**
 * 获取主机地址
 * @return 主机地址
 */
std::string Server::getHOST() {
    return HOST;
}

/**
 * 获取端口
 * @return 端口号
 */
int Server::getPORT() {
    return PORT;
}

/**
 * 设置 SSL 证书和密钥
 * @param cert 证书文件路径
 * @param key 密钥文件路径
 */
void Server::setSSLCredentials(const std::string& cert, const std::string& key) {
    cert_file = cert;
    key_file = key;

    // 检查SSL文件是否存在
    ssl_enabled = (!cert.empty() && !key.empty() &&
                   fs::exists(cert) && fs::exists(key));

    if (ssl_enabled) {
        Logger::getInstance().logInfo("Server", "SSL证书配置成功: " + cert + ", " + key);
    } else {
        Logger::getInstance().logWarning("Server", "SSL证书文件不存在或路径为空，将使用HTTP模式");
    }
}

/**
 * 确保服务器已初始化
 * @return 是否成功初始化
 */
bool Server::ensureServerInitialized() {
    if (!server) {
        try {
            initializeServer();
            return true;
        }
        catch (const std::exception& e) {
            Logger::getInstance().logError("Server", "初始化服务器失败: " + std::string(e.what()));
            return false;
        }
    }
    return true;
}

/**
 * 初始化服务器实例
 */
void Server::initializeServer() {
    if (server) {
        return; // 已初始化
    }

    Logger& logger = Logger::getInstance();

    try {
        if (ssl_enabled) {
            server = std::make_unique<httplib::SSLServer>(cert_file.c_str(), key_file.c_str());
            logger.logInfo("Server", "SSL服务器实例创建成功");
        } else {
            server = std::make_unique<httplib::Server>();
            logger.logInfo("Server", "HTTP服务器实例创建成功");
        }
    }
    catch (const std::exception& e) {
        logger.logError("Server", "创建服务器实例失败: " + std::string(e.what()));
        throw;
    }
}

/**
 * 配置服务器设置
 */
void Server::configureServer() {
    if (!server) {
        throw std::runtime_error("服务器实例未初始化");
    }

    // 设置服务器超时
    server->set_read_timeout(30, 0);  // 30秒读取超时
    server->set_write_timeout(30, 0); // 30秒写入超时

    // 设置最大请求体大小 (100MB)
    server->set_payload_max_length(100 * 1024 * 1024);

    // 注册预路由处理器 - IP封禁检查
    server->set_pre_routing_handler([this](const httplib::Request& req, httplib::Response& res) {
        if (this->isBanned(req.remote_addr)) {
            Logger::getInstance().logWarning("Server", "封禁IP尝试访问: " + req.remote_addr);
            res.status = 403;
            res.set_content("Forbidden: Your IP is banned", "text/plain"); // 保持原有的错误消息格式
            return httplib::Server::HandlerResponse::Handled;
        }
        return httplib::Server::HandlerResponse::Unhandled;
    });

    // 移除全局错误处理器，让具体的路由处理器返回自己的错误信息
    // 这样可以保持原有的多样化错误消息

    Logger::getInstance().logInfo("Server", "服务器配置完成");
}

/**
 * 注册默认路由
 */
void Server::registerDefaultRoutes() {
    if (!server) {
        throw std::runtime_error("服务器实例未初始化");
    }

    // 根路径重定向到 /chatlist（保持原有行为）
    server->Get("/", [](const httplib::Request&, httplib::Response& res) {
        res.set_redirect("/chatlist");
    });

    // 健康检查端点
    server->Get("/health", [](const httplib::Request&, httplib::Response& res) {
        Json::Value health;
        health["status"] = "healthy";
        health["timestamp"] = std::time(nullptr);
        res.set_content(health.toStyledString(), "application/json; charset=utf-8");
    });

    // OPTIONS请求处理（CORS预检）
    server->Options(".*", [this](const httplib::Request&, httplib::Response& res) {
        this->setCorsHeaders(res);
        res.status = 200;
    });

    Logger::getInstance().logInfo("Server", "默认路由注册完成");
}

/**
 * 提供文件内容服务
 * @param filePath 文件路径
 * @param res HTTP响应对象
 * @return 是否成功提供文件
 */
bool Server::serveFileContent(const std::string& filePath, httplib::Response& res) {
    try {
        std::ifstream file(filePath, std::ios::binary);
        if (!file.is_open()) {
            return false;
        }

        // 获取文件大小
        file.seekg(0, std::ios::end);
        std::streamsize fileSize = file.tellg();
        file.seekg(0, std::ios::beg);

        if (fileSize > 100 * 1024 * 1024) { // 100MB限制
            Logger::getInstance().logWarning("Server", "文件过大，拒绝提供服务: " + filePath);
            return false;
        }

        // 读取文件内容
        std::ostringstream content;
        content << file.rdbuf();

        // 设置响应
        res.set_content(content.str(), detectMimeType(filePath));

        // 添加缓存头
        res.set_header("Cache-Control", "public, max-age=3600");

        return true;
    }
    catch (const std::exception& e) {
        Logger::getInstance().logError("Server", "提供文件服务时发生错误 " + filePath + ": " + e.what());
        return false;
    }
}