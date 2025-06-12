//
// Created by seve on 25-6-4.
//

#include "serverconfig.h"
#include "../include/Server.h"
#include "../include/log.h"
#include "chatroom.h"
#include "roommanager.h"
#include "../json/json.h"
#include <string>
#include <fstream>
#include <iostream>

// 默认服务器配置
std::string CURRENT_HOST = "0.0.0.0";
int CURRENT_PORT = 443;

// 管理密码全局变量
std::string ControlLoginPassword = "123456";

// 配置文件路径
const std::string CONFIG_FILE = "./config.json";

static void createDefaultConfig() {
    Logger& logger = Logger::getInstance();
    try {
        Json::Value root;
        
        // 默认服务器配置：HOST 和 PORT
        root["server"]["host"] = CURRENT_HOST;
        root["server"]["port"] = CURRENT_PORT;

        // 默认管理密码
        root["admin_password"] = ControlLoginPassword;

        // 使用 StreamWriterBuilder 来生成合法的 JSON 字符串
        Json::StreamWriterBuilder writerBuilder;
        writerBuilder["indentation"] = "";  // 不要缩进，输出紧凑格式
        std::string outputJson = Json::writeString(writerBuilder, root);

        std::ofstream ofs(CONFIG_FILE, std::ofstream::out | std::ofstream::trunc);
        if (!ofs.is_open()) {
            logger.logError("Config", "无法创建默认配置文件: " + CONFIG_FILE);
            return;
        }

        ofs << outputJson;
        ofs.close();
        logger.logInfo("Config", "已生成默认配置文件: " + CONFIG_FILE);
    }
    catch (const std::exception& e) {
        Logger::getInstance().logFatal("Config", "生成默认配置时发生异常: " + std::string(e.what()));
    }
    catch (...) {
        Logger::getInstance().logFatal("Config", "生成默认配置时发生未知异常");
    }
}

void loadConfig() {
    Logger& logger = Logger::getInstance();
    try {
        std::ifstream ifs(CONFIG_FILE);
        if (!ifs.is_open()) {
            logger.logWarning("Config", "配置文件不存在，生成默认配置: " + CONFIG_FILE);
            createDefaultConfig();
            return;
        }

        Json::Value root;
        try {
            ifs >> root;
        }
        catch (const std::exception& e) {
            logger.logError("Config", "解析配置文件失败: " + std::string(e.what()));
            ifs.close();
            return;
        }
        catch (...) {
            logger.logError("Config", "解析配置文件时发生未知错误");
            ifs.close();
            return;
        }
        ifs.close();

        // 加载服务器配置
        if (!root["server"].isNull()) {
            try {
                if (root["server"].isMember("host"))
                    CURRENT_HOST = root["server"]["host"].asString();
                if (root["server"].isMember("port"))
                    CURRENT_PORT = root["server"]["port"].asInt();
            }
            catch (const std::exception& e) {
                logger.logError("Config", "读取服务器配置失败: " + std::string(e.what()));
            }
            catch (...) {
                logger.logError("Config", "读取服务器配置时发生未知错误");
            }

            try {
                Server& server = Server::getInstance();
                server.setHOST(CURRENT_HOST);
                server.setPORT(CURRENT_PORT);
            }
            catch (const std::exception& e) {
                logger.logError("Config", "设置服务器HOST/PORT失败: " + std::string(e.what()));
            }
            catch (...) {
                logger.logError("Config", "设置服务器HOST/PORT时发生未知错误");
            }
        }

        // 加载管理密码
        if (root.isMember("admin_password")) {
            try {
                ControlLoginPassword = root["admin_password"].asString();
            }
            catch (const std::exception& e) {
                logger.logError("Config", "读取管理密码失败: " + std::string(e.what()));
            }
            catch (...) {
                logger.logError("Config", "读取管理密码时发生未知错误");
            }
        }
    }
    catch (const std::exception& e) {
        logger.logFatal("Config", "加载配置时发生异常: " + std::string(e.what()));
    }
    catch (...) {
        logger.logFatal("Config", "加载配置时发生未知异常");
    }
}
