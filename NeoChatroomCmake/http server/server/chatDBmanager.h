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
    sqlite3* db;                     // SQLite数据库连接句柄
    static ChatDBManager* instance;  // 单例模式实例指针
    
    // 私有构造函数和析构函数，实现单例模式
    ChatDBManager();
    ~ChatDBManager();

    // 初始化数据库，创建必要的表和索引
    bool initDatabase();
    
    // 辅助函数
    bool executeQuery(const std::string& query);     // 执行SQL查询
    bool checkTableExists(const std::string& tableName);  // 检查表是否存在
    bool checkColumnExists(const std::string& tableName, const std::string& columnName);  // 检查列是否存在
    void logTableInfo(const std::string& tableName); // 记录表结构信息

public:
    // 获取单例实例
    static ChatDBManager& getInstance();
    
    // 聊天室基本操作
    bool createChatRoom(int roomId, const std::string& title, const std::string& passwordHash, 
                       const std::string& password, unsigned int flags);  // 创建新聊天室
    bool updateChatRoom(int roomId, const std::string& title, const std::string& passwordHash, 
                       const std::string& password, unsigned int flags);  // 更新聊天室信息
    bool deleteChatRoom(int roomId);  // 删除聊天室及其所有消息
    bool getChatRoom(int roomId, std::string& title, std::string& passwordHash, 
                    std::string& password, unsigned int& flags);  // 获取聊天室信息
    std::vector<int> getAllChatRoomIds();  // 获取所有聊天室ID列表

    // 聊天消息操作
    bool addMessage(int roomId, const Json::Value& message);  // 添加新消息
    bool getMessages(int roomId, std::deque<Json::Value>& messages, long long lastTimestamp = 0);  // 获取消息历史
    bool clearMessages(int roomId);  // 清空指定聊天室的所有消息
};

#endif // CHATDBMANAGER_H