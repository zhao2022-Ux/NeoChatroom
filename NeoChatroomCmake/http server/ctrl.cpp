#include "../include/Server.h"
#include "../include/log.h"
#include "../include/datamanage.h"
#include "server/chatroom.h" 
#include "server/roommanager.h"
#include "../json/json.h"

#include <string>
#include <thread>
#include <fstream>
#include <iostream>
#include <locale>
#include <codecvt>
#include <stdexcept>
#include "RedirectServer.h"
using namespace std;

// 默认服务器配置
static std::string CURRENT_HOST = "0.0.0.0";
static int CURRENT_PORT = 443;

// Configuration file path
const string CONFIG_FILE = "./config.json";

// Forward declaration for login system starter
void start_loginSystem();


// -------------------------------------------------------------------
// Configuration file related functions using jsoncpp
// -------------------------------------------------------------------
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
                    // 假定聊天室有 gettittle() 方法获取当前名称
                    roomObj["name"] = room[i].gettittle();
                    // 保存聊天室密码（采用 GetPassword 获取）
                    roomObj["password"] = room[i].GetPassword();
                    rooms.append(roomObj);
                }
                catch (const std::exception& e) {
                    logger.logError("Config", "保存聊天室信息失败，ID: " + to_string(i) + "，错误: " + e.what());
                    // 继续保存其他聊天室信息
                }
                catch (...) {
                    logger.logError("Config", "保存聊天室信息时发生未知错误，ID: " + to_string(i));
                }
            }
        }
        root["rooms"] = rooms;

        // 保存服务器配置：HOST 和 PORT
        root["server"]["host"] = CURRENT_HOST;
        root["server"]["port"] = CURRENT_PORT;

        ofstream ofs(CONFIG_FILE);
        if (!ofs.is_open()) {
            logger.logError("Config", "无法打开配置文件进行写入: " + CONFIG_FILE);
            return;
        }
        try {
            ofs << root;
        }
        catch (const std::exception& e) {
            logger.logError("Config", "写入配置文件失败: " + string(e.what()));
        }
        catch (...) {
            logger.logError("Config", "写入配置文件时发生未知错误");
        }
        ofs.close();
    }
    catch (const std::exception& e) {
        logger.logFatal("Config", "保存配置时发生异常: " + string(e.what()));
    }
    catch (...) {
        logger.logFatal("Config", "保存配置时发生未知异常");
    }
}

void loadConfig() {
    Logger& logger = Logger::getInstance();
    try {
        ifstream ifs(CONFIG_FILE);
        if (!ifs.is_open()) {
            // 配置文件不存在，无需加载
            logger.logInfo("Config", "配置文件不存在，跳过加载: " + CONFIG_FILE);
            return;
        }

        Json::Value root;
        try {
            ifs >> root;
        }
        catch (const std::exception& e) {
            logger.logError("Config", "解析配置文件失败: " + string(e.what()));
            ifs.close();
            return;
        }
        catch (...) {
            logger.logError("Config", "解析配置文件时发生未知错误");
            ifs.close();
            return;
        }
        ifs.close();

        // 先加载服务器配置（如果存在则更新全局变量并设置到服务器实例上）
        if (!root["server"].isNull()) {
            try {
                if (root["server"].isMember("host"))
                    CURRENT_HOST = root["server"]["host"].asString();
                if (root["server"].isMember("port"))
                    CURRENT_PORT = root["server"]["port"].asInt();
            }
            catch (const std::exception& e) {
                logger.logError("Config", "读取服务器配置失败: " + string(e.what()));
            }
            catch (...) {
                logger.logError("Config", "读取服务器配置时发生未知错误");
            }

            try {
                // 获取服务器实例并设置HOST和PORT
                Server& server = Server::getInstance();
                server.setHOST(CURRENT_HOST);
                server.setPORT(CURRENT_PORT);
            }
            catch (const std::exception& e) {
                logger.logError("Config", "设置服务器HOST/PORT失败: " + string(e.what()));
            }
            catch (...) {
                logger.logError("Config", "设置服务器HOST/PORT时发生未知错误");
            }
        }
    }
    catch (const std::exception& e) {
        Logger::getInstance().logFatal("Config", "加载配置时发生异常: " + string(e.what()));
    }
    catch (...) {
        Logger::getInstance().logFatal("Config", "加载配置时发生未知异常");
    }
}

void loadChatroomInConfig() {
    Logger& logger = Logger::getInstance();
    try {
        // 加载聊天室配置
        ifstream ifs(CONFIG_FILE);
        if (!ifs.is_open()) {
            // 配置文件不存在，无需加载
            logger.logInfo("Config", "配置文件不存在，跳过聊天室加载: " + CONFIG_FILE);
            return;
        }

        Json::Value root;
        try {
            ifs >> root;
        }
        catch (const std::exception& e) {
            logger.logError("Config", "解析聊天室配置失败: " + string(e.what()));
            ifs.close();
            return;
        }
        catch (...) {
            logger.logError("Config", "解析聊天室配置时发生未知错误");
            ifs.close();
            return;
        }
        ifs.close();

        const Json::Value rooms = root["rooms"];
        for (const auto& roomObj : rooms) {
            try {
                int id = roomObj["id"].asInt();
                string name = roomObj["name"].asString();
                if (id < 0 || id >= MAXROOM) {
                    logger.logError("Config", "无效的聊天室 ID (配置文件): " + to_string(id));
                    continue;
                }
                // 如果该聊天室未创建，则尝试创建
                if (!used[id]) {
                    int newId = -1;
                    try {
                        newId = addroom();
                    }
                    catch (const std::exception& e) {
                        logger.logError("Config", "创建聊天室失败 (ID: " + to_string(id) + "): " + string(e.what()));
                        continue;
                    }
                    catch (...) {
                        logger.logError("Config", "创建聊天室时发生未知错误 (ID: " + to_string(id) + ")");
                        continue;
                    }

                    while (newId < id && newId != -1) {
                        try {
                            newId = addroom();
                        }
                        catch (const std::exception& e) {
                            logger.logError("Config", "继续创建聊天室失败 (期望 ID: " + to_string(id) + "): " + string(e.what()));
                            break;
                        }
                        catch (...) {
                            logger.logError("Config", "继续创建聊天室时发生未知错误 (期望 ID: " + to_string(id) + ")");
                            break;
                        }
                    }
                }

                // 根据配置设置聊天室名称
                try {
                    editroom(id, name);
                }
                catch (const std::exception& e) {
                    logger.logError("Config", "编辑聊天室名称失败 (ID: " + to_string(id) + "): " + string(e.what()));
                }
                catch (...) {
                    logger.logError("Config", "编辑聊天室名称时发生未知错误 (ID: " + to_string(id) + ")");
                }

                // 若配置中包含密码字段，则设置聊天室密码
                if (roomObj.isMember("password")) {
                    try {
                        string pwd = roomObj["password"].asString();
                        room[id].setPassword(pwd);
                    }
                    catch (const std::exception& e) {
                        logger.logError("Config", "设置聊天室密码失败 (ID: " + to_string(id) + "): " + string(e.what()));
                    }
                    catch (...) {
                        logger.logError("Config", "设置聊天室密码时发生未知错误 (ID: " + to_string(id) + ")");
                    }
                }

                // 自动启动聊天室
                try {
                    if (!room[id].start()) {
                        logger.logError("Control", "无法启动聊天室，ID: " + to_string(id));
                    }
                }
                catch (const std::exception& e) {
                    logger.logError("Control", "启动聊天室时抛出异常，ID: " + to_string(id) + "，错误: " + string(e.what()));
                }
                catch (...) {
                    logger.logError("Control", "启动聊天室时发生未知错误，ID: " + to_string(id));
                }
            }
            catch (const std::exception& e) {
                logger.logError("Config", "遍历聊天室配置时发生异常: " + string(e.what()));
            }
            catch (...) {
                logger.logError("Config", "遍历聊天室配置时发生未知错误");
            }
        }
    }
    catch (const std::exception& e) {
        Logger::getInstance().logFatal("Config", "加载聊天室配置时发生异常: " + string(e.what()));
    }
    catch (...) {
        Logger::getInstance().logFatal("Config", "加载聊天室配置时发生未知异常");
    }
}

// -------------------------------------------------------------------
// Command runner functions: loop and execute commands.
// 配置保存将在创建、删除和重命名聊天室时触发。
// -------------------------------------------------------------------
void command_runner(string command, int roomid) {
    // 去除命令末尾多余空格
    while (!command.empty() && command.back() == ' ') {
        command.pop_back();
    }

    Logger& logger = Logger::getInstance();
    // 注意这里为了保持原始逻辑，依然使用默认HOST参数获得服务器实例
    Server& server = Server::getInstance(HOST);
    try {
        // 拆分命令为主命令和参数
        string cmd = command;
        string args = "";
        size_t spacePos = command.find(' ');
        if (spacePos != string::npos) {
            cmd = command.substr(0, spacePos);
            args = command.substr(spacePos + 1);
            // 去除参数首尾的空格
            while (!args.empty() && args.front() == ' ') args.erase(0, 1);
            while (!args.empty() && args.back() == ' ') args.pop_back();
        }

        if (command == "start") {  // 完整匹配 start 命令
            logger.logInfo("Control", "尝试开启服务器...");
            try {
                std::thread redirectThread([]() {
                    try {
                        Redirection::startRedirectServer();
                    }
                    catch (const std::exception& e) {
                        Logger::getInstance().logError("Redirect", "重定向服务器启动失败: " + string(e.what()));
                    }
                    catch (...) {
                        Logger::getInstance().logError("Redirect", "重定向服务器启动时发生未知错误");
                    }
                    });
                server.start();
                redirectThread.join();
            }
            catch (const std::exception& e) {
                logger.logFatal("Control", "启动服务器时发生异常: " + string(e.what()));
                return;
            }
            catch (...) {
                logger.logFatal("Control", "启动服务器时发生未知异常");
                return;
            }
        }
        else if (command == "load") {  // 完整匹配 load 命令
            logger.logInfo("Control", "尝试加载数据...");
            try {
                // Initialize the database
                manager::InitDatabase("./database.db");
                logger.logInfo("Control", "数据库已初始化");
            }
            catch (const std::exception& e) {
                logger.logFatal("Control", "数据库初始化失败: " + string(e.what()));
                return;
            }
            catch (...) {
                logger.logFatal("Control", "数据库初始化时发生未知错误");
                return;
            }

            try {
                start_loginSystem();
            }
            catch (const std::exception& e) {
                logger.logError("Control", "启动登录系统失败: " + string(e.what()));
            }
            catch (...) {
                logger.logError("Control", "启动登录系统时发生未知错误");
            }

            try {
                start_manager();
            }
            catch (const std::exception& e) {
                logger.logError("Control", "启动管理系统失败: " + string(e.what()));
            }
            catch (...) {
                logger.logError("Control", "启动管理系统时发生未知错误");
            }

            loadChatroomInConfig();
        }
        else if (cmd == "create") {
            try {
                int newRoomId = addroom();
                if (newRoomId != -1) {
                    // 自动启动新创建的聊天室
                    try {
                        if (room[newRoomId].start()) {
                            logger.logInfo("Control", "聊天室已创建并启动，ID: " + to_string(newRoomId));
                            saveConfig();  // 创建后保存配置
                        }
                        else {
                            logger.logError("Control", "聊天室已创建但无法启动，ID: " + to_string(newRoomId));
                        }
                    }
                    catch (const std::exception& e) {
                        logger.logError("Control", "启动新聊天室失败，ID: " + to_string(newRoomId) + "，错误: " + string(e.what()));
                    }
                    catch (...) {
                        logger.logError("Control", "启动新聊天室时发生未知错误，ID: " + to_string(newRoomId));
                    }
                }
                else {
                    logger.logError("Control", "无法创建聊天室，已达到最大数量");
                }
            }
            catch (const std::exception& e) {
                logger.logError("Control", "执行 create 命令时发生异常: " + string(e.what()));
            }
            catch (...) {
                logger.logError("Control", "执行 create 命令时发生未知错误");
            }
        }
        else if (cmd == "delete" && !args.empty()) {
            try {
                int roomId = stoi(args);
                if (roomId >= 0 && roomId < MAXROOM && used[roomId]) {
                    delroom(roomId);
                    logger.logInfo("Control", "聊天室已删除，ID: " + to_string(roomId));
                    saveConfig();  // 删除后保存配置
                }
                else {
                    logger.logError("Control", "无效的聊天室 ID: " + to_string(roomId));
                }
            }
            catch (const std::invalid_argument&) {
                logger.logError("Control", "delete 命令参数格式错误, ID 必须为整数");
            }
            catch (const std::exception& e) {
                logger.logError("Control", "执行 delete 命令时发生异常: " + string(e.what()));
            }
            catch (...) {
                logger.logError("Control", "执行 delete 命令时发生未知错误");
            }
        }
        else if (cmd == "settype" && !args.empty()) {
            size_t spacePos2 = args.find(' ');
            if (spacePos2 != string::npos) {
                string idPart = args.substr(0, spacePos2);
                string typePart = args.substr(spacePos2 + 1);
                int roomId = -1, newType = -1;

                try {
                    roomId = stoi(idPart);
                    newType = stoi(typePart);
                }
                catch (const std::exception&) {
                    logger.logError("Control", "settype 命令格式错误，ID 和类型必须为整数");
                    return;
                }

                if (roomId < 0 || roomId >= MAXROOM) {
                    logger.logError("Control", "无效的聊天室 ID: " + to_string(roomId));
                }
                else if (!used[roomId]) {
                    logger.logError("Control", "聊天室未创建，ID: " + to_string(roomId));
                }
                else {
                    try {
                        setroomtype(roomId, newType);
                        logger.logInfo("Control", "聊天室类型已更新，ID: " + to_string(roomId) + "，新类型: " + to_string(newType));
                        // Optionally save configuration if type changes need persistence
                        saveConfig();
                    }
                    catch (const std::exception& e) {
                        logger.logError("Control", "更新聊天室类型失败，ID: " + to_string(roomId) + "，错误: " + string(e.what()));
                    }
                    catch (...) {
                        logger.logError("Control", "更新聊天室类型时发生未知错误，ID: " + to_string(roomId));
                    }
                }
            }
            else {
                logger.logError("Control", "settype 命令格式错误，需要提供 ID 和类型");
            }
        }
        else if (cmd == "rename" && !args.empty()) {
            size_t spacePos3 = args.find(' ');
            if (spacePos3 != string::npos) {
                string idPart = args.substr(0, spacePos3);
                string newName = args.substr(spacePos3 + 1);
                try {
                    int roomId = stoi(idPart);
                    if (roomId >= 0 && roomId < MAXROOM && used[roomId]) {
                        try {
                            editroom(roomId, newName);
                            logger.logInfo("Control", "聊天室已重命名，ID: " + to_string(roomId) + "，新名称: " + newName);
                            saveConfig();  // 重命名后保存配置
                        }
                        catch (const std::exception& e) {
                            logger.logError("Control", "重命名聊天室失败，ID: " + to_string(roomId) + "，错误: " + string(e.what()));
                        }
                        catch (...) {
                            logger.logError("Control", "重命名聊天室时发生未知错误，ID: " + to_string(roomId));
                        }
                    }
                    else {
                        logger.logError("Control", "无效的聊天室 ID: " + to_string(roomId));
                    }
                }
                catch (const std::exception&) {
                    logger.logError("Control", "rename 命令参数格式错误，ID 必须为整数");
                }
                catch (...) {
                    logger.logError("Control", "rename 命令时发生未知错误");
                }
            }
            else {
                logger.logError("Control", "rename 命令格式错误，需要提供 ID 和新名称");
            }
        }
        else if (cmd == "stop") {
            try {
                manager::CloseDatabase();
                logger.logInfo("Control", "数据库已关闭");
            }
            catch (const std::exception& e) {
                logger.logError("Control", "关闭数据库失败: " + string(e.what()));
            }
            catch (...) {
                logger.logError("Control", "关闭数据库时发生未知错误");
            }
            exit(0);
        }
        else if (cmd == "say" && !args.empty()) {
            size_t msgPos = args.find(' ');
            if (msgPos != string::npos) {
                try {
                    int roomId = stoi(args.substr(0, msgPos));
                    string message = args.substr(msgPos + 1);
                    if (roomId >= 0 && roomId < MAXROOM && used[roomId]) {
                        try {
                            room[roomId].systemMessage(message);
                            logger.logInfo("Control", "消息已发送到聊天室 " + to_string(roomId));
                        }
                        catch (const std::exception& e) {
                            logger.logError("Control", "发送系统消息失败，聊天室 ID: " + to_string(roomId) + "，错误: " + string(e.what()));
                        }
                        catch (...) {
                            logger.logError("Control", "发送系统消息时发生未知错误，聊天室 ID: " + to_string(roomId));
                        }
                    }
                    else {
                        logger.logError("Control", "无效的聊天室 ID: " + to_string(roomId));
                    }
                }
                catch (const std::exception&) {
                    logger.logError("Control", "say 命令参数格式错误，ID 必须为整数");
                }
                catch (...) {
                    logger.logError("Control", "say 命令时发生未知错误");
                }
            }
            else {
                logger.logError("Control", "命令格式错误: say <roomId> <message>");
            }
        }
        else if (cmd == "clear" && !args.empty()) {
            try {
                int roomId = stoi(args);
                if (roomId >= 0 && roomId < MAXROOM && used[roomId]) {
                    try {
                        room[roomId].clearMessage();
                        logger.logInfo("Control", "聊天室 " + to_string(roomId) + " 的消息列表已清空");
                    }
                    catch (const std::exception& e) {
                        logger.logError("Control", "清空消息失败，聊天室 ID: " + to_string(roomId) + "，错误: " + string(e.what()));
                    }
                    catch (...) {
                        logger.logError("Control", "清空消息时发生未知错误，聊天室 ID: " + to_string(roomId));
                    }
                }
                else {
                    logger.logError("Control", "无效的聊天室 ID: " + to_string(roomId));
                }
            }
            catch (const std::exception&) {
                logger.logError("Control", "clear 命令参数格式错误，ID 必须为整数");
            }
            catch (...) {
                logger.logError("Control", "clear 命令时发生未知错误");
            }
        }
        else if (cmd == "ban" && !args.empty()) {
            try {
                server.banIP(args);
                logger.logInfo("Control", "已封禁 IP: " + args);
            }
            catch (const std::exception& e) {
                logger.logError("Control", "封禁 IP 失败: " + string(e.what()));
            }
            catch (...) {
                logger.logError("Control", "封禁 IP 时发生未知错误");
            }
        }
        else if (cmd == "deban" && !args.empty()) {
            try {
                server.debanIP(args);
                logger.logInfo("Control", "已解封 IP: " + args);
            }
            catch (const std::exception& e) {
                logger.logError("Control", "解封 IP 失败: " + string(e.what()));
            }
            catch (...) {
                logger.logError("Control", "解封 IP 时发生未知错误");
            }
        }
        else if (cmd == "setpassword" && !args.empty()) {
            size_t spacePos4 = args.find(' ');
            if (spacePos4 != string::npos) {
                string idPart = args.substr(0, spacePos4);
                string password = args.substr(spacePos4 + 1);
                try {
                    int roomId = stoi(idPart);
                    if (roomId >= 0 && roomId < MAXROOM && used[roomId]) {
                        try {
                            if (password == "clear") {
                                room[roomId].setPassword(""); // 清空密码
                                logger.logInfo("Control", "聊天室密码已清空，ID: " + to_string(roomId));
                            }
                            else {
                                room[roomId].setPassword(password); // 设置新密码
                                logger.logInfo("Control", "聊天室密码已设置，ID: " + to_string(roomId));
                            }
                            saveConfig();
                        }
                        catch (const std::exception& e) {
                            logger.logError("Control", "设置聊天室密码失败, ID: " + to_string(roomId) + "，错误: " + string(e.what()));
                        }
                        catch (...) {
                            logger.logError("Control", "设置聊天室密码时发生未知错误, ID: " + to_string(roomId));
                        }
                    }
                    else {
                        logger.logError("Control", "无效的聊天室 ID: " + to_string(roomId));
                    }
                }
                catch (const std::exception&) {
                    logger.logError("Control", "setpassword 命令参数格式错误，ID 必须为整数");
                }
                catch (...) {
                    logger.logError("Control", "setpassword 命令时发生未知错误");
                }
            }
            else {
                logger.logError("Control", "setpassword 命令格式错误，需要提供 ID 和密码");
            }
        }
        else if (cmd == "listuser") {
            try {
                auto userDetails = manager::GetUserDetails();
                if (userDetails.empty()) {
                    logger.logInfo("Control", "当前没有用户");
                }
                else {
                    logger.logInfo("Control", "用户列表:");
                    for (const auto& [name, password, uid] : userDetails) {
                        logger.logInfo("Control", "用户名: " + name + ", 密码: " + password + ", UID: " + to_string(uid));
                    }
                }
            }
            catch (const std::exception& e) {
                logger.logError("Control", "获取用户列表失败: " + string(e.what()));
            }
            catch (...) {
                logger.logError("Control", "获取用户列表时发生未知错误");
            }
        }
        else if (cmd == "listroom") {
            try {
                auto roomDetails = GetRoomListDetails();
                if (roomDetails.empty()) {
                    logger.logInfo("Control", "当前没有聊天室");
                }
                else {
                    logger.logInfo("Control", "聊天室列表:");
                    for (const auto& [id, name, password] : roomDetails) {
                        logger.logInfo("Control", "ID: " + to_string(id) + ", 名称: " + name + ", 密码: " + password);
                    }
                }
            }
            catch (const std::exception& e) {
                logger.logError("Control", "获取聊天室列表失败: " + string(e.what()));
            }
            catch (...) {
                logger.logError("Control", "获取聊天室列表时发生未知错误");
            }
        }
        else if (cmd == "rmuser" && !args.empty()) {
            try {
                int userId = stoi(args);
                try {
                    if (manager::RemoveUser(userId)) {
                        logger.logInfo("Control", "用户已成功删除，UID: " + to_string(userId));
                        //manager::WriteUserData("./", manager::datafile);
                    }
                    else {
                        logger.logError("Control", "删除用户失败，可能找不到UID: " + to_string(userId));
                    }
                }
                catch (const std::exception& e) {
                    logger.logError("Control", "删除用户时发生异常，UID: " + to_string(userId) + "，错误: " + string(e.what()));
                }
                catch (...) {
                    logger.logError("Control", "删除用户时发生未知错误，UID: " + to_string(userId));
                }
            }
            catch (const std::exception&) {
                logger.logError("Control", "无效的用户ID格式");
            }
            catch (...) {
                logger.logError("Control", "rmuser 命令时发生未知错误");
            }
        }
        else if (cmd == "help") {
            logger.logInfo("Control", "可用指令:\n"
                "  start - 启动服务器\n"
                "  stop - 停止服务器\n"
                "  create - 创建并启动一个新的聊天室\n"
                "  delete <roomId> - 删除指定聊天室\n"
                "  rename <roomId> <name> - 为指定聊天室重命名\n"
                "  settype <roomId> <type> - 为指定聊天室更改类型 (ROOM_HIDDEN = 1 << 0,ROOM_NO_JOIN = 1 << 1)\n"
                "  say <roomId> <message> - 向指定聊天室发送消息\n"
                "  clear <roomId> - 清空指定聊天室的消息\n"
                "  ban <ip> - 封禁指定IP\n"
                "  deban <ip> - 解封指定IP\n"
                "  setpassword <roomId> <password> - 为指定聊天室设置密码\n"
                "  listuser - 列出所有用户\n"
                "  listroom - 列出所有聊天室\n"
                "  rmuser <userId> - 删除指定用户ID的用户\n"
                "  help - 显示此帮助信息");
        }
        else {
            logger.logError("Control", "不合法的指令: " + command);
        }
    }
    catch (const exception& e) {
        logger.logFatal("Control", "命令执行失败: " + string(e.what()));
    }
    catch (...) {
        logger.logFatal("Control", "命令执行时发生未知错误");
    }
}

void command() {
    Logger& logger = Logger::getInstance();
    while (true) {
        try {
            string cmd;
            if (!getline(cin, cmd)) {
                // 如果读取失败或EOF，退出循环
                break;
            }
            // 每条命令均在分离线程中处理
            try {
                thread cmd_thread(command_runner, cmd, 0);
                cmd_thread.detach();
            }
            catch (const std::exception& e) {
                logger.logError("Control", "启动命令线程失败: " + string(e.what()));
            }
            catch (...) {
                logger.logError("Control", "启动命令线程时发生未知错误");
            }
        }
        catch (const std::exception& e) {
            logger.logError("Control", "读取命令时发生异常: " + string(e.what()));
        }
        catch (...) {
            logger.logError("Control", "读取命令时发生未知错误");
        }
    }
}

void run() {
    Logger& logger = Logger::getInstance();
    try {
        // 初始化：创建几个默认聊天室
        //addroom(); // 创建第一个聊天室
        //addroom(); // 创建第二个聊天室
        // 启动时加载配置（包括聊天室配置及服务器配置）
        logger.logInfo("Control", "控制线程已开启");
        loadConfig();
        logger.logInfo("Control", "已加载配置文件");
        try {
            thread maint(command);
            maint.join();
        }
        catch (const std::exception& e) {
            logger.logError("Control", "启动控制线程失败: " + string(e.what()));
        }
        catch (...) {
            logger.logError("Control", "启动控制线程时发生未知错误");
        }

        logger.logInfo("Control", "控制线程已结束");
    }
    catch (const std::exception& e) {
        logger.logFatal("Control", "运行控制模块时发生异常: " + string(e.what()));
    }
    catch (...) {
        logger.logFatal("Control", "运行控制模块时发生未知错误");
    }
    return;
}
