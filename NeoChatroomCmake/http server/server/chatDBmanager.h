#ifndef CHATDBMANAGER_H
#define CHATDBMANAGER_H

#include <string>
#include <vector>
#include <sqlite3.h>
#include <memory>
#include "../../json/json.h"
#include "../../include/log.h"
#include <deque>

class ChatDBManager {
private:
    sqlite3* db;
    static ChatDBManager* instance;
    
    ChatDBManager();
    ~ChatDBManager();

    // 数据库初始化函数
    bool initDatabase();
    
    // 帮助函数
    bool executeQuery(const std::string& query);
    bool checkTableExists(const std::string& tableName);
    void logTableInfo(const std::string& tableName);

public:
    static ChatDBManager& getInstance();
    
    // 聊天室操作
    bool createChatRoom(int roomId, const std::string& title, const std::string& passwordHash, 
                       const std::string& password, unsigned int flags);
    bool updateChatRoom(int roomId, const std::string& title, const std::string& passwordHash, 
                       const std::string& password, unsigned int flags);
    bool deleteChatRoom(int roomId);
    bool getChatRoom(int roomId, std::string& title, std::string& passwordHash, 
                    std::string& password, unsigned int& flags);
    std::vector<int> getAllChatRoomIds();

    // 聊天消息操作
    bool addMessage(int roomId, const Json::Value& message);
    bool getMessages(int roomId, std::deque<Json::Value>& messages, long long lastTimestamp = 0);
    bool clearMessages(int roomId);
};

#endif // CHATDBMANAGER_H
