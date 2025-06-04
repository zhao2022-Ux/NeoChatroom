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
        // 默认没有任何聊天室
        Json::Value rooms(Json::arrayValue);
        root["rooms"] = rooms;

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

void saveConfig() {
    Logger& logger = Logger::getInstance();
    try {
        Json::Value root;
        Json::Value rooms(Json::arrayValue);

        // 遍历所有已使用的聊天室，将信息写入配置文件
        for (int i = 0; i < MAXROOM; i++) {
            if (used[i]) {
                try {
                    Json::Value roomObj;
                    roomObj["id"] = i;
                    roomObj["name"] = room[i].gettittle();
                    roomObj["password"] = room[i].GetPassword();

                    // 序列化聊天室的 flags
                    unsigned int flags = 0;
                    if (room[i].hasFlag(chatroom::ROOM_HIDDEN)) {
                        flags |= chatroom::ROOM_HIDDEN;
                    }
                    if (room[i].hasFlag(chatroom::ROOM_NO_JOIN)) {
                        flags |= chatroom::ROOM_NO_JOIN;
                    }
                    roomObj["flags"] = flags;

                    rooms.append(roomObj);
                }
                catch (const std::exception& e) {
                    logger.logError("Config", "保存聊天室信息失败，ID: " + std::to_string(i) + "，错误: " + e.what());
                }
                catch (...) {
                    logger.logError("Config", "保存聊天室信息时发生未知错误，ID: " + std::to_string(i));
                }
            }
        }
        root["rooms"] = rooms;

        // 保存服务器配置：HOST 和 PORT
        root["server"]["host"] = CURRENT_HOST;
        root["server"]["port"] = CURRENT_PORT;

        // 保存管理密码
        root["admin_password"] = ControlLoginPassword;

        // 生成字符串
        Json::StreamWriterBuilder writerBuilder;
        writerBuilder["indentation"] = "";
        std::string outputJson = Json::writeString(writerBuilder, root);

        std::ofstream ofs(CONFIG_FILE, std::ofstream::out | std::ofstream::trunc);
        if (!ofs.is_open()) {
            logger.logError("Config", "无法打开配置文件进行写入: " + CONFIG_FILE);
            return;
        }

        try {
            ofs << outputJson;
        }
        catch (const std::exception& e) {
            logger.logError("Config", "写入配置文件失败: " + std::string(e.what()));
        }
        catch (...) {
            logger.logError("Config", "写入配置文件时发生未知错误");
        }
        ofs.close();
    }
    catch (const std::exception& e) {
        logger.logFatal("Config", "保存配置时发生异常: " + std::string(e.what()));
    }
    catch (...) {
        logger.logFatal("Config", "保存配置时发生未知异常");
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

void loadChatroomInConfig() {
    Logger& logger = Logger::getInstance();
    try {
        std::ifstream ifs(CONFIG_FILE);
        if (!ifs.is_open()) {
            logger.logInfo("Config", "配置文件不存在，跳过聊天室加载: " + CONFIG_FILE);
            return;
        }

        Json::Value root;
        try {
            ifs >> root;
        }
        catch (const std::exception& e) {
            logger.logWarning("Config", "解析聊天室配置失败，跳过加载: " + std::string(e.what()));
            ifs.close();
            return;
        }
        catch (...) {
            logger.logWarning("Config", "解析聊天室配置时发生未知错误，跳过加载");
            ifs.close();
            return;
        }
        ifs.close();

        const Json::Value rooms = root["rooms"];
        if (!rooms.isArray()) {
            // rooms 不是数组也当成空处理
            return;
        }

        for (const auto& roomObj : rooms) {
            try {
                int id = roomObj["id"].asInt();
                std::string name = roomObj["name"].asString();
                if (id < 0 || id >= MAXROOM) {
                    logger.logError("Config", "无效的聊天室 ID (配置文件): " + std::to_string(id));
                    continue;
                }

                if (!used[id]) {
                    int newId = -1;
                    try {
                        newId = addroom();
                    }
                    catch (const std::exception& e) {
                        logger.logError("Config", "创建聊天室失败 (ID: " + std::to_string(id) + "): " + std::string(e.what()));
                        continue;
                    }
                    catch (...) {
                        logger.logError("Config", "创建聊天室时发生未知错误 (ID: " + std::to_string(id) + ")");
                        continue;
                    }

                    while (newId < id && newId != -1) {
                        try {
                            newId = addroom();
                        }
                        catch (const std::exception& e) {
                            logger.logError("Config", "继续创建聊天室失败 (期望 ID: " + std::to_string(id) + "): " + std::string(e.what()));
                            break;
                        }
                        catch (...) {
                            logger.logError("Config", "继续创建聊天室时发生未知错误 (期望 ID: " + std::to_string(id) + ")");
                            break;
                        }
                    }
                }

                try {
                    editroom(id, name);
                }
                catch (const std::exception& e) {
                    logger.logError("Config", "编辑聊天室名称失败 (ID: " + std::to_string(id) + "): " + std::string(e.what()));
                }
                catch (...) {
                    logger.logError("Config", "编辑聊天室名称时发生未知错误 (ID: " + std::to_string(id) + ")");
                }

                // 如果配置中包含密码字段，则设置密码
                if (roomObj.isMember("password")) {
                    try {
                        std::string pwd = roomObj["password"].asString();
                        room[id].setPassword(pwd);
                    }
                    catch (const std::exception& e) {
                        logger.logError("Config", "设置聊天室密码失败 (ID: " + std::to_string(id) + "): " + std::string(e.what()));
                    }
                    catch (...) {
                        logger.logError("Config", "设置聊天室密码时发生未知错误 (ID: " + std::to_string(id) + ")");
                    }
                }

                // 如果配置中包含 flags 字段，则还原 flags
                if (roomObj.isMember("flags")) {
                    try {
                        unsigned int f = roomObj["flags"].asUInt();
                        room[id].setflag(static_cast<int>(f));
                    }
                    catch (const std::exception& e) {
                        logger.logError("Config", "设置聊天室标志位失败 (ID: " + std::to_string(id) + "): " + std::string(e.what()));
                    }
                    catch (...) {
                        logger.logError("Config", "设置聊天室标志位时发生未知错误 (ID: " + std::to_string(id) + ")");
                    }
                }

                try {
                    if (!room[id].start()) {
                        logger.logError("Control", "无法启动聊天室，ID: " + std::to_string(id));
                    }
                }
                catch (const std::exception& e) {
                    logger.logError("Control", "启动聊天室时抛出异常，ID: " + std::to_string(id) + "，错误: " + std::string(e.what()));
                }
                catch (...) {
                    logger.logError("Control", "启动聊天室时发生未知错误，ID: " + std::to_string(id));
                }
            }
            catch (const std::exception& e) {
                logger.logError("Config", "遍历聊天室配置时发生异常: " + std::string(e.what()));
            }
            catch (...) {
                logger.logError("Config", "遍历聊天室配置时发生未知错误");
            }
        }
    }
    catch (const std::exception& e) {
        Logger::getInstance().logFatal("Config", "加载聊天室配置时发生异常: " + std::string(e.what()));
    }
    catch (...) {
        Logger::getInstance().logFatal("Config", "加载聊天室配置时发生未知异常");
    }
}
