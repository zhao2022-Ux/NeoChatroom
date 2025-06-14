#ifndef SIMPLE_SERVER_H
#define SIMPLE_SERVER_H

// 如果遇到byte冲突，检查一下include顺序
#include "../../../../../lib/httplib.h"
#include <string>
#include <functional>
#include <unordered_set>    // 用于存储被封禁的 IP
#include <unordered_map>    // 用于MIME类型映射
#include <mutex>            // 用于线程安全
#include <memory>           // 用于智能指针
#include <atomic>           // 用于原子操作
#include "json/json.h"

// 全局变量声明（保持原有接口不变）
extern std::string HOST;
extern int PORT;

// 根URL常量定义
const std::string ROOTURL = "/chat";

/**
 * HTTP服务器封装类
 * 提供对cpp-httplib的高级封装，支持SSL、IP封禁、文件服务等功能
 * 使用单例模式确保全局唯一实例
 */
class Server {
public:
    /**
     * 获取 Server 实例，使用单例模式
     * @param host 服务器主机地址，默认使用全局HOST变量
     * @param port 服务器端口，默认使用全局PORT变量
     * @return Server实例引用
     */
    static Server& getInstance(const std::string& host = HOST, int port = PORT);

    /**
     * 为指定的端点提供 HTML 内容响应
     * @param endpoint 端点路径
     * @param htmlContent HTML内容字符串
     */
    void serveHtml(const std::string& endpoint, const std::string& htmlContent);

    /**
     * 为指定的端点提供文件响应
     * @param endpoint 端点路径
     * @param filePath 文件系统中的文件路径
     */
    void serveFile(const std::string& endpoint, const std::string& filePath);

    /**
     * 设置处理 GET 请求的方法
     * @param endpoint 端点路径
     * @param handler 请求处理函数
     */
    void handleRequest(const std::string& endpoint,
                      const std::function<void(const httplib::Request&, httplib::Response&)>& handler);

    /**
     * 设置处理带JSON数据的 POST 请求的方法
     * @param endpoint 端点路径
     * @param handler JSON请求处理函数，第三个参数为解析后的JSON对象
     */
    void handlePostRequest(const std::string& endpoint,
                          const std::function<void(const httplib::Request&, httplib::Response&, const Json::Value&)>& handler);

    /**
     * 设置处理带文件上传的 POST 请求的方法
     * @param endpoint 端点路径
     * @param fileHandler 文件上传处理函数
     */
    void handlePostRequest(const std::string& endpoint,
                          const std::function<void(const httplib::Request&, httplib::Response&)>& fileHandler);

    /**
     * 启动服务器
     * 根据SSL配置自动选择HTTP或HTTPS模式
     */
    void start();

    /**
     * 停止服务器
     */
    void stop();

    /**
     * 设置响应 Cookie
     * @param res HTTP响应对象
     * @param cookieName Cookie名称
     * @param cookieValue Cookie值
     */
    void setCookie(httplib::Response& res, const std::string& cookieName, const std::string& cookieValue);

    /**
     * 发送 JSON 数据作为响应
     * @param jsonResponse JSON响应对象
     * @param res HTTP响应对象
     */
    void sendJson(const Json::Value& jsonResponse, httplib::Response& res);

    /**
     * 设置跨域请求的 CORS 头信息
     * @param res HTTP响应对象
     */
    void setCorsHeaders(httplib::Response& res);

    /**
     * 设置用户登录凭据
     * @param res HTTP响应对象
     * @param uid 用户ID
     * @param clientid 客户端ID
     */
    void setToken(httplib::Response& res, const std::string& uid, const std::string& clientid);

    /**
     * 添加IP地址到封禁列表
     * @param ipAddress 要封禁的IP地址
     */
    void banIP(const std::string& ipAddress);

    /**
     * 从封禁列表中移除IP地址
     * @param ipAddress 要解封的IP地址
     */
    void debanIP(const std::string& ipAddress);

    /**
     * 设置全局主机地址
     * @param x 主机地址
     */
    void setHOST(std::string x);

    /**
     * 设置全局端口
     * @param x 端口号
     */
    void setPORT(int x);

    /**
     * 获取全局主机地址
     * @return 主机地址
     */
    std::string getHOST();

    /**
     * 获取全局端口
     * @return 端口号
     */
    int getPORT();

    /**
     * 设置 SSL 证书和密钥文件路径
     * @param cert 证书文件路径
     * @param key 私钥文件路径
     */
    void setSSLCredentials(const std::string& cert, const std::string& key);

    /**
     * 根据文件后缀自动推测 MIME 类型
     * @param filePath 文件路径
     * @return MIME类型字符串
     */
    static std::string detectMimeType(const std::string& filePath);

private:
    /**
     * 构造函数 - 私有化以实现单例模式
     * @param host 服务器主机地址
     * @param port 服务器端口
     */
    Server(const std::string& host, int port);

    /**
     * 析构函数 - 确保服务器正确关闭
     */
    ~Server();

    // 禁用拷贝构造和赋值操作
    Server(const Server&) = delete;
    Server& operator=(const Server&) = delete;

    /**
     * 服务器基本配置
     */
    std::string server_host;        // 服务器主机地址
    int server_port;                // 服务器端口

    /**
     * 服务器实例 - 使用智能指针管理
     * 根据SSL配置自动选择httplib::Server或httplib::SSLServer
     */
    std::unique_ptr<httplib::Server> server;

    /**
     * SSL相关配置
     */
    std::string cert_file;          // SSL证书文件路径
    std::string key_file;           // SSL私钥文件路径
    bool ssl_enabled;               // SSL是否启用

    /**
     * 服务器状态管理
     */
    std::atomic<bool> is_running;   // 服务器运行状态

    /**
     * IP封禁管理
     */
    std::unordered_set<std::string> bannedIPs;    // 被封禁的IP地址集合
    mutable std::mutex banned_ips_mutex;          // IP封禁列表的线程安全锁

    /**
     * 判断IP地址是否被封禁
     * @param ipAddress IP地址
     * @return 是否被封禁
     */
    bool isBanned(const std::string& ipAddress) const;

    /**
     * 确保服务器实例已初始化
     * @return 是否成功初始化
     */
    bool ensureServerInitialized();

    /**
     * 初始化服务器实例
     * 根据SSL配置创建相应的服务器实例
     */
    void initializeServer();

    /**
     * 配置服务器基本设置
     * 包括超时、请求大小限制、错误处理等
     */
    void configureServer();

    /**
     * 注册默认路由
     * 包括根路径重定向、健康检查、CORS预检等
     */
    void registerDefaultRoutes();

    /**
     * 提供文件内容服务的内部实现
     * @param filePath 文件路径
     * @param res HTTP响应对象
     * @return 是否成功提供文件
     */
    bool serveFileContent(const std::string& filePath, httplib::Response& res);
};

#endif // SIMPLE_SERVER_H