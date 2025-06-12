#include "chatDBmanager.h"
#include <iostream>

ChatDBManager* ChatDBManager::instance = nullptr;

ChatDBManager::ChatDBManager() : db(nullptr) {
    initDatabase();
}

ChatDBManager::~ChatDBManager() {
    if (db) {
        sqlite3_close(db);
    }
}

ChatDBManager& ChatDBManager::getInstance() {
    if (instance == nullptr) {
        instance = new ChatDBManager();
    }
    return *instance;
}

bool ChatDBManager::initDatabase() {
    Logger& logger = Logger::getInstance();
    int rc = sqlite3_open("database.db", &db);
    
    if (rc) {
        logger.logError("ChatDBManager", "无法打开数据库: " + std::string(sqlite3_errmsg(db)));
        return false;
    }
    
    // 创建聊天室表
    if (!checkTableExists("chatrooms")) {
        const char* sql = "CREATE TABLE chatrooms ("
                         "roomid INTEGER PRIMARY KEY, "
                         "title TEXT, "
                         "password_hash TEXT, "
                         "password TEXT, "
                         "flags INTEGER);";
        
        if (!executeQuery(sql)) {
            logger.logError("ChatDBManager", "创建聊天室表失败");
            return false;
        }
    } else {
        // 检查表结构并修复
        sqlite3_stmt* stmt;
        int rc = sqlite3_prepare_v2(db, "PRAGMA table_info(chatrooms);", -1, &stmt, nullptr);
        if (rc == SQLITE_OK) {
            bool hasRoomId = false;
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                const char* colName = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
                if (strcmp(colName, "roomid") == 0) {
                    hasRoomId = true;
                    break;
                }
            }
            sqlite3_finalize(stmt);
            
            // 如果没有roomid列，需要重建表
            if (!hasRoomId) {
                logger.logWarning("ChatDBManager", "chatrooms表缺少roomid列，重建表...");
                
                // 重命名现有表
                if (!executeQuery("ALTER TABLE chatrooms RENAME TO chatrooms_old;")) {
                    logger.logError("ChatDBManager", "无法重命名chatrooms表");
                    return false;
                }
                
                // 创建新表
                const char* sql = "CREATE TABLE chatrooms ("
                                 "roomid INTEGER PRIMARY KEY, "
                                 "title TEXT, "
                                 "password_hash TEXT, "
                                 "password TEXT, "
                                 "flags INTEGER);";
                
                if (!executeQuery(sql)) {
                    logger.logError("ChatDBManager", "创建新chatrooms表失败");
                    return false;
                }
                
                // 迁移数据
                if (!executeQuery("INSERT INTO chatrooms SELECT id as roomid, title, password_hash, password, flags FROM chatrooms_old;")) {
                    logger.logError("ChatDBManager", "迁移chatrooms数据失败");
                    return false;
                }
                
                // 删除旧表
                if (!executeQuery("DROP TABLE chatrooms_old;")) {
                    logger.logError("ChatDBManager", "删除旧chatrooms表失败");
                    return false;
                }
                
                logger.logInfo("ChatDBManager", "chatrooms表重建完成");
            }
        }
    }
    
    // 创建消息表
    if (!checkTableExists("messages")) {
        const char* sql = "CREATE TABLE messages ("
                         "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                         "room_id INTEGER, "
                         "user TEXT, "
                         "label TEXT, "
                         "message TEXT, "
                         "image_url TEXT, "
                         "timestamp INTEGER, "
                         "FOREIGN KEY(room_id) REFERENCES chatrooms(roomid));";
        
        if (!executeQuery(sql)) {
            logger.logError("ChatDBManager", "创建消息表失败");
            return false;
        }
    } else {
        // 检查表结构并修复
        sqlite3_stmt* stmt;
        int rc = sqlite3_prepare_v2(db, "PRAGMA table_info(messages);", -1, &stmt, nullptr);
        if (rc == SQLITE_OK) {
            bool hasLabel = false;
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                const char* colName = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
                if (strcmp(colName, "label") == 0) {
                    hasLabel = true;
                    break;
                }
            }
            sqlite3_finalize(stmt);
            
            // 如果没有label列，添加它
            if (!hasLabel) {
                logger.logWarning("ChatDBManager", "messages表缺少label列，添加列...");
                
                if (!executeQuery("ALTER TABLE messages ADD COLUMN label TEXT;")) {
                    logger.logError("ChatDBManager", "无法添加label列到messages表");
                    return false;
                }
                
                logger.logInfo("ChatDBManager", "messages表修复完成");
            }
        }
    }
    
    logger.logInfo("ChatDBManager", "数据库初始化完成");
    return true;
}

bool ChatDBManager::executeQuery(const std::string& query) {
    Logger& logger = Logger::getInstance();
    char* errMsg = nullptr;
    int rc = sqlite3_exec(db, query.c_str(), nullptr, nullptr, &errMsg);
    
    if (rc != SQLITE_OK) {
        std::string error = errMsg ? errMsg : "未知错误";
        logger.logError("ChatDBManager", "SQL错误: " + error);
        if (errMsg) sqlite3_free(errMsg);
        return false;
    }
    
    return true;
}

bool ChatDBManager::checkTableExists(const std::string& tableName) {
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

bool ChatDBManager::createChatRoom(int roomId, const std::string& title, const std::string& passwordHash, 
                               const std::string& password, unsigned int flags) {
    std::string query = "INSERT INTO chatrooms (roomid, title, password_hash, password, flags) VALUES (" +
                       std::to_string(roomId) + ", '" +
                       title + "', '" +
                       passwordHash + "', '" +
                       password + "', " +
                       std::to_string(flags) + ");";
                       
    return executeQuery(query);
}

bool ChatDBManager::updateChatRoom(int roomId, const std::string& title, const std::string& passwordHash, 
                               const std::string& password, unsigned int flags) {
    std::string query = "UPDATE chatrooms SET title = '" + title +
                       "', password_hash = '" + passwordHash +
                       "', password = '" + password +
                       "', flags = " + std::to_string(flags) +
                       " WHERE roomid = " + std::to_string(roomId) + ";";
                       
    return executeQuery(query);
}

bool ChatDBManager::deleteChatRoom(int roomId) {
    // 首先删除聊天室的所有消息
    std::string deleteMessages = "DELETE FROM messages WHERE room_id = " + std::to_string(roomId) + ";";
    if (!executeQuery(deleteMessages)) {
        return false;
    }
    
    // 然后删除聊天室本身
    std::string deleteRoom = "DELETE FROM chatrooms WHERE roomid = " + std::to_string(roomId) + ";";
    return executeQuery(deleteRoom);
}

bool ChatDBManager::getChatRoom(int roomId, std::string& title, std::string& passwordHash, 
                            std::string& password, unsigned int& flags) {
    std::string query = "SELECT title, password_hash, password, flags FROM chatrooms WHERE roomid = " + 
                       std::to_string(roomId) + ";";
                       
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return false;
    }
    
    bool success = false;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* titleText = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        const char* passwordHashText = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        const char* passwordText = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        
        title = titleText ? titleText : "";
        passwordHash = passwordHashText ? passwordHashText : "";
        password = passwordText ? passwordText : "";
        flags = sqlite3_column_int(stmt, 3);
        success = true;
    }
    
    sqlite3_finalize(stmt);
    return success;
}

std::vector<int> ChatDBManager::getAllChatRoomIds() {
    std::vector<int> ids;
    std::string query = "SELECT roomid FROM chatrooms;";
    
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return ids;
    }
    
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        ids.push_back(sqlite3_column_int(stmt, 0));
    }
    
    sqlite3_finalize(stmt);
    return ids;
}

bool ChatDBManager::addMessage(int roomId, const Json::Value& message) {
    std::string user = message["user"].asString();
    std::string label = message.isMember("labei") ? message["labei"].asString() : "";
    std::string msgContent = message["message"].asString();
    std::string imageUrl = message.isMember("imageUrl") ? message["imageUrl"].asString() : "";
    long long timestamp = message["timestamp"].asInt64();
    
    // 转义单引号以防止SQL注入
    user = std::string(user).find('\'') != std::string::npos ? 
           std::string(user).replace(std::string(user).find('\''), 1, "''") : user;
    label = std::string(label).find('\'') != std::string::npos ? 
            std::string(label).replace(std::string(label).find('\''), 1, "''") : label;
    msgContent = std::string(msgContent).find('\'') != std::string::npos ? 
                 std::string(msgContent).replace(std::string(msgContent).find('\''), 1, "''") : msgContent;
    imageUrl = std::string(imageUrl).find('\'') != std::string::npos ? 
               std::string(imageUrl).replace(std::string(imageUrl).find('\''), 1, "''") : imageUrl;
    
    std::string query = "INSERT INTO messages (room_id, user, label, message, image_url, timestamp) VALUES (" +
                       std::to_string(roomId) + ", '" +
                       user + "', '" +
                       label + "', '" +
                       msgContent + "', '" +
                       imageUrl + "', " +
                       std::to_string(timestamp) + ");";
                       
    return executeQuery(query);
}

bool ChatDBManager::getMessages(int roomId, std::deque<Json::Value>& messages, long long lastTimestamp) {
    std::string query = "SELECT user, label, message, image_url, timestamp FROM messages WHERE room_id = " + 
                       std::to_string(roomId);
    
    if (lastTimestamp > 0) {
        query += " AND timestamp > " + std::to_string(lastTimestamp);
    }
    
    query += " ORDER BY timestamp ASC;";
    
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return false;
    }
    
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        Json::Value msg;
        const char* userText = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        const char* labelText = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        const char* messageText = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        const char* imageUrlText = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        
        msg["user"] = userText ? userText : "";
        msg["labei"] = labelText ? labelText : "";
        msg["message"] = messageText ? messageText : "";
        
        if (imageUrlText && strlen(imageUrlText) > 0) {
            msg["imageUrl"] = imageUrlText;
        }
        
        msg["timestamp"] = static_cast<Json::Int64>(sqlite3_column_int64(stmt, 4));
        
        messages.push_back(msg);
    }
    
    sqlite3_finalize(stmt);
    return true;
}

bool ChatDBManager::clearMessages(int roomId) {
    std::string query = "DELETE FROM messages WHERE room_id = " + std::to_string(roomId) + ";";
    return executeQuery(query);
}