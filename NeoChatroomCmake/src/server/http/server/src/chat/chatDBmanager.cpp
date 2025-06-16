#include "chatDBmanager.h"
#include <iostream>

ChatDBManager* ChatDBManager::instance = nullptr;

ChatDBManager::ChatDBManager() : db(nullptr) {
    initDatabase();
}

ChatDBManager::~ChatDBManager() {
    if (db) {
        // 确保关闭数据库前执行WAL检查点
        sqlite3_exec(db, "PRAGMA wal_checkpoint(FULL);", nullptr, nullptr, nullptr);
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
    int rc = sqlite3_open("database.db", &db);
    
    if (rc) {
        return false;
    }
    
    // 设置SQLite的同步模式，确保数据写入磁盘
    executeQuery("PRAGMA synchronous = NORMAL;");
    
    // 使用WAL模式提高性能和可靠性
    executeQuery("PRAGMA journal_mode = WAL;");
    
    // 创建聊天室表
    if (!checkTableExists("chatrooms")) {
        const char* sql = "CREATE TABLE chatrooms ("
                         "roomid INTEGER PRIMARY KEY, "
                         "title TEXT, "
                         "password_hash TEXT, "
                         "password TEXT, "
                         "flags INTEGER);";
        
        if (!executeQuery(sql)) {
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
                // 重命名现有表
                if (!executeQuery("ALTER TABLE chatrooms RENAME TO chatrooms_old;")) {
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
                    return false;
                }
                
                // 迁移数据
                if (!executeQuery("INSERT INTO chatrooms SELECT id as roomid, title, password_hash, password, flags FROM chatrooms_old;")) {
                    return false;
                }
                
                // 删除旧表
                if (!executeQuery("DROP TABLE chatrooms_old;")) {
                    return false;
                }
            }
        }
    }
    
    // 创建消息表或更新现有表结构
    if (!checkTableExists("messages")) {
        // 如果消息表不存在，创建一个新表
        const char* sql = "CREATE TABLE messages ("
                         "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                         "room_id INTEGER, "
                         "user TEXT, "
                         "label TEXT, "
                         "message TEXT, "
                         "image_url TEXT, "
                         "timestamp INTEGER, "
                         "metadata TEXT, "
                         "FOREIGN KEY(room_id) REFERENCES chatrooms(roomid));";
        
        if (!executeQuery(sql)) {
            return false;
        }
    } else {
        // 检查消息表结构并添加缺失的列
        
        // 检查 label 列
        bool hasLabel = checkColumnExists("messages", "label");
        if (!hasLabel) {
            if (!executeQuery("ALTER TABLE messages ADD COLUMN label TEXT;")) {
                return false;
            }
        }
        
        // 检查 image_url 列
        bool hasImageUrl = checkColumnExists("messages", "image_url");
        if (!hasImageUrl) {
            if (!executeQuery("ALTER TABLE messages ADD COLUMN image_url TEXT;")) {
                return false;
            }
        }
        
        // 检查 metadata 列
        bool hasMetadata = checkColumnExists("messages", "metadata");
        if (!hasMetadata) {
            if (!executeQuery("ALTER TABLE messages ADD COLUMN metadata TEXT;")) {
                return false;
            }
        }
    }
    
    return true;
}

bool ChatDBManager::checkColumnExists(const std::string& tableName, const std::string& columnName) {
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

bool ChatDBManager::executeQuery(const std::string& query) {
    char* errMsg = nullptr;
    int rc = sqlite3_exec(db, query.c_str(), nullptr, nullptr, &errMsg);
    
    if (rc != SQLITE_OK) {
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
    // 开始事务
    if (!executeQuery("BEGIN TRANSACTION;")) {
        return false;
    }
    
    std::string user = message["user"].asString();
    std::string label = message.isMember("label") ? message["label"].asString() : "";
    std::string msgContent = message["message"].asString();
    std::string imageUrl = message.isMember("imageUrl") ? message["imageUrl"].asString() : "";
    long long timestamp = message["timestamp"].asInt64();
    
    // 创建metadata字段的JSON字符串，用于保存私聊信息
    std::string metadataStr = "";
    if (message.isMember("metadata")) {
        Json::FastWriter writer;
        metadataStr = writer.write(message["metadata"]);
        // 移除尾部的换行符
        if (!metadataStr.empty() && metadataStr[metadataStr.length()-1] == '\n') {
            metadataStr = metadataStr.substr(0, metadataStr.length()-1);
        }
    }
    
    // 转义所有的单引号以防止SQL注入
    auto escapeQuotes = [](std::string str) {
        std::string result;
        for (char c : str) {
            if (c == '\'') {
                result += "''";
            } else {
                result += c;
            }
        }
        return result;
    };
    
    user = escapeQuotes(user);
    label = escapeQuotes(label);
    msgContent = escapeQuotes(msgContent);
    imageUrl = escapeQuotes(imageUrl);
    metadataStr = escapeQuotes(metadataStr);
    
    // 插入消息
    std::string query = "INSERT INTO messages (room_id, user, label, message, image_url, timestamp, metadata) VALUES (" +
            std::to_string(roomId) + ", '" +
            user + "', '" +
            label + "', '" +
            msgContent + "', '" +
            imageUrl + "', " +
            std::to_string(timestamp) + ", '" +
            metadataStr + "');";
    
    bool success = executeQuery(query);
    
    // 提交或回滚事务
    if (success) {
        if (!executeQuery("COMMIT;")) {
            executeQuery("ROLLBACK;");
            return false;
        }
        // 执行WAL检查点，确保数据写入主数据库文件
        executeQuery("PRAGMA wal_checkpoint;");
    } else {
        executeQuery("ROLLBACK;");
    }
    
    return success;
}

bool ChatDBManager::getMessages(int roomId, std::deque<Json::Value>& messages, long long lastTimestamp) {
    std::string query = "SELECT user, label, message, image_url, timestamp, metadata FROM messages WHERE room_id = " + 
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
        const char* metadataText = nullptr;
        
        if (sqlite3_column_type(stmt, 5) != SQLITE_NULL) {
            metadataText = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
        }
        
        msg["user"] = userText ? userText : "";
        msg["label"] = labelText ? labelText : "";
        msg["message"] = messageText ? messageText : "";
        
        if (imageUrlText && strlen(imageUrlText) > 0) {
            msg["imageUrl"] = imageUrlText;
        }
        
        msg["timestamp"] = static_cast<Json::Int64>(sqlite3_column_int64(stmt, 4));
        
        // 处理metadata
        if (metadataText && strlen(metadataText) > 0) {
            Json::Value metadata;
            Json::Reader reader;
            bool parseSuccess = false;
            
            // 尝试直接解析
            parseSuccess = reader.parse(metadataText, metadata);
            
            if (parseSuccess) {
                msg["metadata"] = metadata;
            } else {
                // 如果解析失败，尝试修复常见的JSON格式问题
                std::string fixedMetadata = std::string(metadataText);
                
                // 修复双引号问题
                std::string::size_type pos = 0;
                while ((pos = fixedMetadata.find("'", pos)) != std::string::npos) {
                    fixedMetadata.replace(pos, 1, "\"");
                    pos += 1;
                }
                
                // 确保是有效的JSON对象
                if (fixedMetadata.front() != '{') fixedMetadata = '{' + fixedMetadata;
                if (fixedMetadata.back() != '}') fixedMetadata += '}';
                
                if (reader.parse(fixedMetadata, metadata)) {
                    msg["metadata"] = metadata;
                } else {
                    // 极端情况：如果仍然无法解析，手动创建metadata对象
                    // 查找to字段的值
                    std::string toValue;
                    std::string::size_type toPos = std::string(metadataText).find("\"to\":");
                    if (toPos != std::string::npos) {
                        toPos += 5; // 跳过 "to":
                        std::string::size_type valueStart = std::string(metadataText).find("\"", toPos);
                        if (valueStart != std::string::npos) {
                            valueStart += 1; // 跳过开始的引号
                            std::string::size_type valueEnd = std::string(metadataText).find("\"", valueStart);
                            if (valueEnd != std::string::npos) {
                                toValue = std::string(metadataText).substr(valueStart, valueEnd - valueStart);
                                
                                // 手动创建metadata对象
                                Json::Value manualMetadata;
                                manualMetadata["to"] = toValue;
                                msg["metadata"] = manualMetadata;
                            }
                        }
                    }
                    
                    if (toValue.empty()) {
                        // 如果无法手动解析，存储原始字符串
                        msg["metadata"] = metadataText;
                    }
                }
            }
        }
        
        messages.push_back(msg);
    }
    
    sqlite3_finalize(stmt);
    return true;
}

bool ChatDBManager::clearMessages(int roomId) {
    // 在事务中执行删除操作
    if (!executeQuery("BEGIN TRANSACTION;")) {
        return false;
    }
    
    std::string query = "DELETE FROM messages WHERE room_id = " + std::to_string(roomId) + ";";
    bool success = executeQuery(query);
    
    // 提交或回滚事务
    if (success) {
        return executeQuery("COMMIT;");
    } else {
        executeQuery("ROLLBACK;");
        return false;
    }
}
