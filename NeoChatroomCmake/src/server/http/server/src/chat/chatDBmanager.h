#ifndef CHATDBMANAGER_H
#define CHATDBMANAGER_H

#include <string>
#include <vector>
#include <sqlite3.h>
#include <memory>
#include "json/json.h"
#include "../tool/log.h"
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

    // 新增方法：检查索引是否存在
    bool checkIndexExists(const std::string& indexName);
    
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

    // 检查两个用户之间是否有未读消息
    bool hasUnreadMessages(int roomId, const std::string& fromUser, const std::string& toUser, long long lastTimestamp = 0);
    
    // 检查用户是否有未读消息
    bool userHasUnreadMessages(int roomId, const std::string& toUser, long long lastTimestamp = 0);
    
    // 将消息标记为已读
    bool markMessagesAsRead(int roomId, const std::string& fromUser, const std::string& toUser);
    
    // 获取与指定用户相关的所有消息（发送或接收）
    bool getUserRelatedMessages(int roomId, const std::string& username, std::deque<Json::Value>& messages, long long lastTimestamp = 0);
    
    // 获取两个特定用户之间的私聊消息
    bool getPrivateMessagesBetweenUsers(int roomId, const std::string& userA, const std::string& userB, 
                                      std::deque<Json::Value>& messages, long long lastTimestamp = 0);

    // 获取用户的最近聊天列表
    bool getUserRecentChats(int roomId, const std::string& username, std::vector<std::string>& chatUsers, int limit = 20);
    
    // 获取用户未读消息计数的方法
    int getUserUnreadCount(int roomId, const std::string& username);
    
    // 批量标记消息为已读
    bool batchMarkMessagesAsRead(int roomId, const std::string& fromUser, const std::string& toUser);
    
    // 获取分页的消息历史
    bool getPagedMessages(int roomId, const std::string& userA, const std::string& userB, 
                          std::deque<Json::Value>& messages, long long lastTimestamp = 0, 
                          int page = 0, int pageSize = 50);
};

#endif // CHATDBMANAGER_H