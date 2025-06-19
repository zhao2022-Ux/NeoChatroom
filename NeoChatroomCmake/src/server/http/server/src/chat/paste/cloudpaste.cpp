//
// Created by seve on 25-6-19.
//

#include "cloudpaste.h"
#include <iostream>
#include <ctime>
#include <chrono>
#include <Server.h>
#include "../../data/datamanage.h"  // 引入用户管理模块

// 初始化静态成员变量
CloudPaste* CloudPaste::instance = nullptr;

CloudPaste::CloudPaste() : db(nullptr) {
    initDatabase();
}

CloudPaste::~CloudPaste() {
    if (db) {
        // 确保关闭数据库前执行WAL检查点
        sqlite3_exec(db, "PRAGMA wal_checkpoint(FULL);", nullptr, nullptr, nullptr);
        sqlite3_close(db);
        db = nullptr;
    }
}

CloudPaste& CloudPaste::getInstance() {
    if (instance == nullptr) {
        instance = new CloudPaste();
    }
    return *instance;
}

void CloudPaste::releaseInstance() {
    if (instance != nullptr) {
        delete instance;
        instance = nullptr;
    }
}

bool CloudPaste::initDatabase() {
    int rc = sqlite3_open("database.db", &db);

    if (rc) {
        std::cerr << "无法打开数据库: " << sqlite3_errmsg(db) << std::endl;
        return false;
    }

    // 设置SQLite的性能优化选项
    executeQuery("PRAGMA synchronous = NORMAL;");
    executeQuery("PRAGMA journal_mode = WAL;");
    executeQuery("PRAGMA cache_size = 10000;");
    executeQuery("PRAGMA mmap_size = 30000000000;");
    executeQuery("PRAGMA temp_store = MEMORY;");

    // 创建剪贴板表 - 使用IF NOT EXISTS简化逻辑
    const char* sql = "CREATE TABLE IF NOT EXISTS pastes ("
                     "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                     "title TEXT, "
                     "content TEXT, "
                     "author TEXT, "
                     "timestamp INTEGER, "
                     "language TEXT, "
                     "is_private INTEGER DEFAULT 0, "
                     "password TEXT, "
                     "expiry_days INTEGER DEFAULT 0);";

    if (!executeQuery(sql)) {
        std::cerr << "创建pastes表失败: " << sqlite3_errmsg(db) << std::endl;
        return false;
    }

    // 添加性能关键索引 - 使用IF NOT EXISTS简化逻辑
    executeQuery("CREATE INDEX IF NOT EXISTS idx_pastes_author ON pastes(author);");
    executeQuery("CREATE INDEX IF NOT EXISTS idx_pastes_timestamp ON pastes(timestamp);");
    executeQuery("CREATE INDEX IF NOT EXISTS idx_pastes_private ON pastes(is_private);");

    return true;
}

bool CloudPaste::executeQuery(const std::string& query) {
    char* errMsg = nullptr;
    int rc = sqlite3_exec(db, query.c_str(), nullptr, nullptr, &errMsg);

    if (rc != SQLITE_OK) {
        std::cerr << "SQL执行错误: " << (errMsg ? errMsg : "未知错误") << std::endl;
        if (errMsg) sqlite3_free(errMsg);
        return false;
    }

    return true;
}

// 以下辅助方法保留，但不再用于主要SQL操作
bool CloudPaste::checkTableExists(const std::string& tableName) {
    std::string query = "SELECT name FROM sqlite_master WHERE type='table' AND name='" + tableName + "';";

    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return false;
    }

    bool exists = false;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        exists = true;
    }

    sqlite3_finalize(stmt);
    return exists;
}

bool CloudPaste::checkColumnExists(const std::string& tableName, const std::string& columnName) {
    std::string query = "PRAGMA table_info(" + tableName + ");";

    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return false;
    }

    bool exists = false;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* colName = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        if (colName && columnName == colName) {
            exists = true;
            break;
        }
    }

    sqlite3_finalize(stmt);
    return exists;
}

bool CloudPaste::checkIndexExists(const std::string& indexName) {
    std::string query = "SELECT name FROM sqlite_master WHERE type='index' AND name='" + indexName + "';";

    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return false;
    }

    bool exists = false;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        exists = true;
    }

    sqlite3_finalize(stmt);
    return exists;
}

// 不再需要此方法，保留API兼容性
std::string CloudPaste::escapeSqlString(const std::string& str) {
    // 此方法不再用于主要的SQL操作，已被参数绑定替代
    std::string result;
    for (char c : str) {
        if (c == '\'') {
            result += "''";
        } else {
            result += c;
        }
    }
    return result;
}

int CloudPaste::addPaste(const std::string& title, const std::string& content, 
                         const std::string& author, const std::string& language, 
                         bool isPrivate, const std::string& password, int expiryDays) {
    // 获取当前时间戳
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(
                     now.time_since_epoch()).count();

    // 开始事务
    executeQuery("BEGIN TRANSACTION;");

    // 使用预处理语句和参数绑定防止SQL注入
    const char* sql = "INSERT INTO pastes (title, content, author, timestamp, language, is_private, password, expiry_days) "
                      "VALUES (?, ?, ?, ?, ?, ?, ?, ?);";
    
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "SQL prepare failed: " << sqlite3_errmsg(db) << std::endl;
        executeQuery("ROLLBACK;");
        return -1;
    }

    // 绑定参数
    sqlite3_bind_text(stmt, 1, title.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, content.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, author.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 4, timestamp);
    sqlite3_bind_text(stmt, 5, language.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 6, isPrivate ? 1 : 0);
    sqlite3_bind_text(stmt, 7, password.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 8, expiryDays);

    bool success = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);

    if (!success) {
        std::cerr << "SQL execution failed: " << sqlite3_errmsg(db) << std::endl;
        executeQuery("ROLLBACK;");
        return -1;
    }

    // 获取新插入的ID
    int newId = sqlite3_last_insert_rowid(db);
    
    // 提交事务
    executeQuery("COMMIT;");
    
    return newId;
}

std::shared_ptr<PasteContent> CloudPaste::getPasteById(int id) {
    const char* sql = "SELECT id, title, content, author, timestamp, language, is_private, password, expiry_days "
                      "FROM pastes WHERE id = ?;";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return nullptr;
    }

    sqlite3_bind_int(stmt, 1, id);

    std::shared_ptr<PasteContent> paste = nullptr;
    
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        paste = std::make_shared<PasteContent>();
        paste->id = sqlite3_column_int(stmt, 0);
        
        const char* title = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        paste->title = title ? title : "";
        
        const char* content = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        paste->content = content ? content : "";
        
        const char* author = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        paste->author = author ? author : "";
        
        paste->timestamp = sqlite3_column_int64(stmt, 4);
        
        const char* language = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
        paste->language = language ? language : "";
        
        paste->isPrivate = sqlite3_column_int(stmt, 6) != 0;
        
        const char* password = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 7));
        paste->password = password ? password : "";
        
        paste->expiryDays = sqlite3_column_int(stmt, 8);
    }

    sqlite3_finalize(stmt);
    return paste;
}

std::vector<std::shared_ptr<PasteContent>> CloudPaste::getPublicPastes(int limit, int offset) {
    std::vector<std::shared_ptr<PasteContent>> pastes;
    
    // 检查参数有效性
    if (limit <= 0) limit = 20;
    if (limit > 100) limit = 100;  // 限制最大查���数量
    if (offset < 0) offset = 0;

    const char* sql = "SELECT id, title, author, timestamp, language, expiry_days "
                      "FROM pastes WHERE is_private = 0 "
                      "ORDER BY timestamp DESC "
                      "LIMIT ? OFFSET ?;";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return pastes;
    }

    sqlite3_bind_int(stmt, 1, limit);
    sqlite3_bind_int(stmt, 2, offset);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        auto paste = std::make_shared<PasteContent>();
        paste->id = sqlite3_column_int(stmt, 0);
        
        const char* title = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        paste->title = title ? title : "";
        
        const char* author = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        paste->author = author ? author : "";
        
        paste->timestamp = sqlite3_column_int64(stmt, 3);
        
        const char* language = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        paste->language = language ? language : "";
        
        paste->expiryDays = sqlite3_column_int(stmt, 5);
        
        // 注意：不包含内容，以节省内存
        paste->content = "";
        paste->isPrivate = false;
        paste->password = "";
        
        pastes.push_back(paste);
    }

    sqlite3_finalize(stmt);
    return pastes;
}

std::vector<std::shared_ptr<PasteContent>> CloudPaste::getUserPastes(const std::string& author, int limit, int offset) {
    std::vector<std::shared_ptr<PasteContent>> pastes;
    
    // 检查参数有效性
    if (limit <= 0) limit = 20;
    if (limit > 100) limit = 100;  // 限制最大查询数量
    if (offset < 0) offset = 0;

    const char* sql = "SELECT id, title, timestamp, language, is_private, expiry_days "
                      "FROM pastes WHERE author = ? "
                      "ORDER BY timestamp DESC "
                      "LIMIT ? OFFSET ?;";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return pastes;
    }

    sqlite3_bind_text(stmt, 1, author.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, limit);
    sqlite3_bind_int(stmt, 3, offset);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        auto paste = std::make_shared<PasteContent>();
        paste->id = sqlite3_column_int(stmt, 0);
        
        const char* title = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        paste->title = title ? title : "";
        
        paste->author = author;
        paste->timestamp = sqlite3_column_int64(stmt, 2);
        
        const char* language = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        paste->language = language ? language : "";
        
        paste->isPrivate = sqlite3_column_int(stmt, 4) != 0;
        paste->expiryDays = sqlite3_column_int(stmt, 5);
        
        // 注意：不包含内容和密码，以节省内存
        paste->content = "";
        paste->password = "";
        
        pastes.push_back(paste);
    }

    sqlite3_finalize(stmt);
    return pastes;
}

bool CloudPaste::deletePaste(int id, const std::string& author) {
    // 首先检查剪贴板是否存在并且属于该用户
    const char* checkSql = "SELECT COUNT(*) FROM pastes WHERE id = ? AND author = ?;";
    
    sqlite3_stmt* checkStmt;
    if (sqlite3_prepare_v2(db, checkSql, -1, &checkStmt, nullptr) != SQLITE_OK) {
        return false;
    }
    
    sqlite3_bind_int(checkStmt, 1, id);
    sqlite3_bind_text(checkStmt, 2, author.c_str(), -1, SQLITE_TRANSIENT);
    
    bool canDelete = false;
    if (sqlite3_step(checkStmt) == SQLITE_ROW) {
        canDelete = sqlite3_column_int(checkStmt, 0) > 0;
    }
    
    sqlite3_finalize(checkStmt);
    
    if (!canDelete) {
        return false;
    }
    
    // 执行删除
    const char* deleteSql = "DELETE FROM pastes WHERE id = ?;";
    
    sqlite3_stmt* deleteStmt;
    if (sqlite3_prepare_v2(db, deleteSql, -1, &deleteStmt, nullptr) != SQLITE_OK) {
        return false;
    }
    
    sqlite3_bind_int(deleteStmt, 1, id);
    
    bool success = (sqlite3_step(deleteStmt) == SQLITE_DONE);
    sqlite3_finalize(deleteStmt);
    
    return success;
}

bool CloudPaste::updatePaste(int id, const std::string& title, const std::string& content, 
                           const std::string& language, bool isPrivate, 
                           const std::string& password, int expiryDays) {
    const char* sql = "UPDATE pastes SET "
                    "title = ?, "
                    "content = ?, "
                    "language = ?, "
                    "is_private = ?, "
                    "password = ?, "
                    "expiry_days = ? "
                    "WHERE id = ?;";
    
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }
    
    sqlite3_bind_text(stmt, 1, title.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, content.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, language.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 4, isPrivate ? 1 : 0);
    sqlite3_bind_text(stmt, 5, password.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 6, expiryDays);
    sqlite3_bind_int(stmt, 7, id);
    
    bool success = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    
    return success;
}

bool CloudPaste::pasteExists(int id) {
    const char* sql = "SELECT COUNT(*) FROM pastes WHERE id = ?;";
    
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }
    
    sqlite3_bind_int(stmt, 1, id);
    
    bool exists = false;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        exists = sqlite3_column_int(stmt, 0) > 0;
    }
    
    sqlite3_finalize(stmt);
    return exists;
}

void CloudPaste::cleanupExpiredPastes() {
    // 获取当前时间戳
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(
                     now.time_since_epoch()).count();
    
    // 计算过期时间的秒数（天数 * 24小时 * 60分钟 * 60秒）
    const char* sql = "DELETE FROM pastes WHERE "
                    "expiry_days > 0 AND " // 只删除有过期天数的
                    "timestamp + (expiry_days * 86400) < ?;";
    
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return;
    }
    
    sqlite3_bind_int64(stmt, 1, timestamp);
    
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

Json::Value CloudPaste::pasteToJson(const PasteContent& paste, bool includeContent) {
    Json::Value json;
    json["id"] = paste.id;
    json["title"] = paste.title;
    json["author"] = paste.author;
    json["timestamp"] = static_cast<Json::Int64>(paste.timestamp);
    json["language"] = paste.language;
    json["isPrivate"] = paste.isPrivate;
    json["expiryDays"] = paste.expiryDays;
    
    // 创建可读的时间格式
    char timeStr[100];
    time_t time = paste.timestamp;
    struct tm* timeinfo = localtime(&time);
    strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", timeinfo);
    json["formattedTime"] = timeStr;
    
    // 只有在需要时才包含内容，节省带宽
    if (includeContent) {
        json["content"] = paste.content;
    }
    
    // 不要将密码包含在JSON响应中
    return json;
}

// 从请求中获取用户ID
int CloudPaste::getUserIdFromRequest(const httplib::Request& req) {
    std::string cookies = req.get_header_value("Cookie");
    std::string clientid, uid;
    
    // 使用datamanage中的Cookie解析功能
    manager::transCookie(clientid, uid, cookies);
    
    int userId = -1;
    if (!uid.empty()) {
        try {
            userId = std::stoi(uid);
        } catch (const std::exception& e) {
            std::cerr << "无法解析用户ID: " << e.what() << std::endl;
        }
    }
    
    return userId;
}

// 从用户名获取用户ID
int CloudPaste::getUserIdFromUsername(const std::string& username) {
    // 使用datamanage查找用户
    manager::user* user = manager::FindUser(username);
    if (user != nullptr) {
        return user->getuid();
    }
    return -1;
}

// 查询剪贴板接口
void CloudPaste::handleQueryPaste(const httplib::Request& req, httplib::Response& res) {
    // 设置CORS头
    res.set_header("Access-Control-Allow-Origin", "*");
    res.set_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    res.set_header("Access-Control-Allow-Headers", "Content-Type");
    res.set_header("Content-Type", "application/json");
    
    // 清理过期的剪贴板
    cleanupExpiredPastes();
    
    // 处理参数
    int pasteId = -1;
    if (req.has_param("id")) {
        try {
            pasteId = std::stoi(req.get_param_value("id"));
        } catch (const std::exception& e) {
            Json::Value errorResponse;
            errorResponse["success"] = false;
            errorResponse["message"] = "无效的剪贴板ID";
            res.status = 400;
            res.set_content(errorResponse.toStyledString(), "application/json");
            return;
        }
    }
    
    // 根据不同的查询类型处理
    if (pasteId > 0) {
        // 根据ID查询单个剪贴板
        auto paste = getPasteById(pasteId);
        if (!paste) {
            Json::Value errorResponse;
            errorResponse["success"] = false;
            errorResponse["message"] = "剪贴板不存在或已过期";
            res.status = 404;
            res.set_content(errorResponse.toStyledString(), "application/json");
            return;
        }
        
        // 检查私有剪贴板的访问权限 - 修复逻辑错误
        if (paste->isPrivate) {
            int userId = getUserIdFromRequest(req);
            manager::user* currentUser = manager::FindUser(userId);
            bool isOwner = currentUser && (currentUser->getname() == paste->author);
            bool isAdmin = currentUser && (currentUser->getlabel() == manager::GMlabel);
            
            std::string password;
            if (req.has_param("password")) {
                password = req.get_param_value("password");
            }
            bool passwordMatch = !paste->password.empty() && paste->password == password;
            
            // 只有所有者、管理员或提供了正确密码的用户才能访问
            if (!isOwner && !isAdmin && !passwordMatch) {
                Json::Value errorResponse;
                errorResponse["success"] = false;
                errorResponse["message"] = "无权访问该私有剪贴板";
                res.status = 403;
                res.set_content(errorResponse.toStyledString(), "application/json");
                return;
            }
        }
        
        // 返回剪贴板内容
        Json::Value response;
        response["success"] = true;
        response["data"] = pasteToJson(*paste);
        res.set_content(response.toStyledString(), "application/json");
    } else if (req.has_param("username")) {
        // 根据用户名查询剪贴板列表
        std::string username = req.get_param_value("username");
        int userId = getUserIdFromUsername(username);
        
        if (userId == -1) {
            Json::Value errorResponse;
            errorResponse["success"] = false;
            errorResponse["message"] = "用户不存在";
            res.status = 404;
            res.set_content(errorResponse.toStyledString(), "application/json");
            return;
        }
        
        // 验证当前用户是否有权查看该用户的剪贴板列表
        int currentUserId = getUserIdFromRequest(req);
        if (currentUserId == -1 || (currentUserId != userId && manager::FindUser(currentUserId)->getlabel() != manager::GMlabel)) {
            Json::Value errorResponse;
            errorResponse["success"] = false;
            errorResponse["message"] = "无权查看该用户的剪贴板列表";
            res.status = 403;
            res.set_content(errorResponse.toStyledString(), "application/json");
            return;
        }
        
        // 处理分页参数
        int limit = 20;
        int offset = 0;
        
        if (req.has_param("limit")) {
            try {
                limit = std::stoi(req.get_param_value("limit"));
                if (limit <= 0) limit = 20;
                if (limit > 100) limit = 100;
            } catch (const std::exception&) {}
        }
        
        if (req.has_param("offset")) {
            try {
                offset = std::stoi(req.get_param_value("offset"));
                if (offset < 0) offset = 0;
            } catch (const std::exception&) {}
        }
        
        // 查询用户的剪��板列表
        auto pastes = getUserPastes(username, limit, offset);
        
        Json::Value response;
        response["success"] = true;
        response["data"] = Json::Value(Json::arrayValue);
        
        for (const auto& paste : pastes) {
            response["data"].append(pasteToJson(*paste, false));  // 不包含内容
        }
        
        res.set_content(response.toStyledString(), "application/json");
    } else {
        // 查询公开剪贴板列表
        int limit = 20;
        int offset = 0;
        
        if (req.has_param("limit")) {
            try {
                limit = std::stoi(req.get_param_value("limit"));
                if (limit <= 0) limit = 20;
                if (limit > 100) limit = 100;
            } catch (const std::exception&) {}
        }
        
        if (req.has_param("offset")) {
            try {
                offset = std::stoi(req.get_param_value("offset"));
                if (offset < 0) offset = 0;
            } catch (const std::exception&) {}
        }
        
        auto pastes = getPublicPastes(limit, offset);
        
        Json::Value response;
        response["success"] = true;
        response["data"] = Json::Value(Json::arrayValue);
        
        for (const auto& paste : pastes) {
            response["data"].append(pasteToJson(*paste, false));  // 不包含内容
        }
        
        res.set_content(response.toStyledString(), "application/json");
    }
}

// 修改剪贴板接口
void CloudPaste::handleUpdatePaste(const httplib::Request& req, httplib::Response& res, const Json::Value& root) {
    // 设置CORS头
    res.set_header("Access-Control-Allow-Origin", "*");
    res.set_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    res.set_header("Access-Control-Allow-Headers", "Content-Type");
    res.set_header("Content-Type", "application/json");
    
    // 验证用户身份
    int userId = getUserIdFromRequest(req);
    if (userId == -1) {
        Json::Value errorResponse;
        errorResponse["success"] = false;
        errorResponse["message"] = "未登录或会话已过期";
        res.status = 401;
        res.set_content(errorResponse.toStyledString(), "application/json");
        return;
    }
    
    // 获取用户对象
    manager::user* user = manager::FindUser(userId);
    if (!user) {
        Json::Value errorResponse;
        errorResponse["success"] = false;
        errorResponse["message"] = "用户不存在";
        res.status = 404;
        res.set_content(errorResponse.toStyledString(), "application/json");
        return;
    }
    
    // 检查请求中是否包含ID字段
    if (!root.isMember("id") || root["id"].asInt() <= 0) {
        // 如果没有ID或ID无效，则创建新的剪贴板
        if (!root.isMember("title") || !root.isMember("content")) {
            Json::Value errorResponse;
            errorResponse["success"] = false;
            errorResponse["message"] = "缺少必要的字段：title, content";
            res.status = 400;
            res.set_content(errorResponse.toStyledString(), "application/json");
            return;
        }
        
        // 处理可选参数
        std::string language = root.isMember("language") ? root["language"].asString() : "";
        bool isPrivate = root.isMember("isPrivate") ? root["isPrivate"].asBool() : false;
        std::string password = root.isMember("password") ? root["password"].asString() : "";
        int expiryDays = root.isMember("expiryDays") ? root["expiryDays"].asInt() : 0;
        
        // 创建新剪贴板
        int newId = addPaste(
            root["title"].asString(),
            root["content"].asString(),
            user->getname(),
            language,
            isPrivate,
            password,
            expiryDays
        );
        
        if (newId == -1) {
            Json::Value errorResponse;
            errorResponse["success"] = false;
            errorResponse["message"] = "创建剪贴板失败";
            res.status = 500;
            res.set_content(errorResponse.toStyledString(), "application/json");
            return;
        }
        
        // 返回新创建的剪贴板信息
        auto newPaste = getPasteById(newId);
        Json::Value response;
        response["success"] = true;
        response["message"] = "剪贴板创建成功";
        response["data"] = pasteToJson(*newPaste);
        res.set_content(response.toStyledString(), "application/json");
        return;
    }
    
    // 处理更新现有剪贴板的请求
    int pasteId = root["id"].asInt();
    auto paste = getPasteById(pasteId);
    
    if (!paste) {
        Json::Value errorResponse;
        errorResponse["success"] = false;
        errorResponse["message"] = "剪贴板不存在或已过期";
        res.status = 404;
        res.set_content(errorResponse.toStyledString(), "application/json");
        return;
    }
    
    // 检查权限 - 只��所��者或管理员可以修改
    if (paste->author != user->getname() && user->getlabel() != manager::GMlabel) {
        Json::Value errorResponse;
        errorResponse["success"] = false;
        errorResponse["message"] = "无权修改此剪贴板";
        res.status = 403;
        res.set_content(errorResponse.toStyledString(), "application/json");
        return;
    }
    
    // 更新字段
    std::string title = root.isMember("title") ? root["title"].asString() : paste->title;
    std::string content = root.isMember("content") ? root["content"].asString() : paste->content;
    std::string language = root.isMember("language") ? root["language"].asString() : paste->language;
    bool isPrivate = root.isMember("isPrivate") ? root["isPrivate"].asBool() : paste->isPrivate;
    std::string password = root.isMember("password") ? root["password"].asString() : paste->password;
    int expiryDays = root.isMember("expiryDays") ? root["expiryDays"].asInt() : paste->expiryDays;
    
    // 执行更新
    bool success = updatePaste(
        pasteId,
        title,
        content,
        language,
        isPrivate,
        password,
        expiryDays
    );
    
    if (!success) {
        Json::Value errorResponse;
        errorResponse["success"] = false;
        errorResponse["message"] = "更新剪贴板失败";
        res.status = 500;
        res.set_content(errorResponse.toStyledString(), "application/json");
        return;
    }
    
    // 返回更新后的剪贴板信息
    auto updatedPaste = getPasteById(pasteId);
    Json::Value response;
    response["success"] = true;
    response["message"] = "剪贴板更新成功";
    response["data"] = pasteToJson(*updatedPaste);
    res.set_content(response.toStyledString(), "application/json");
}

// 注册API路由
void CloudPaste::registerRoutes() {
    Server& server = Server::getInstance();
    
    // 注册Web页面路由 - 修改为直接提供HTML内容而不是文件路径
    server.handleRequest("/paste", [](const httplib::Request& req, httplib::Response& res) {
        std::ifstream htmlFile("html/files/paste.html", std::ios::binary);
        if (htmlFile) {
            std::stringstream buffer;
            buffer << htmlFile.rdbuf();
            res.set_content(buffer.str(), "text/html; charset=utf-8");
        }
        else {
            res.status = 404;
            res.set_content("Paste page not found", "text/plain");
        }
    });
    
    // 注册剪贴板详情页面路由
    server.handleRequest("/paste/view", [](const httplib::Request& req, httplib::Response& res) {
        std::ifstream htmlFile("html/files/paste-page.html", std::ios::binary);
        if (htmlFile) {
            std::stringstream buffer;
            buffer << htmlFile.rdbuf();
            res.set_content(buffer.str(), "text/html; charset=utf-8");
        }
        else {
            res.status = 404;
            res.set_content("Paste detail page not found", "text/plain");
        }
    });
    /*
    // 注册JS文件路由
    server.handleRequest("/files/paste.js", [](const httplib::Request& req, httplib::Response& res) {
        std::ifstream jsFile("html/files/paste.js", std::ios::binary);
        if (jsFile) {
            std::stringstream buffer;
            buffer << jsFile.rdbuf();
            res.set_content(buffer.str(), "application/javascript; charset=utf-8");
        }
        else {
            res.status = 404;
            res.set_content("paste.js not found", "text/plain");
        }
    });
    
    // 注册CSS文件路由（highlight.js的样式）
    server.handleRequest("/files/github.min.css", [](const httplib::Request& req, httplib::Response& res) {
        std::ifstream cssFile("html/files/github.min.css", std::ios::binary);
        if (cssFile) {
            std::stringstream buffer;
            buffer << cssFile.rdbuf();
            res.set_content(buffer.str(), "text/css; charset=utf-8");
        }
        else {
            res.status = 404;
            res.set_content("github.min.css not found", "text/plain");
        }
    });
    
    // 注册highlight.js文件路由
    server.handleRequest("/files/highlight.min.js", [](const httplib::Request& req, httplib::Response& res) {
        std::ifstream jsFile("html/files/highlight.min.js", std::ios::binary);
        if (jsFile) {
            std::stringstream buffer;
            buffer << jsFile.rdbuf();
            res.set_content(buffer.str(), "application/javascript; charset=utf-8");
        }
        else {
            res.status = 404;
            res.set_content("highlight.min.js not found", "text/plain");
        }
    });
    
    // 注册chatlist-paste.js文件路由
    server.handleRequest("/files/chatlist-paste.js", [](const httplib::Request& req, httplib::Response& res) {
        std::ifstream jsFile("html/files/chatlist-paste.js", std::ios::binary);
        if (jsFile) {
            std::stringstream buffer;
            buffer << jsFile.rdbuf();
            res.set_content(buffer.str(), "application/javascript; charset=utf-8");
        }
        else {
            res.status = 404;
            res.set_content("chatlist-paste.js not found", "text/plain");
        }
    });
    */
    // 注册查询接口
    server.handleRequest("/paste/query", [this](const httplib::Request& req, httplib::Response& res) {
        handleQueryPaste(req, res);
    });
    
    // 注册更新接口
    server.handlePostRequest("/paste/update", [this](const httplib::Request& req, httplib::Response& res, const Json::Value& root) {
        handleUpdatePaste(req, res, root);
    });
    
    // 注册删除接口
    server.handlePostRequest("/paste/delete", [this](const httplib::Request& req, httplib::Response& res, const Json::Value& root) {
        // 设置CORS头
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "Content-Type");
        res.set_header("Content-Type", "application/json");
        
        // 验证请求参数
        if (!root.isMember("id") || root["id"].asInt() <= 0) {
            Json::Value errorResponse;
            errorResponse["success"] = false;
            errorResponse["message"] = "缺少必要的字段：id";
            res.status = 400;
            res.set_content(errorResponse.toStyledString(), "application/json");
            return;
        }
        
        // 验证用户身份
        int userId = getUserIdFromRequest(req);
        if (userId == -1) {
            Json::Value errorResponse;
            errorResponse["success"] = false;
            errorResponse["message"] = "未登录或会话已过期";
            res.status = 401;
            res.set_content(errorResponse.toStyledString(), "application/json");
            return;
        }
        
        // 获取用户对象
        manager::user* user = manager::FindUser(userId);
        if (!user) {
            Json::Value errorResponse;
            errorResponse["success"] = false;
            errorResponse["message"] = "用户不存在";
            res.status = 404;
            res.set_content(errorResponse.toStyledString(), "application/json");
            return;
        }
        
        // 查找剪贴板
        int pasteId = root["id"].asInt();
        auto paste = getPasteById(pasteId);
        
        if (!paste) {
            Json::Value errorResponse;
            errorResponse["success"] = false;
            errorResponse["message"] = "剪贴板不存在或已过期";
            res.status = 404;
            res.set_content(errorResponse.toStyledString(), "application/json");
            return;
        }
        
        // 检查权限 - 只有所有者或管理员可以删除
        if (paste->author != user->getname() && user->getlabel() != manager::GMlabel) {
            Json::Value errorResponse;
            errorResponse["success"] = false;
            errorResponse["message"] = "无权删除此剪贴板";
            res.status = 403;
            res.set_content(errorResponse.toStyledString(), "application/json");
            return;
        }
        
        // 执行删除操作
        bool success = deletePaste(pasteId, user->getname());
        
        if (!success) {
            Json::Value errorResponse;
            errorResponse["success"] = false;
            errorResponse["message"] = "删除剪贴板失败";
            res.status = 500;
            res.set_content(errorResponse.toStyledString(), "application/json");
            return;
        }
        
        // 返回成功响应
        Json::Value response;
        response["success"] = true;
        response["message"] = "剪贴板删除成功";
        res.set_content(response.toStyledString(), "application/json");
    });
    
    // 注册清理过期剪贴板接口 (仅管理员可用)
    server.handleRequest("/paste/cleanup", [this](const httplib::Request& req, httplib::Response& res) {
        // 设置CORS头
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "Content-Type");
        res.set_header("Content-Type", "application/json");
        
        // 验证用户身份
        int userId = getUserIdFromRequest(req);
        if (userId == -1) {
            Json::Value errorResponse;
            errorResponse["success"] = false;
            errorResponse["message"] = "未登录或会话已过期";
            res.status = 401;
            res.set_content(errorResponse.toStyledString(), "application/json");
            return;
        }
        
        // 获取用户对象
        manager::user* user = manager::FindUser(userId);
        if (!user || user->getlabel() != manager::GMlabel) {
            Json::Value errorResponse;
            errorResponse["success"] = false;
            errorResponse["message"] = "权限不足，需要管理员权限";
            res.status = 403;
            res.set_content(errorResponse.toStyledString(), "application/json");
            return;
        }
        
        // 执行清理操作
        cleanupExpiredPastes();
        
        // 返回成功响应
        Json::Value response;
        response["success"] = true;
        response["message"] = "过期���贴板清理成功";
        res.set_content(response.toStyledString(), "application/json");
    });
}