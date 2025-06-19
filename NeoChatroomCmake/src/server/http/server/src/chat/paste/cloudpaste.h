//
// Created by seve on 25-6-19.
//

#ifndef CLOUDPASTE_H
#define CLOUDPASTE_H

#include <string>
#include <vector>
#include <sqlite3.h>
#include <json/json.h>
#include <memory>
#include "../../../../../lib/httplib.h"  // 添加httplib

// 剪贴板内容的结构
struct PasteContent {
    int id;
    std::string title;
    std::string content;
    std::string author;
    long long timestamp;
    std::string language;  // 用于代码高亮
    bool isPrivate;        // 是否私有
    std::string password;  // 私有剪贴板的密码
    int expiryDays;        // 过期天数，0表示永不过期
};

class CloudPaste {
private:
    static CloudPaste* instance;
    sqlite3* db;

    // 私��构造函数，确保单例模式
    CloudPaste();
    
    // 禁止复制和赋值操作，确保单例模式
    CloudPaste(const CloudPaste&) = delete;
    CloudPaste& operator=(const CloudPaste&) = delete;

    // 初始化数据库
    bool initDatabase();
    
    // 辅助函数
    bool executeQuery(const std::string& query);
    bool checkTableExists(const std::string& tableName);
    bool checkColumnExists(const std::string& tableName, const std::string& columnName);
    bool checkIndexExists(const std::string& indexName);
    std::string escapeSqlString(const std::string& str);

public:
    // 析构函数
    ~CloudPaste();

    // 获取单例实例
    static CloudPaste& getInstance();
    
    // 释放单例实例（用于程序退出前的清理）
    static void releaseInstance();

    // 添加新的剪贴板内容
    int addPaste(const std::string& title, const std::string& content, 
                 const std::string& author, const std::string& language = "", 
                 bool isPrivate = false, const std::string& password = "", 
                 int expiryDays = 0);

    // 根据ID获取剪贴板内容
    std::shared_ptr<PasteContent> getPasteById(int id);

    // 获取公开的剪贴板列表
    std::vector<std::shared_ptr<PasteContent>> getPublicPastes(int limit = 20, int offset = 0);

    // 获取用户的剪贴板列表
    std::vector<std::shared_ptr<PasteContent>> getUserPastes(const std::string& author, int limit = 20, int offset = 0);

    // 删除剪贴板内容
    bool deletePaste(int id, const std::string& author);

    // 更新剪贴板内容
    bool updatePaste(int id, const std::string& title, const std::string& content, 
                     const std::string& language = "", bool isPrivate = false, 
                     const std::string& password = "", int expiryDays = 0);

    // 检查剪贴板是否存在
    bool pasteExists(int id);

    // 清理过期的剪贴板
    void cleanupExpiredPastes();

    // 将剪贴板内容转换为JSON
    Json::Value pasteToJson(const PasteContent& paste, bool includeContent = true);

    // 添加API处理函数
    void handleQueryPaste(const httplib::Request& req, httplib::Response& res);
    void handleUpdatePaste(const httplib::Request& req, httplib::Response& res, const Json::Value& root);
    
    // 用于从Cookie获取用户ID
    int getUserIdFromRequest(const httplib::Request& req);
    
    // 从用户名获取用户ID
    int getUserIdFromUsername(const std::string& username);
    
    // 注册API路由
    void registerRoutes();
};

#endif //CLOUDPASTE_H