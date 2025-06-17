#ifndef PRIVATECHAT_H
#define PRIVATECHAT_H

#include "chatroom.h"
#include "chatDBmanager.h"
#include "../tool/log.h"
#include <mutex>
#include <unordered_map>
#include <deque>

// 私聊管理类 - 使用roommanager中room数组下标为0的位置作为私聊聊天室
class PrivateChat {
private:
    static PrivateChat* instance;  // 单例实例
    std::mutex mtx;
    bool initialized;

    // 消息缓存 - 减少数据库访问
    std::unordered_map<std::string, std::deque<Json::Value>> messageCache;
    std::mutex cacheMutex;

    // 清理过期缓存
    void cleanupCache();

public:
    // 获取单例实例
    static PrivateChat& getInstance();

    // 初始化私聊系统
    bool init();

    // 启动私聊系统
    bool start();

    // 发送系统消息到私聊室
    void sendSystemMessage(const std::string& message);

    // 检查用户是否有未读消息
    bool checkUnreadMessages(const std::string& username, Json::Value& response);

    // 获取用户消息，只需toUser参数，消息发送成功后自动标记为已读
    bool getUserMessages(const std::string& currentUserName, const std::string& partner,
                            std::vector<Json::Value>& messages, long long lastTimestamp = 0,
                            int page = 0, int pageSize = 50);
    // 添加私聊消息
    bool addPrivateMessage(const std::string& fromUser, const std::string& toUser, 
                          const std::string& message, const std::string& imageUrl = "");

    // 设置私聊API路由
    void setupRoutes();

    PrivateChat();
    // 析构函数
    ~PrivateChat();
};

#endif //PRIVATECHAT_H