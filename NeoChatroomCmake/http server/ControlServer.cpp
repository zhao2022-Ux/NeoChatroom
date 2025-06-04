// ControlServer.cpp
#include "ControlServer.h"
#include "../include/Server.h"
#include "../include/log.h"
#include "../json/json.h"
#include <fstream>
#include <sstream>
#include <unordered_set>
#include <thread>
#include <future>
#include <ctime>

// ─────────────────────────────────────────────────────────────
// 外部依赖：执行命令函数
extern Json::Value command_runner(std::string command, int roomid);
// ─────────────────────────────────────────────────────────────

// ────────────────── Session 管理 ──────────────────
namespace {
    // 简单的内存会话表；如需持久化或分布式请替换为安全方案
    std::unordered_set<std::string> g_allowedSessions;

    std::string generateSessionID() {
        // 随机性足够即可；如有需求可改为 UUID
        return std::to_string(static_cast<uint64_t>(std::hash<std::thread::id>{}(std::this_thread::get_id())))
             + std::to_string(static_cast<uint64_t>(std::time(nullptr)));
    }

    /** 解析 Cookie 中的某个键 */
    std::string extractCookie(const httplib::Request& req, const std::string& key) {
        auto it = req.headers.find("Cookie");
        if (it == req.headers.end()) return {};
        const std::string& cookie = it->second;
        std::string pattern = key + "=";
        auto pos = cookie.find(pattern);
        if (pos == std::string::npos) return {};
        auto end = cookie.find(';', pos);
        return cookie.substr(pos + pattern.size(), end == std::string::npos ? std::string::npos : end - pos - pattern.size());
    }

    /** 鉴权：检查 session_id Cookie 是否有效 */
    bool isAuthenticated(const httplib::Request& req) {
        std::string sessionID = extractCookie(req, "session_id");
        return !sessionID.empty() && g_allowedSessions.count(sessionID);
    }
} // namespace

// ────────────────── 主入口：注册全部路由 ──────────────────
/**
 * @brief 启动控制路由
 * @param password 访问 /control 的口令
 *
 * 注意：此函数只注册路由，不主动调用 Server::start()；
 *       应由主程序在所有路由注册完毕后统一 start。
 */
void startControlServer(const std::string& password) {
    Logger& logger = Logger::getInstance();

    // 获取全局 Server 单例（第一次获取时应传 HOST / PORT）
    Server& server = Server::getInstance(HOST, PORT);

    // ───────── 1. 登录页：GET /control/login ─────────
    //   放在 html/login.html；只提供静态文件
    server.serveFile("/control/login", "html/files/controllogin.html");

    // ───────── 2. 登录提交：POST /control/auth ─────────
    server.handlePostRequest(
        "/control/auth",
        [password, &server](const httplib::Request& req,
                            httplib::Response& res,
                            const Json::Value& json) {
            Logger& logger = Logger::getInstance();

            // 参数校验
            if (!json.isMember("password")) {
                res.status = 400;
                Json::Value err;
                err["status"]  = "error";
                err["message"] = "Missing password";
                server.sendJson(err, res);
                return;
            }

            // 口令验证
            if (json["password"].asString() != password) {
                res.status = 401;
                Json::Value err;
                err["status"]  = "error";
                err["message"] = "Invalid password";
                server.sendJson(err, res);
                return;
            }

            // 生成 Session
            std::string sessionID = generateSessionID();
            g_allowedSessions.insert(sessionID);

            // 返回成功
            Json::Value ok;
            ok["status"]   = "success";
            ok["redirect"] = "/control";
            res.status     = 200;

            // 设置 Cookie（HttpOnly）
            server.setCookie(res, "session_id", sessionID + "; Path=/; HttpOnly");

            server.sendJson(ok, res);

            logger.logInfo("ControlServer", "用户登录成功，分配 session_id=" + sessionID);
        });

    // ───────── 3. 控制面板页：GET /control ─────────
    //   只有已鉴权的会话才能访问
    server.handleRequest(
        "/control",
        [&server](const httplib::Request& req, httplib::Response& res) {
            Logger& logger = Logger::getInstance();

            // 未登录 → 重定向到 /control/login
            if (!isAuthenticated(req)) {
                res.status = 302;
                res.set_header("Location", "/control/login");
                return;
            }

            // 登录通过 → 读取页面文件并返回
            const std::string filePath = "html/control.html";
            std::ifstream file(filePath, std::ios::binary);
            if (!file) {
                logger.logError("ControlServer", "无法打开控制页面文件: " + filePath);
                res.status = 404;
                res.set_content("Control page not found", "text/plain");
                return;
            }
            std::ostringstream oss;
            oss << file.rdbuf();
            res.status = 200;
            res.set_content(oss.str(), server.detectMimeType(filePath));
        });

    // ───────── 4. 命令接口：POST /control/command ─────────
    server.handlePostRequest(
        "/control/command",
        [](const httplib::Request& req,
           httplib::Response& res,
           const Json::Value& json) {
            Logger& logger = Logger::getInstance();

            // 先检查 Cookie 权限（与页面同一逻辑）
            if (!isAuthenticated(req)) {
                res.status = 401;
                Json::Value err;
                err["status"]  = "error";
                err["message"] = "Unauthorized";
                Server::getInstance().sendJson(err, res);
                logger.logWarning("ControlServer", "未授权的命令请求，来源 IP: " + req.remote_addr);
                return;
            }

            // 检查 command 字段
            if (!json.isMember("command") || json["command"].asString().empty()) {
                res.status = 400;
                Json::Value err;
                err["status"]  = "error";
                err["message"] = "Missing or empty command";
                Server::getInstance().sendJson(err, res);
                return;
            }

            std::string command = json["command"].asString();
            logger.logInfo("ControlServer", "收到命令: " + command);

            // 使用 promise / future 执行命令，最大等待 5 秒
            std::promise<Json::Value> prom;
            auto fut = prom.get_future();
            std::thread([command, &prom]() {
                try {
                    Json::Value ret = command_runner(command, 0);
                    prom.set_value(ret);
                } catch (const std::exception& e) {
                    Logger::getInstance().logError("ControlServer", "命令执行异常: " + std::string(e.what()));
                    Json::Value err;
                    err["status"]  = "error";
                    err["message"] = e.what();
                    prom.set_value(err);
                } catch (...) {
                    Logger::getInstance().logError("ControlServer", "命令执行未知异常");
                    Json::Value err;
                    err["status"]  = "error";
                    err["message"] = "Unknown error";
                    prom.set_value(err);
                }
            }).detach();

            // 等待结果
            if (fut.wait_for(std::chrono::seconds(5)) == std::future_status::ready) {
                Json::Value result = fut.get();
                res.status = (result["status"].asString() == "success") ? 200 : 500;
                Server::getInstance().sendJson(result, res);
            } else {
                Json::Value timeout;
                timeout["status"]  = "error";
                timeout["message"] = "Command execution timed out, still running";
                res.status = 202;
                Server::getInstance().sendJson(timeout, res);
                logger.logWarning("ControlServer", "命令执行超时，已返回 202 Accepted");
            }
        });

    logger.logInfo("ControlServer", "控制路由已注册完成，受密码保护");
}
