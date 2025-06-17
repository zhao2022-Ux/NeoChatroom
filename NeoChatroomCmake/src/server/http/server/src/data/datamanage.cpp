#include "../tool/tool.h"
#include "../config/config.h"
#include "datamanage.h"
#include "../tool/log.h"
#include <map>
#include <mutex>
#include <json/json.h>
#include <vector>
#include <tuple>
#include <json/json.h>
#include <sqlite3.h>
#include <iostream>
#include <unordered_map>
#include <chrono>
#include <thread>
#include <algorithm>
using namespace std;

namespace manager {
    // 转义SQL字符串辅助方法
    std::string escapeSqlString(const std::string& s) {
        std::string escaped_s = "";
        for (char c : s) {
            if (c == '\'') {
                escaped_s += "''";
            } else {
                escaped_s += c;
            }
        }
        return escaped_s;
    }

    std::mutex mtx;  // 用于保护共享数据的互斥锁
    sqlite3* db = nullptr;

    // 用户数据缓存，带时间戳
    struct CachedUser {
        user* userPtr;
        std::chrono::steady_clock::time_point lastAccess;
    };
    std::unordered_map<int, CachedUser> userCacheById;
    std::unordered_map<std::string, CachedUser> userCacheByName;

    // 缓存管理设置
    const size_t MAX_CACHE_SIZE = 1000; // 最大缓存用户数量
    const std::chrono::minutes CACHE_EXPIRY_TIME{ 30 }; // 30分钟过期时间
    bool cacheCleanerRunning = false;

    // 帮助函数以清除用户缓存
    void InvalidateUserCache(int uid, const std::string& name) {
        try {
            lock_guard<mutex> lock(mtx);
            auto itById = userCacheById.find(uid);
            if (itById != userCacheById.end()) {
                delete itById->second.userPtr;
                userCacheById.erase(itById);
            }
            auto itByName = userCacheByName.find(name);
            if (itByName != userCacheByName.end()) {
                // 不在这里删除，因为这是与userCacheById相同的指针
                userCacheByName.erase(itByName);
            }
        }
        catch (const std::exception& e) {
            Logger::getInstance().logError("cache", "清除用户缓存时发生错误: " + string(e.what()));
        }
    }

    // 缓存清理线程函数
    void CleanCache() {
        try {
            while (cacheCleanerRunning) {
                std::this_thread::sleep_for(std::chrono::minutes(5));
                auto now = std::chrono::steady_clock::now();
                size_t cleanedCount = 0;
                {
                    lock_guard<mutex> lock(mtx);
                    // 根据过期时间清理
                    for (auto it = userCacheById.begin(); it != userCacheById.end(); ) {
                        if (now - it->second.lastAccess > CACHE_EXPIRY_TIME) {
                            std::string username = it->second.userPtr->getname(); //SB玩意
                            delete it->second.userPtr;
                            userCacheByName.erase(username);
                            it = userCacheById.erase(it);
                            cleanedCount++;
                        }
                        else {
                            ++it;
                        }
                    }
                    // 如果仍然过大，则根据LRU（最近最少使用）清理
                    if (userCacheById.size() > MAX_CACHE_SIZE) {
                        std::vector<std::pair<int, std::chrono::steady_clock::time_point>> entries;
                        for (const auto& entry : userCacheById) {
                            entries.emplace_back(entry.first, entry.second.lastAccess);
                        }
                        std::sort(entries.begin(), entries.end(),
                            [](const auto& a, const auto& b) { return a.second < b.second; });
                        for (size_t i = 0; i < entries.size() - MAX_CACHE_SIZE / 2; ++i) {
                            auto it = userCacheById.find(entries[i].first);
                            if (it != userCacheById.end()) {
                                std::string username;
                                if (it->second.userPtr != nullptr) {
                                    // 先保存用户名，再删除 userPtr 以避免访问无效内存
                                    username = it->second.userPtr->getname();
                                    delete it->second.userPtr;
                                    it->second.userPtr = nullptr;
                                }
                                if (!username.empty()) {
                                    userCacheByName.erase(username);
                                }
                                userCacheById.erase(it);
                                cleanedCount++;
                            }

                        }
                    }
                }
                if (cleanedCount > 0) {
                    Logger::getInstance().logInfo("cache", "清理了 " + std::to_string(cleanedCount) + " 个过期或最近最少使用的缓存条目。");
                }
                // 增加短暂休眠，防止高速循环导致日志消息无限累积
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }
        catch (const std::exception& e) {
            std::string errMsg = e.what();
            if (errMsg.size() > 512) {
                errMsg = errMsg.substr(0, 512);
            }
            Logger::getInstance().logError("cache", "缓存清理线程发生错误: " + errMsg);
        }
    }



    // 启动缓存清理线程
    void StartCacheCleaner() {
        try {
            if (!cacheCleanerRunning) {
                cacheCleanerRunning = true;
                std::thread(CleanCache).detach();
                Logger::getInstance().logInfo("cache", "缓存清理线程已启动。");
            }
        }
        catch (const std::exception& e) {
            Logger::getInstance().logError("cache", "启动缓存清理线程时发生错误: " + string(e.what()));
        }
    }

    // 停止缓存清理线程
    void StopCacheCleaner() {
        try {
            cacheCleanerRunning = false;
            Logger::getInstance().logInfo("cache", "缓存清理线程已停止。");
        }
        catch (const std::exception& e) {
            Logger::getInstance().logError("cache", "停止缓存清理线程时发生错误: " + string(e.what()));
        }
    }

    // 检查字符是否安全
    bool SafeWord(char word) {
        if (('0' <= word && word <= '9')
            || ('a' <= word && word <= 'z')
            || ('A' <= word && word <= 'Z')
            || word == '_') return true;
        return false;
    }

    // 检查用户名是否合法
    bool CheckUserName(string name) {
        if (name.length() > 50) return false;
        for (int i = 0; i < name.length(); i++) {
            if (!SafeWord(name[i])) return false;
        }
        return true;
    }

    // 存储数据的文件路径
    int usernum = 0;

    string user::getname() { return name; }
    string user::getcookie() { return cookie; }
    string user::getlabei() { return labei; }
    string user::getpassword() { return password; }

    void user::setcookie(string new_cookie) {
        try {
            lock_guard<mutex> lock(mtx);
            cookie = new_cookie;
            Keyword::replace_sql_injection_keywords(cookie);
            // 更新数据库
            const char* updateQuery = R"(
                UPDATE users SET cookie = ? WHERE uid = ?;
            )";
            sqlite3_stmt* stmt;
            if (sqlite3_prepare_v2(db, updateQuery, -1, &stmt, nullptr) == SQLITE_OK) {
                sqlite3_bind_text(stmt, 1, new_cookie.c_str(), -1, SQLITE_STATIC);
                sqlite3_bind_int(stmt, 2, uid);
                if (sqlite3_step(stmt) != SQLITE_DONE) {
                    Logger::getInstance().logError("database", "更新数据库中的cookie失败: " + std::string(sqlite3_errmsg(db)));
                }
                sqlite3_finalize(stmt);
            }
            else {
                Logger::getInstance().logError("database", "准备更新查询语句时出错: " + std::string(sqlite3_errmsg(db)));
            }
            // 更新缓存中的访问时间
            auto now = std::chrono::steady_clock::now();
            auto it = userCacheById.find(uid);
            if (it != userCacheById.end()) {
                it->second.lastAccess = now;
            }
            auto itName = userCacheByName.find(name);
            if (itName != userCacheByName.end()) {
                itName->second.lastAccess = now;
            }
        }
        catch (const std::exception& e) {
            Logger::getInstance().logError("database", "设置cookie时发生错误: " + string(e.what()));
        }
    }

    void user::ban() {
        try {
            lock_guard<mutex> lock(mtx);
            labei = BanedLabei;
            // 更新数据库
            const char* updateQuery = R"(
                UPDATE users SET labei = ? WHERE uid = ?;
            )";
            sqlite3_stmt* stmt;
            if (sqlite3_prepare_v2(db, updateQuery, -1, &stmt, nullptr) == SQLITE_OK) {
                sqlite3_bind_text(stmt, 1, BanedLabei.c_str(), -1, SQLITE_STATIC);
                sqlite3_bind_int(stmt, 2, uid);
                if (sqlite3_step(stmt) != SQLITE_DONE) {
                    Logger::getInstance().logError("database", "更新数据库中的labei失败: " + std::string(sqlite3_errmsg(db)));
                }
                sqlite3_finalize(stmt);
            }
            else {
                Logger::getInstance().logError("database", "准备更新查询语句时出错: " + std::string(sqlite3_errmsg(db)));
            }
            // 使缓存无效
            InvalidateUserCache(uid, name);
        }
        catch (const std::exception& e) {
            Logger::getInstance().logError("database", "禁用用户时发生错误: " + string(e.what()));
        }
    }

    bool user::setname(string str) {
        if (!CheckUserName(str)) {
            return false;
        }
        name = str;
        return true;
    }

    int user::getuid() { return uid; }

    bool user::operator<(user x) {
        return uid < x.uid;
    }

    user::user(string name_, string password_, string cookie_, string labei_) {
        name = name_, password = password_, cookie = cookie_, labei = labei_;
        uid = 0;
    }

    void user::setuid(int value) {
        if (value != -1) {
            uid = value;
            usernum = max(value, usernum);
        }
        else uid = ++usernum;
    }

    // 初始化SQLite3数据库
    void InitDatabase(const std::string& dbPath) {
        try {
            if (sqlite3_open(dbPath.c_str(), &db) != SQLITE_OK) {
                Logger::getInstance().logFatal("database", "打开SQLite3数据库失败: " + std::string(sqlite3_errmsg(db)));
                throw std::runtime_error("打开SQLite3数据库失败");
            }
            const char* createTableQuery = R"(
                CREATE TABLE IF NOT EXISTS users (
                    uid INTEGER PRIMARY KEY AUTOINCREMENT,
                    name TEXT UNIQUE NOT NULL,
                    password TEXT NOT NULL,
                    cookie TEXT,
                    labei TEXT
                );
            )";
            char* errMsg = nullptr;
            if (sqlite3_exec(db, createTableQuery, nullptr, nullptr, &errMsg) != SQLITE_OK) {
                std::string error = "创建表失败: " + std::string(errMsg);
                Logger::getInstance().logFatal("database", error);
                sqlite3_free(errMsg);
                throw std::runtime_error(error);
            }
            StartCacheCleaner();
        }
        catch (const std::exception& e) {
            Logger::getInstance().logFatal("database", "初始化数据库时发生错误: " + string(e.what()));
        }
    }

    // 向数据库中新增用户
    bool AddUser(const std::string& name, const std::string& psw, const std::string& cookie, const std::string& labei) {
        try {
            if (!CheckUserName(name) || Keyword::check_sql_keywords(name)) {
                Logger::getInstance().logError("database", "用户名不合法，发现可能的sql注入: " + name);
                return false;
            }
            const char* insertQuery = R"(
                INSERT INTO users (name, password, cookie, labei)
                VALUES (?, ?, ?, ?);
            )";
            sqlite3_stmt* stmt;
            if (sqlite3_prepare_v2(db, insertQuery, -1, &stmt, nullptr) != SQLITE_OK) {
                Logger::getInstance().logError("database", "准备插入用户数据时发生错误: " + std::string(sqlite3_errmsg(db)));
                return false;
            }
            sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 2, psw.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 3, cookie.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 4, labei.c_str(), -1, SQLITE_STATIC);
            bool success = (sqlite3_step(stmt) == SQLITE_DONE);
            if (!success) {
                Logger::getInstance().logError("database", "插入用户数据失败: " + std::string(sqlite3_errmsg(db)));
            }
            else {
                Logger::getInstance().logInfo("database", "用户 " + name + " 已成功添加。");
                // Retrieve the auto-generated UID
                int newUid = (int)sqlite3_last_insert_rowid(db);
                // Cache the new user
                auto now = std::chrono::steady_clock::now();
                lock_guard<mutex> lock(mtx);
                user* newUser = new user(name, psw, cookie, labei);
                newUser->setuid(newUid);
                userCacheById[newUid] = { newUser, now };
                userCacheByName[name] = { newUser, now };
            }
            sqlite3_finalize(stmt);
            return success;
        }
        catch (const std::exception& e) {
            Logger::getInstance().logError("database", "添加用户时发生错误: " + string(e.what()));
            return false;
        }
    }

    // 通过UID查找用户
    user* FindUser(int uid) {
        try {
            auto now = std::chrono::steady_clock::now();
            {
                lock_guard<mutex> lock(mtx);
                auto it = userCacheById.find(uid);
                if (it != userCacheById.end()) {
                    it->second.lastAccess = now; // 更新访问时间
                    return it->second.userPtr;
                }
            }
            const char* selectQuery = R"(
                SELECT name, password, cookie, labei FROM users WHERE uid = ?;
            )";
            sqlite3_stmt* stmt;
            if (sqlite3_prepare_v2(db, selectQuery, -1, &stmt, nullptr) != SQLITE_OK) {
                Logger::getInstance().logError("database", "查找用户时发生错���: " + std::string(sqlite3_errmsg(db)));
                return nullptr;
            }
            sqlite3_bind_int(stmt, 1, uid);
            user* foundUser = nullptr;
            if (sqlite3_step(stmt) == SQLITE_ROW) {
                std::string name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
                std::string password = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
                std::string cookie = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
                std::string labei = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
                foundUser = new user(name, password, cookie, labei);
                foundUser->setuid(uid);
                // 缓存用户
                lock_guard<mutex> lock(mtx);
                userCacheById[uid] = { foundUser, now };
                userCacheByName[name] = { foundUser, now };
            }
            sqlite3_finalize(stmt);
            return foundUser;
        }
        catch (const std::exception& e) {
            Logger::getInstance().logError("database", "通过UID查找用户时发生错误: " + string(e.what()));
            return nullptr;
        }
    }

    // 通过名称查找用户
    user* FindUser(string name) {
        try {
            auto now = std::chrono::steady_clock::now();
            {
                lock_guard<mutex> lock(mtx);
                auto it = userCacheByName.find(name);
                if (it != userCacheByName.end()) {
                    it->second.lastAccess = now; // 更新访问时间
                    return it->second.userPtr;
                }
            }
            if (Keyword::check_sql_keywords(name)) return nullptr;
            const char* selectQuery = R"(
                SELECT uid, password, cookie, labei FROM users WHERE name = ?;
            )";
            sqlite3_stmt* stmt;
            if (sqlite3_prepare_v2(db, selectQuery, -1, &stmt, nullptr) != SQLITE_OK) {
                return nullptr;
            }
            sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_STATIC);
            user* foundUser = nullptr;
            if (sqlite3_step(stmt) == SQLITE_ROW) {
                int uid = sqlite3_column_int(stmt, 0);
                std::string password = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
                std::string cookie = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
                std::string labei = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
                foundUser = new user(name, password, cookie, labei);
                foundUser->setuid(uid);
                // 缓存用户
                lock_guard<mutex> lock(mtx);
                userCacheById[uid] = { foundUser, now };
                userCacheByName[name] = { foundUser, now };
            }
            sqlite3_finalize(stmt);
            return foundUser;
        }
        catch (const std::exception& e) {
            Logger::getInstance().logError("database", "通过名称查找用户时发生错误: " + string(e.what()));
            return nullptr;
        }
    }

    // 移除用户
    bool BanUser(int uid) {
        try {
            const char* updateQuery = R"(
                UPDATE users SET labei = ? WHERE uid = ?;
            )";
            sqlite3_stmt* stmt;
            if (sqlite3_prepare_v2(db, updateQuery, -1, &stmt, nullptr) != SQLITE_OK) {
                Logger::getInstance().logError("database", "准备封禁用户时发生错误: " + std::string(sqlite3_errmsg(db)));
                return false;
            }
            sqlite3_bind_text(stmt, 1, BanedLabei.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_int(stmt, 2, uid);
            bool success = (sqlite3_step(stmt) == SQLITE_DONE);
            if (success) {
                Logger::getInstance().logInfo("database", "UID为 " + std::to_string(uid) + " 的用户已被封禁。");
                // 使缓存无效
                lock_guard<mutex> lock(mtx);
                auto it = userCacheById.find(uid);
                if (it != userCacheById.end()) {
                    userCacheByName.erase(it->second.userPtr->getname());
                    delete it->second.userPtr;
                    userCacheById.erase(it);
                }
            }
            else {
                Logger::getInstance().logError("database", "封禁用户失败: " + std::string(sqlite3_errmsg(db)));
            }
            sqlite3_finalize(stmt);
            return success;
        }
        catch (const std::exception& e) {
            Logger::getInstance().logError("database", "移除用户时发生错误: " + string(e.what()));
            return false;
        }
    }

    bool RemoveUser(int uid) {
        try {
            const char* deleteQuery = R"(
            DELETE FROM users WHERE uid = ?;
        )";
            sqlite3_stmt* stmt;
            if (sqlite3_prepare_v2(db, deleteQuery, -1, &stmt, nullptr) != SQLITE_OK) {
                Logger::getInstance().logError("database", "准备删除用户时发生错误: " + std::string(sqlite3_errmsg(db)));
                return false;
            }
            sqlite3_bind_int(stmt, 1, uid);
            bool success = (sqlite3_step(stmt) == SQLITE_DONE);
            if (success) {
                Logger::getInstance().logInfo("database", "UID为 " + std::to_string(uid) + " 的用户已被永久删除。");

                // 清除缓存
                lock_guard<mutex> lock(mtx);
                auto it = userCacheById.find(uid);
                if (it != userCacheById.end()) {
                    std::string name = it->second.userPtr->getname();
                    userCacheByName.erase(name);
                    delete it->second.userPtr;
                    userCacheById.erase(it);
                }
            } else {
                Logger::getInstance().logError("database", "删除用户失败: " + std::string(sqlite3_errmsg(db)));
            }
            sqlite3_finalize(stmt);
            return success;
        }
        catch (const std::exception& e) {
            Logger::getInstance().logError("database", "删除用户时发生异常: " + std::string(e.what()));
            return false;
        }
    }


    bool checkInRoom(int number, int uid) {
        try {
            user* targetUser = FindUser(uid);
            if (targetUser == nullptr) {
                return false; // 用户未找到
            }
            string cookie = targetUser->getcookie();
            if (cookie.empty()) {
                return false; // 空cookie
            }
            lock_guard<mutex> lock(mtx); // 保护共享数据访问
            // 分割cookie字符串并检查每个数字
            size_t start = 0;
            size_t end = cookie.find('&');
            while (end != string::npos) {
                string numStr = cookie.substr(start, end - start);
                int currentNum;
                if (str::safeatoi(numStr, currentNum) && currentNum == number) {
                    return true;
                }
                start = end + 1;
                end = cookie.find('&', start);
            }
            // 检查最后一个数字在最后一个'&'之后
            string lastNumStr = cookie.substr(start);
            int lastNum;
            if (str::safeatoi(lastNumStr, lastNum) && lastNum == number) {
                return true;
            }
            return false;
        }
        catch (const std::exception& e) {
            Logger::getInstance().logError("database", "检查用户是否在房间时发生错误: " + string(e.what()));
            return false;
        }
    }

    // 打印异常
    void LogError(string path, string filename, int line) {
        Logger& logger = Logger::getInstance();
        logger.logError("config", "读取用户信息" + path + "/" + filename + " 行" + to_string(line) + "时发生错误");
    }

    // 读取整个文件
    void ReadUserData(string path, string filename) {
        try {
            // 不再需要，因为数据存储在SQLite3中
            throw std::runtime_error("已弃用");
        }
        catch (const std::exception& e) {
            Logger::getInstance().logError("config", "读取用户数据时发生错误: " + string(e.what()));
        }
    }

    // 保存当前用户列表
    void WriteUserData(string path, string filename) {
        try {
            throw std::runtime_error("已弃用");
        }
        catch (const std::exception& e) {
            Logger::getInstance().logError("config", "写入用户数据时发生错误: " + string(e.what()));
        }
    }

    // 函数返回包含用���名、密码和UID的元组向量
    std::vector<std::tuple<std::string, std::string, int>> GetUserDetails() {
        try {
            const char* selectQuery = R"(
                SELECT name, password, uid FROM users;
            )";
            sqlite3_stmt* stmt;
            if (sqlite3_prepare_v2(db, selectQuery, -1, &stmt, nullptr) != SQLITE_OK) {
                return {};
            }
            std::vector<std::tuple<std::string, std::string, int>> userDetails;
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                std::string name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
                std::string password = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
                int uid = sqlite3_column_int(stmt, 2);
                userDetails.emplace_back(name, password, uid);
            }
            sqlite3_finalize(stmt);
            return userDetails;
        }
        catch (const std::exception& e) {
            Logger::getInstance().logError("database", "获取用户详细信息时发生错误: " + string(e.what()));
            return {};
        }
    }

    // 获取指定范围的用户列表
    std::vector<Json::Value> GetUserList(int startUid, int endUid, int pageSize, bool withPassword) {
        try {
            std::vector<Json::Value> userList;
            if (startUid <= 0 || endUid <= 0 || startUid > endUid) {
                Logger::getInstance().logWarning("database", "获取用户列表参数无效: start=" + std::to_string(startUid) + ", end=" + std::to_string(endUid));
                return userList;
            }

            // 限制一次请求的最大用户数量，避免性能问题
            if (endUid - startUid + 1 > pageSize) {
                endUid = startUid + pageSize - 1;
                Logger::getInstance().logInfo("database", "调整用户列表范围至: " + std::to_string(startUid) + "-" + std::to_string(endUid));
            }

            const char* selectQuery = R"(
                SELECT uid, name, labei FROM users 
                WHERE uid >= ? AND uid <= ? 
                ORDER BY uid ASC
                LIMIT ?;
            )";

            sqlite3_stmt* stmt;
            if (sqlite3_prepare_v2(db, selectQuery, -1, &stmt, nullptr) != SQLITE_OK) {
                Logger::getInstance().logError("database", "准备查询用户列表时发生错误: " + std::string(sqlite3_errmsg(db)));
                return userList;
            }

            sqlite3_bind_int(stmt, 1, startUid);
            sqlite3_bind_int(stmt, 2, endUid);
            sqlite3_bind_int(stmt, 3, pageSize);

            // 使用计数器追踪处理的行数
            int rowCount = 0;
            auto startTime = std::chrono::high_resolution_clock::now();

            // 先检查缓存
            std::unordered_map<int, user*> cachedUsers;
            {
                lock_guard<mutex> lock(mtx);
                for (int uid = startUid; uid <= endUid; uid++) {
                    auto it = userCacheById.find(uid);
                    if (it != userCacheById.end()) {
                        cachedUsers[uid] = it->second.userPtr;
                        // 更新访问时间
                        it->second.lastAccess = std::chrono::steady_clock::now();
                    }
                }
            }

            // 处理数据库结果
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                int uid = sqlite3_column_int(stmt, 0);
                Json::Value userObj;
                userObj["uid"] = uid;

                // 如果用户在��存中，优先使用缓存数据
                auto cacheIt = cachedUsers.find(uid);
                if (cacheIt != cachedUsers.end()) {
                    user* cachedUser = cacheIt->second;
                    userObj["username"] = cachedUser->getname();
                    userObj["labei"] = cachedUser->getlabei();
                    if (withPassword) {
                        userObj["password"] = cachedUser->getpassword();
                    }
                } else {
                    // 使用数据库结果
                    std::string name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
                    std::string labei = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
                    userObj["username"] = name;
                    userObj["labei"] = labei;
                }

                userList.push_back(userObj);
                rowCount++;
            }

            sqlite3_finalize(stmt);

            auto endTime = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
            
            // 记录性能信息
            if (duration > 100) { // 只记录慢查询
                Logger::getInstance().logWarning("database", "用户列表查询耗时较长: " + std::to_string(duration) + 
                    "ms, 范围: " + std::to_string(startUid) + "-" + std::to_string(endUid) + 
                    ", 结果数: " + std::to_string(rowCount));
            }

            return userList;
        }
        catch (const std::exception& e) {
            Logger::getInstance().logError("database", "获取用户列表时发生错误: " + string(e.what()));
            return {};
        }
    }

    // 获取用户列表API
    void getUserList(const httplib::Request& req, httplib::Response& res) {
        try {
            // 设置CORS头
            res.set_header("Access-Control-Allow-Origin", "*");
            res.set_header("Access-Control-Allow-Methods", "GET, OPTIONS");
            res.set_header("Access-Control-Allow-Headers", "Content-Type");
            
            // 处理OPTIONS请求（预检请求）
            if (req.method == "OPTIONS") {
                res.status = 204; // No content for OPTIONS
                return;
            }
            
            // 验证参数
            if (!req.has_param("start") || !req.has_param("end")) {
                res.status = 400;
                res.set_content("Missing required parameters: start and end", "text/plain");
                return;
            }
            
            int startUid = 0, endUid = 0;
            std::string startParam = req.get_param_value("start");
            std::string endParam = req.get_param_value("end");
            
            if (!str::safeatoi(startParam, startUid) || !str::safeatoi(endParam, endUid)) {
                res.status = 400;
                res.set_content("Invalid parameters: start and end must be integers", "text/plain");
                return;
            }
            
            // 获取可选参数
            int pageSize = 100; // 默认页大小
            if (req.has_param("size")) {
                std::string sizeParam = req.get_param_value("size");
                int requestedSize = 0;
                if (str::safeatoi(sizeParam, requestedSize) && requestedSize > 0) {
                    // 限制最大页大小为500，防止服务器过载
                    pageSize = std::min(requestedSize, 500);
                }
            }
            
            // 检查范围是否合理
            if (startUid <= 0 || endUid <= 0 || startUid > endUid) {
                res.status = 400;
                res.set_content("Invalid range: start must be positive and not greater than end", "text/plain");
                return;
            }
            
            // 限制一次请求的最大用户数量
            if (endUid - startUid + 1 > 500) {
                endUid = startUid + 499; // 最多500个用户
            }
            
            // 获取用户列表
            std::vector<Json::Value> userList = GetUserList(startUid, endUid, pageSize);
            
            // 构建响应
            Json::Value response;
            response["totalCount"] = userList.size();
            response["startUid"] = startUid;
            response["endUid"] = endUid;
            
            Json::Value usersArray;
            for (const auto& user : userList) {
                usersArray.append(user);
            }
            response["users"] = usersArray;
            
            // 设置分页头部
            res.set_header("X-Total-Count", std::to_string(userList.size()));
            res.set_header("X-Start-Id", std::to_string(startUid));
            res.set_header("X-End-Id", std::to_string(endUid));
            
            // 发送响应
            res.set_content(response.toStyledString(), "application/json");
        }
        catch (const std::exception& e) {
            Logger::getInstance().logError("database", "处理用户列表请求时发生错误: " + string(e.what()));
            res.status = 500;
            res.set_content("Internal server error", "text/plain");
        }
    }

    // 从 chatroom 类移动过来的 Cookie 解析功能
    void transCookie(std::string& cid, std::string& uid, std::string cookie) {
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

    // 从 chatroom 类移动过来的获取用户名API
    void getUsername(const httplib::Request& req, httplib::Response& res) {
        try {
            res.set_header("Access-Control-Allow-Origin", "*"); // 允许所有来源访问
            res.set_header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE"); // 允许的 HTTP 方法
            res.set_header("Access-Control-Allow-Headers", "Content-Type"); // 允许的头部字段

            std::string cookies = req.get_header_value("Cookie");
            std::string password, uid;
            transCookie(password, uid, cookies);

            // 如果 uid 为空，尝试从查询参数获取
            if (uid.empty() && req.has_param("uid")) {
                uid = req.get_param_value("uid");
            }

            // 获取用户名
            int uid_;
            if (uid.empty() || !str::safeatoi(uid, uid_)) {
                res.status = 401;
                res.set_content("Invalid uid or user not found", "text/plain");
                return;
            }

            user* foundUser = FindUser(uid_);
            if (foundUser == nullptr) {
                res.status = 401;
                res.set_content("Invalid uid or user not found", "text/plain");
                return;
            }

            std::string username = foundUser->getname();

            if (username.empty()) {
                res.status = 401;
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
        catch (const std::exception& e) {
            Logger::getInstance().logError("database", "获取用户名时发生错误: " + string(e.what()));
            res.status = 500;
            res.set_content("Internal server error", "text/plain");
        }
    }

    // 获取用户总数
    int getUserCount() {
        try {
            // 使用数据库查询获取用户总数，而不是遍历AllUser数组
            const char* countQuery = "SELECT COUNT(*) FROM users;";
            sqlite3_stmt* stmt;
            
            if (sqlite3_prepare_v2(db, countQuery, -1, &stmt, nullptr) != SQLITE_OK) {
                Logger::getInstance().logError("database", "准备查询用户总数时发生错误");
                return 0;
            }
            
            int count = 0;
            if (sqlite3_step(stmt) == SQLITE_ROW) {
                count = sqlite3_column_int(stmt, 0);
            }
            
            sqlite3_finalize(stmt);
            return count;
        } catch (const std::exception& e) {
            Logger::getInstance().logError("database", "获取用户总数时发生错误: " + std::string(e.what()));
            return 0;
        }
    }

    // 分页获取用户列表
    std::vector<user> getUsers(int start, int end) {
        std::vector<user> result;
        
        try {
            // 确保边界合法
            if (start < 1) start = 1;
            if (end < start) end = start;
            
            // 限制一次请求的最大用户数量
            if (end - start > 500) {
                end = start + 500;
            }
            
            // 使用数据库查询获取用户列表
            const char* selectQuery = R"(
                SELECT uid, name, password, cookie, labei 
                FROM users 
                ORDER BY uid 
                LIMIT ? OFFSET ?;
            )";
            
            sqlite3_stmt* stmt;
            if (sqlite3_prepare_v2(db, selectQuery, -1, &stmt, nullptr) != SQLITE_OK) {
                Logger::getInstance().logError("database", "准备分页查询用户列表时发生错误");
                return result;
            }
            
            int limit = end - start + 1;
            int offset = start - 1;
            
            sqlite3_bind_int(stmt, 1, limit);
            sqlite3_bind_int(stmt, 2, offset);
            
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                int uid = sqlite3_column_int(stmt, 0);
                const char* nameText = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
                const char* passwordText = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
                const char* cookieText = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
                const char* labeiText = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
                
                std::string name = nameText ? nameText : "";
                std::string password = passwordText ? passwordText : "";
                std::string cookie = cookieText ? cookieText : "";
                std::string labei = labeiText ? labeiText : "";
                
                user newUser(name, password, cookie, labei);
                newUser.setuid(uid);
                result.push_back(newUser);
            }
            
            sqlite3_finalize(stmt);
        } catch (const std::exception& e) {
            Logger::getInstance().logError("database", "分页获取用户列表时发生错误: " + std::string(e.what()));
        }
        
        return result;
    }
    
    // 搜索用户
    std::vector<user> searchUsers(const std::string& searchTerm, int start, int end) {
        std::vector<user> result;
        
        try {
            // 确保边界合法
            if (start < 1) start = 1;
            if (end < start) end = start;
            
            // 限制一次请求的最大用户数量
            if (end - start > 500) {
                end = start + 500;
            }
            
            // 转义搜索词，防止SQL注入
            std::string safeTerm = "%" + escapeSqlString(searchTerm) + "%";
            
            // 使用数据库查询搜索用户
            const char* searchQuery = R"(
                SELECT uid, name, password, cookie, labei 
                FROM users 
                WHERE name LIKE ? 
                ORDER BY uid 
                LIMIT ? OFFSET ?;
            )";
            
            sqlite3_stmt* stmt;
            if (sqlite3_prepare_v2(db, searchQuery, -1, &stmt, nullptr) != SQLITE_OK) {
                Logger::getInstance().logError("database", "准备搜索用户查询时发生错误");
                return result;
            }
            
            int limit = end - start + 1;
            int offset = start - 1;
            
            sqlite3_bind_text(stmt, 1, safeTerm.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_int(stmt, 2, limit);
            sqlite3_bind_int(stmt, 3, offset);
            
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                int uid = sqlite3_column_int(stmt, 0);
                const char* nameText = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
                const char* passwordText = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
                const char* cookieText = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
                const char* labeiText = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
                
                std::string name = nameText ? nameText : "";
                std::string password = passwordText ? passwordText : "";
                std::string cookie = cookieText ? cookieText : "";
                std::string labei = labeiText ? labeiText : "";
                
                user newUser(name, password, cookie, labei);
                newUser.setuid(uid);
                result.push_back(newUser);
            }
            
            sqlite3_finalize(stmt);
        } catch (const std::exception& e) {
            Logger::getInstance().logError("database", "搜索用户时发生错误: " + std::string(e.what()));
        }
        
        return result;
    }
    
    // 获取搜索结果总数
    int searchUsersCount(const std::string& searchTerm) {
        try {
            // 转义搜���词，防止SQL注入
            std::string safeTerm = "%" + escapeSqlString(searchTerm) + "%";
            
            // 使用数据库查询获取匹配用户数量
            const char* countQuery = "SELECT COUNT(*) FROM users WHERE name LIKE ?;";
            sqlite3_stmt* stmt;
            
            if (sqlite3_prepare_v2(db, countQuery, -1, &stmt, nullptr) != SQLITE_OK) {
                Logger::getInstance().logError("database", "准备搜索用户计数查询时发生错误");
                return 0;
            }
            
            sqlite3_bind_text(stmt, 1, safeTerm.c_str(), -1, SQLITE_STATIC);
            
            int count = 0;
            if (sqlite3_step(stmt) == SQLITE_ROW) {
                count = sqlite3_column_int(stmt, 0);
            }
            
            sqlite3_finalize(stmt);
            return count;
        } catch (const std::exception& e) {
            Logger::getInstance().logError("database", "获取搜索结果总数时发生错误: " + std::string(e.what()));
            return 0;
        }
    }
    
    // 关闭数据库连接
    void CloseDatabase() {
        try {
        // 停止缓存清理线程
        StopCacheCleaner();
        
        // 等待一段时间确保缓存清理线程完全停止
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        // 清理缓存中的所有用户对象
        {
            lock_guard<mutex> lock(mtx);
            for (auto& entry : userCacheById) {
                if (entry.second.userPtr) {
                    delete entry.second.userPtr;
                    entry.second.userPtr = nullptr;
                }
            }
            userCacheById.clear();
            userCacheByName.clear();
            Logger::getInstance().logInfo("database", "用户缓存已清空");
        }
        
        // 关闭数据库连接
        if (db) {
            int result = sqlite3_close(db);
            if (result != SQLITE_OK) {
                Logger::getInstance().logWarning("database", "关闭数据库时出现警告: " + std::string(sqlite3_errmsg(db)));
            } else {
                db = nullptr;
                Logger::getInstance().logInfo("database", "数据库连接已安全关闭");
            }
        }
        }
        catch (const std::exception& e) {
            Logger::getInstance().logError("database", "关闭数据库时发生错误: " + string(e.what()));
        }
    }
}