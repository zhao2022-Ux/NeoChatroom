#include "../include/tool.h"
#include "../include/config.h"
#include "../include/datamanage.h"
#include "../include/log.h" // 包含日志库
#include <map>
#include <mutex>
#include <vector>
#include <tuple>
#include <sqlite3.h>
#include <iostream>
#include <unordered_map>
#include <chrono>
#include <thread>
#include <algorithm>
using namespace std;
namespace manager {
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
    // 缓存清理线程函数
    void CleanCache() {
        while (cacheCleanerRunning) {
            std::this_thread::sleep_for(std::chrono::minutes(5)); // 每5分钟运行一次
            auto now = std::chrono::steady_clock::now();
            size_t cleanedCount = 0;
            {
                lock_guard<mutex> lock(mtx);
                // 根据过期时间清理
                for (auto it = userCacheById.begin(); it != userCacheById.end(); ) {
                    if (now - it->second.lastAccess > CACHE_EXPIRY_TIME) {
                        delete it->second.userPtr;
                        userCacheByName.erase(it->second.userPtr->getname());
                        it = userCacheById.erase(it);
                        cleanedCount++;
                    }
                    else {
                        ++it;
                    }
                }
                // 如果仍然过大，则根据LRU（最近最少使用）清理
                if (userCacheById.size() > MAX_CACHE_SIZE) {
                    // 创建一个包含缓存条目的向量以便按最后访问时间排序
                    std::vector<std::pair<int, std::chrono::steady_clock::time_point>> entries;
                    for (const auto& entry : userCacheById) {
                        entries.emplace_back(entry.first, entry.second.lastAccess);
                    }
                    // 按最后访问时间排序（最早先）
                    std::sort(entries.begin(), entries.end(),
                        [](const auto& a, const auto& b) { return a.second < b.second; });
                    // 删除最旧的条目直到我们低于限制
                    for (size_t i = 0; i < entries.size() - MAX_CACHE_SIZE / 2; ++i) {
                        auto it = userCacheById.find(entries[i].first);
                        if (it != userCacheById.end()) {
                            delete it->second.userPtr;
                            userCacheByName.erase(it->second.userPtr->getname());
                            userCacheById.erase(it);
                            cleanedCount++;
                        }
                    }
                }
            }
            if (cleanedCount > 0) {
                Logger::getInstance().logInfo("cache", "清理了 " + std::to_string(cleanedCount) + " 个过期或最近最少使用的缓存条目。");
            }
        }
    }
    // 启动缓存清理线程
    void StartCacheCleaner() {
        if (!cacheCleanerRunning) {
            cacheCleanerRunning = true;
            std::thread(CleanCache).detach();
            Logger::getInstance().logInfo("cache", "缓存清理线程已启动。");
        }
    }
    // 停止缓存清理线程
    void StopCacheCleaner() {
        cacheCleanerRunning = false;
        Logger::getInstance().logInfo("cache", "缓存清理线程已停止。");
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
    void user::ban() {
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
    bool user::setname(string str) {
        if (!CheckUserName(str)) {
            return false;
        }
        name = str;
        return true;
    }
    // 这玩意返回的是TM int!!! int!!! int!!!
    int user::getuid() { return uid; }
    bool user::operator <(user x) {
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
        if (sqlite3_open(dbPath.c_str(), &db) != SQLITE_OK) {
            throw std::runtime_error("打开SQLite3数据库失败");
        }
        const char* createTableQuery = R"(
            CREATE TABLE IF NOT EXISTS users (
                uid INTEGER PRIMARY KEY,
                name TEXT UNIQUE NOT NULL,
                password TEXT NOT NULL,
                cookie TEXT,
                labei TEXT
            );
        )";
        char* errMsg = nullptr;
        if (sqlite3_exec(db, createTableQuery, nullptr, nullptr, &errMsg) != SQLITE_OK) {
            std::string error = "创建表失败: " + std::string(errMsg);
            sqlite3_free(errMsg);
            throw std::runtime_error(error);
        }
        // 启动缓存清理器
        StartCacheCleaner();
    }
    // 向数据库中新增用户
    bool AddUser(const std::string& name, const std::string& psw, const std::string& cookie, const std::string& labei) {
        if (!CheckUserName(name) || Keyword::check_sql_keywords(name)) {
            Logger::getInstance().logError("database", "用户名不合法: " + name);
            return false;
        }
        
        //Keyword::replace_sql_injection_keywords(name);
        
        const char* getMaxUidQuery = R"(
            SELECT IFNULL(MAX(uid), 0) FROM users;
        )";
        sqlite3_stmt* stmt;
        if (sqlite3_prepare_v2(db, getMaxUidQuery, -1, &stmt, nullptr) != SQLITE_OK) {
            Logger::getInstance().logError("database", "获取最大UID时发生错误: " + std::string(sqlite3_errmsg(db)));
            return false;
        }
        int newUid = 0;
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            newUid = sqlite3_column_int(stmt, 0) + 1;
        }
        sqlite3_finalize(stmt);
        const char* insertQuery = R"(
            INSERT INTO users (uid, name, password, cookie, labei)
            VALUES (?, ?, ?, ?, ?);
        )";
        if (sqlite3_prepare_v2(db, insertQuery, -1, &stmt, nullptr) != SQLITE_OK) {
            Logger::getInstance().logError("database", "准备插入用户数据时发生错误: " + std::string(sqlite3_errmsg(db)));
            return false;
        }
        sqlite3_bind_int(stmt, 1, newUid);
        sqlite3_bind_text(stmt, 2, name.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 3, psw.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 4, cookie.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 5, labei.c_str(), -1, SQLITE_STATIC);
        bool success = (sqlite3_step(stmt) == SQLITE_DONE);
        if (!success) {
            Logger::getInstance().logError("database", "插入用户数据失败: " + std::string(sqlite3_errmsg(db)));
        }
        else {
            Logger::getInstance().logInfo("database", "用户 " + name + " 已成功添加。");
            // 将新用户缓存
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
    // 通过UID查找用户
    user* FindUser(int uid) {
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
            Logger::getInstance().logError("database", "查找用户时发生错误: " + std::string(sqlite3_errmsg(db)));
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
    // 通过名称查找用户
    user* FindUser(string name) {
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
    // 移除用户
    bool RemoveUser(int uid) {
        const char* updateQuery = R"(
            UPDATE users SET labei = ? WHERE uid = ?;
        )";
        sqlite3_stmt* stmt;
        if (sqlite3_prepare_v2(db, updateQuery, -1, &stmt, nullptr) != SQLITE_OK) {
            Logger::getInstance().logError("database", "准备移除用户时发生错误: " + std::string(sqlite3_errmsg(db)));
            return false;
        }
        sqlite3_bind_text(stmt, 1, BanedLabei.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 2, uid);
        bool success = (sqlite3_step(stmt) == SQLITE_DONE);
        if (success) {
            Logger::getInstance().logInfo("database", "UID为 " + std::to_string(uid) + " 的用户已被移除。");
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
            Logger::getInstance().logError("database", "移除用户失败: " + std::string(sqlite3_errmsg(db)));
        }
        sqlite3_finalize(stmt);
        return success;
    }
    bool checkInRoom(int number, int uid) {
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
    // 读取时的缓冲区
    config DataFile;
    std::vector<item> list;
    // 打印异常
    void LogError(string path, string filename, int line) {
        Logger& logger = Logger::getInstance();
        logger.logError("config", "读取用户信息" + path + "/" + filename + " 行" + to_string(line) + "时发生错误");
    }
    // 读取整个文件
    void ReadUserData(string path, string filename) {
        // 不再需要，因为数据存储在SQLite3中
        throw std::runtime_error("已弃用");
    }
    // 保存当前用户列表
    void WriteUserData(string path, string filename) {
        throw std::runtime_error("已弃用");
    }
    // 函数返回包含用户名、密码和UID的元组向量
    std::vector<std::tuple<std::string, std::string, int>> GetUserDetails() {
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
    // 关闭SQLite3数据库，增强健壮性并记录日志
    void CloseDatabase() {
        StopCacheCleaner();
        // 清理所有缓存的用户
        {
            lock_guard<mutex> lock(mtx);
            for (auto& entry : userCacheById) {
                delete entry.second.userPtr;
            }
            userCacheById.clear();
            userCacheByName.clear();
        }
        if (db) {
            if (sqlite3_close(db) == SQLITE_OK) {
                Logger::getInstance().logInfo("database", "数据库已成功关闭。");
            }
            else {
                Logger::getInstance().logError("database", "关闭数据库时发生错误: " + std::string(sqlite3_errmsg(db)));
            }
            db = nullptr;
        }
        else {
            Logger::getInstance().logInfo("database", "数据库已关闭或未初始化。");
        }
    }
}