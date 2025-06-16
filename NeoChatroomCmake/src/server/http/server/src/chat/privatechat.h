#ifndef PRIVATECHAT_H
#define PRIVATECHAT_H

#include "chatroom.h"
#include "chatDBmanager.h"
#include "../tool/log.h"
#include <mutex>

// 私聊管理类 - 使用roommanager中room数组下标为0的位置作为私聊聊天室
class PrivateChat {
private:
    static PrivateChat* instance;  // 单例实例
    std::mutex mtx;
    bool initialized;

    // 私有构造函数，实现单例模式
    PrivateChat();

    // 禁止复制和赋值
    PrivateChat(const PrivateChat&) = delete;
    PrivateChat& operator=(const PrivateChat&) = delete;

public:
    // 获取单例实例
    static PrivateChat& getInstance();

    // 初始化私聊系统
    bool init();

    // 启动私聊系统
    bool start();

    // 发送系统消息到私聊室
    void sendSystemMessage(const std::string& message);

    // 获取用户间的私聊消息
    bool getUserMessages(const std::string& fromUser, const std::string& toUser, 
                         std::vector<Json::Value>& messages, long long lastTimestamp = 0);

    // 添加私聊消息
    bool addPrivateMessage(const std::string& fromUser, const std::string& toUser, 
                          const std::string& message, const std::string& imageUrl = "");

    // 设置私聊API路由
    void setupRoutes();

    // 析构函数
    ~PrivateChat();
};

#endif //PRIVATECHAT_H
