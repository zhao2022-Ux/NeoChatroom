#include "../Server.h"
#include "../../tool/log.h"
#include "../../data/datamanage.h"
#include "../../chat/chatroom.h"
#include "../../chat/roommanager.h"
#include "../../config/serverconfig.h"
#include <string>
#include <thread>
#include <iostream>
#include <locale>
#include <codecvt>
#include <stdexcept>
#include "../ControlServer.h"
#include "../RedirectServer.h"

using namespace std;

// Forward declaration for login system starter
void start_loginSystem();

// 修改函数声明，返回Json::Value
/*
* 命令执行函数，返回JSON格式的执行结果
* 返回值结构：
* {
*   "status": "success/error",  // 执行状态
*   "command": "执行的命令",     // 原始命令
*   "data": {                   // 命令返回的具体数据
*     // 根据不同命令返回不同的数据结构
*   },
*   "message": "执行结果描述"    // 执行结果的详细描述
* }
*/
Json::Value command_runner(string command, int roomid) {
    Json::Value result;
    result["command"] = command;
    result["data"] = Json::Value(Json::objectValue);

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

        if (command == "start") {
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
                result["status"] = "success";
                result["message"] = "服务器已成功启动";
            }
            catch (const std::exception& e) {
                logger.logFatal("Control", "启动服务器时发生异常: " + string(e.what()));
                result["status"] = "error";
                result["message"] = "启动服务器时发生异常: " + string(e.what());
                return result;
            }
            catch (...) {
                logger.logFatal("Control", "启动服务器时发生未知异常");
                result["status"] = "error";
                result["message"] = "启动服务器时发生未知异常";
                return result;
            }
        }
        else if (command == "load") {
            logger.logInfo("Control", "尝试加载数据...");
            try {
                // Initialize the database
                manager::InitDatabase("./database.db");
                logger.logInfo("Control", "数据库已初始化");

                try {
                    start_loginSystem();
                }
                catch (const std::exception& e) {
                    logger.logError("Control", "启动登录系统失败: " + string(e.what()));
                    result["status"] = "error";
                    result["message"] = "启动登录系统失败: " + string(e.what());
                    return result;
                }
                catch (...) {
                    logger.logError("Control", "启动登录系统时发生未知错误");
                    result["status"] = "error";
                    result["message"] = "启动登录系统时发生未知错误";
                    return result;
                }

                try {
                    start_manager();
                    startControlServer(ControlLoginPassword);
                    logger.logInfo("Control", "WEB控制服务器已启动");
                }
                catch (const std::exception& e) {
                    logger.logError("Control", "启动管理系统失败: " + string(e.what()));
                    result["status"] = "error";
                    result["message"] = "启动管理系统失败: " + string(e.what());
                    return result;
                }
                catch (...) {
                    logger.logError("Control", "启动管理系统时发生未知错误");
                    result["status"] = "error";
                    result["message"] = "启动管理系统时发生未知错误";
                    return result;
                }

                //loadChatroomInConfig();
                result["status"] = "success";
                result["message"] = "数据加载完成";
            }
            catch (const std::exception& e) {
                logger.logFatal("Control", "数据库初始化失败: " + string(e.what()));
                result["status"] = "error";
                result["message"] = "数据库初始化失败: " + string(e.what());
                return result;
            }
            catch (...) {
                logger.logFatal("Control", "数据库初始化时发生未知错误");
                result["status"] = "error";
                result["message"] = "数据库初始化时发生未知错误";
                return result;
            }
        }
        else if (cmd == "create") {
            try {
                int newRoomId = addroom();
                if (newRoomId != -1) {
                    try {
                        if (room[newRoomId].start()) {
                            logger.logInfo("Control", "聊天室已创建并启动，ID: " + to_string(newRoomId));
                            //saveConfig();  // 创建后保存配置
                            result["status"] = "success";
                            result["message"] = "聊天室已创建并启动";
                            result["data"]["roomId"] = newRoomId;
                        }
                        else {
                            logger.logError("Control", "聊天室已创建但无法启动，ID: " + to_string(newRoomId));
                            result["status"] = "error";
                            result["message"] = "聊天室已创建但无法启动";
                            result["data"]["roomId"] = newRoomId;
                        }
                    }
                    catch (const std::exception& e) {
                        logger.logError("Control", "启动新聊天室失败，ID: " + to_string(newRoomId) + "，错误: " + string(e.what()));
                        result["status"] = "error";
                        result["message"] = "启动新聊天室失败: " + string(e.what());
                        result["data"]["roomId"] = newRoomId;
                    }
                    catch (...) {
                        logger.logError("Control", "启动新聊天室时发生未知错误，ID: " + to_string(newRoomId));
                        result["status"] = "error";
                        result["message"] = "启动新聊天室时发生未知错误";
                        result["data"]["roomId"] = newRoomId;
                    }
                }
                else {
                    logger.logError("Control", "无法创建聊天室，已达到最大数量");
                    result["status"] = "error";
                    result["message"] = "无法创建聊天室，已达到最大数量";
                }
            }
            catch (const std::exception& e) {
                logger.logError("Control", "执行 create 命令时发生异常: " + string(e.what()));
                result["status"] = "error";
                result["message"] = "执行 create 命令时发生异常: " + string(e.what());
            }
            catch (...) {
                logger.logError("Control", "执行 create 命令时发生未知错误");
                result["status"] = "error";
                result["message"] = "执行 create 命令时发生未知错误";
            }
        }
        else if (cmd == "delete" && !args.empty()) {
            try {
                int roomId = stoi(args);
                if (roomId >= 0 && roomId < MAXROOM && used[roomId]) {
                    delroom(roomId);
                    logger.logInfo("Control", "聊天室已删除，ID: " + to_string(roomId));
                    //saveConfig();  // 删除后保存配置
                    result["status"] = "success";
                    result["message"] = "聊天室已删除";
                    result["data"]["roomId"] = roomId;
                }
                else {
                    logger.logError("Control", "无效的聊天室 ID: " + to_string(roomId));
                    result["status"] = "error";
                    result["message"] = "无效的聊天室 ID: " + to_string(roomId);
                }
            }
            catch (const std::invalid_argument&) {
                logger.logError("Control", "delete 命令参数格式错误, ID 必须为整数");
                result["status"] = "error";
                result["message"] = "delete 命令参数格式错误, ID 必须为整数";
            }
            catch (const std::exception& e) {
                logger.logError("Control", "执行 delete 命令时发生异常: " + string(e.what()));
                result["status"] = "error";
                result["message"] = "执行 delete 命令时发生异常: " + string(e.what());
            }
            catch (...) {
                logger.logError("Control", "执行 delete 命令时发生未知错误");
                result["status"] = "error";
                result["message"] = "执行 delete 命令时发生未知错误";
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

                    if (roomId < 0 || roomId >= MAXROOM) {
                        logger.logError("Control", "无效的聊天室 ID: " + to_string(roomId));
                        result["status"] = "error";
                        result["message"] = "无效的聊天室 ID: " + to_string(roomId);
                    }
                    else if (!used[roomId]) {
                        logger.logError("Control", "聊天室未创建，ID: " + to_string(roomId));
                        result["status"] = "error";
                        result["message"] = "聊天室未创建，ID: " + to_string(roomId);
                    }
                    else {
                        try {
                            setroomtype(roomId, newType);
                            logger.logInfo("Control", "聊天室类型已更新，ID: " + to_string(roomId) + "，新类型: " + to_string(newType));
                            //saveConfig();
                            result["status"] = "success";
                            result["message"] = "聊天室类型已更新";
                            result["data"]["roomId"] = roomId;
                            result["data"]["newType"] = newType;
                        }
                        catch (const std::exception& e) {
                            logger.logError("Control", "更新聊天室类型失败，ID: " + to_string(roomId) + "，错误: " + string(e.what()));
                            result["status"] = "error";
                            result["message"] = "更新聊天室类型失败: " + string(e.what());
                        }
                        catch (...) {
                            logger.logError("Control", "更新聊天室类型时发生未知错误，ID: " + to_string(roomId));
                            result["status"] = "error";
                            result["message"] = "更新聊天室类型时发生未知错误";
                        }
                    }
                }
                catch (const std::exception&) {
                    logger.logError("Control", "settype 命令参数格式错误，ID 和类型必须为整数");
                    result["status"] = "error";
                    result["message"] = "settype 命令参数格式错误，ID 和类型必须为整数";
                }
            }
            else {
                logger.logError("Control", "settype 命令格式错误，需要提供 ID 和类型");
                result["status"] = "error";
                result["message"] = "settype 命令格式错误，需要提供 ID 和类型";
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
                            //saveConfig();  // 重命名后保存配置
                            result["status"] = "success";
                            result["message"] = "聊天室已重命名";
                            result["data"]["roomId"] = roomId;
                            result["data"]["newName"] = newName;
                        }
                        catch (const std::exception& e) {
                            logger.logError("Control", "重命名聊天室失败，ID: " + to_string(roomId) + "，错误: " + string(e.what()));
                            result["status"] = "error";
                            result["message"] = "重命名聊天室失败: " + string(e.what());
                        }
                        catch (...) {
                            logger.logError("Control", "重命名聊天室时发生未知错误，ID: " + to_string(roomId));
                            result["status"] = "error";
                            result["message"] = "重命名聊天室时发生未知错误";
                        }
                    }
                    else {
                        logger.logError("Control", "无效的聊天室 ID: " + to_string(roomId));
                        result["status"] = "error";
                        result["message"] = "无效的聊天室 ID: " + to_string(roomId);
                    }
                }
                catch (const std::exception&) {
                    logger.logError("Control", "rename 命令参数格式错误，ID 必须为整数");
                    result["status"] = "error";
                    result["message"] = "rename 命令参数格式错误，ID 必须为整数";
                }
                catch (...) {
                    logger.logError("Control", "rename 命令时发生未知错误");
                    result["status"] = "error";
                    result["message"] = "rename 命令时发生未知错误";
                }
            }
            else {
                logger.logError("Control", "rename 命令格式错误，需要提供 ID 和新名称");
                result["status"] = "error";
                result["message"] = "rename 命令格式错误，需要提供 ID 和新名称";
            }
        }
        else if (cmd == "stop") {
            try {
                manager::CloseDatabase();
                logger.logInfo("Control", "数据库已关闭");
                result["status"] = "success";
                result["message"] = "服务器已停止，数据库已关闭";
                exit(0);
            }
            catch (const std::exception& e) {
                logger.logError("Control", "关闭数据库失败: " + string(e.what()));
                result["status"] = "error";
                result["message"] = "关闭数据库失败: " + string(e.what());
            }
            catch (...) {
                logger.logError("Control", "关闭数据库时发生未知错误");
                result["status"] = "error";
                result["message"] = "关闭数据库时发生未知错误";
            }
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
                            result["status"] = "success";
                            result["message"] = "消息已发送到聊天室";
                            result["data"]["roomId"] = roomId;
                            result["data"]["message"] = message;
                        }
                        catch (const std::exception& e) {
                            logger.logError("Control", "发送系统消息失败，聊天室 ID: " + to_string(roomId) + "，错误: " + string(e.what()));
                            result["status"] = "error";
                            result["message"] = "发送系统消息失败: " + string(e.what());
                        }
                        catch (...) {
                            logger.logError("Control", "发送系统消息时发生未知错误，聊天室 ID: " + to_string(roomId));
                            result["status"] = "error";
                            result["message"] = "发送系统消息时发生未知错误";
                        }
                    }
                    else {
                        logger.logError("Control", "无效的聊天室 ID: " + to_string(roomId));
                        result["status"] = "error";
                        result["message"] = "无效的聊天室 ID: " + to_string(roomId);
                    }
                }
                catch (const std::exception&) {
                    logger.logError("Control", "say 命令参数格式错误，ID 必须为整数");
                    result["status"] = "error";
                    result["message"] = "say 命令参数格式错误，ID 必须为整数";
                }
                catch (...) {
                    logger.logError("Control", "say 命令时发生未知错误");
                    result["status"] = "error";
                    result["message"] = "say 命令时发生未知错误";
                }
            }
            else {
                logger.logError("Control", "命令格式错误: say <roomId> <message>");
                result["status"] = "error";
                result["message"] = "命令格式错误: say <roomId> <message>";
            }
        }
        else if (cmd == "clear" && !args.empty()) {
            try {
                int roomId = stoi(args);
                if (roomId >= 0 && roomId < MAXROOM && used[roomId]) {
                    try {
                        room[roomId].clearMessage();
                        logger.logInfo("Control", "聊天室 " + to_string(roomId) + " 的消息列表已清空");
                        result["status"] = "success";
                        result["message"] = "聊天室消息列表已清空";
                        result["data"]["roomId"] = roomId;
                    }
                    catch (const std::exception& e) {
                        logger.logError("Control", "清空消息失败，聊天室 ID: " + to_string(roomId) + "，错误: " + string(e.what()));
                        result["status"] = "error";
                        result["message"] = "清空消息失败: " + string(e.what());
                    }
                    catch (...) {
                        logger.logError("Control", "清空消息时发生未知错误，聊天室 ID: " + to_string(roomId));
                        result["status"] = "error";
                        result["message"] = "清空消息时发生未知错误";
                    }
                }
                else {
                    logger.logError("Control", "无效的聊天室 ID: " + to_string(roomId));
                    result["status"] = "error";
                    result["message"] = "无效的聊天室 ID: " + to_string(roomId);
                }
            }
            catch (const std::exception&) {
                logger.logError("Control", "clear 命令参数格式错误，ID 必须为整数");
                result["status"] = "error";
                result["message"] = "clear 命令参数格式错误，ID 必须为整数";
            }
            catch (...) {
                logger.logError("Control", "clear 命令时发生未知错误");
                result["status"] = "error";
                result["message"] = "clear 命令时发生未知错误";
            }
        }
        else if (cmd == "ban" && !args.empty()) {
            try {
                server.banIP(args);
                logger.logInfo("Control", "已封禁 IP: " + args);
                result["status"] = "success";
                result["message"] = "IP已封禁";
                result["data"]["ip"] = args;
            }
            catch (const std::exception& e) {
                logger.logError("Control", "封禁 IP 失败: " + string(e.what()));
                result["status"] = "error";
                result["message"] = "封禁 IP 失败: " + string(e.what());
            }
            catch (...) {
                logger.logError("Control", "封禁 IP 时发生未知错误");
                result["status"] = "error";
                result["message"] = "封禁 IP 时发生未知错误";
            }
        }
        else if (cmd == "deban" && !args.empty()) {
            try {
                server.debanIP(args);
                logger.logInfo("Control", "已解封 IP: " + args);
                result["status"] = "success";
                result["message"] = "IP已解封";
                result["data"]["ip"] = args;
            }
            catch (const std::exception& e) {
                logger.logError("Control", "解封 IP 失败: " + string(e.what()));
                result["status"] = "error";
                result["message"] = "解封 IP 失败: " + string(e.what());
            }
            catch (...) {
                logger.logError("Control", "解封 IP 时发生未知错误");
                result["status"] = "error";
                result["message"] = "解封 IP 时发生未知错误";
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
                                result["status"] = "success";
                                result["message"] = "聊天室密码已清空";
                            }
                            else {
                                room[roomId].setPassword(password); // 设置新密码
                                logger.logInfo("Control", "聊天室密码已设置，ID: " + to_string(roomId));
                                result["status"] = "success";
                                result["message"] = "聊天室密码已设置";
                            }
                            result["data"]["roomId"] = roomId;
                            //saveConfig();
                        }
                        catch (const std::exception& e) {
                            logger.logError("Control", "设置聊天室密码失败, ID: " + to_string(roomId) + "，错误: " + string(e.what()));
                            result["status"] = "error";
                            result["message"] = "设置聊天室密码失败: " + string(e.what());
                        }
                        catch (...) {
                            logger.logError("Control", "设置聊天室密码时发生未知错误, ID: " + to_string(roomId));
                            result["status"] = "error";
                            result["message"] = "设置聊天室密码时发生未知错误";
                        }
                    }
                    else {
                        logger.logError("Control", "无效的聊天室 ID: " + to_string(roomId));
                        result["status"] = "error";
                        result["message"] = "无效的聊天室 ID: " + to_string(roomId);
                    }
                }
                catch (const std::exception&) {
                    logger.logError("Control", "setpassword 命令参数格式错误，ID 必须为整数");
                    result["status"] = "error";
                    result["message"] = "setpassword 命令参数格式错误，ID 必须为整数";
                }
                catch (...) {
                    logger.logError("Control", "setpassword 命令时发生未知错误");
                    result["status"] = "error";
                    result["message"] = "setpassword 命令时发生未知错误";
                }
            }
            else {
                logger.logError("Control", "setpassword 命令格式错误，需要提供 ID 和密码");
                result["status"] = "error";
                result["message"] = "setpassword 命令格式错误，需要提供 ID 和密码";
            }
        }
        else if (cmd == "listuser") {
            try {
                auto userDetails = manager::GetUserDetails();
                if (userDetails.empty()) {
                    logger.logInfo("Control", "当前没有用户");
                    result["status"] = "success";
                    result["message"] = "当前没有用户";
                }
                else {
                    logger.logInfo("Control", "用户列表已获取");
                    result["status"] = "success";
                    result["message"] = "用户列表获取成功";
                    Json::Value users(Json::arrayValue);
                    for (const auto& [name, password, uid] : userDetails) {
                        Json::Value user;
                        user["name"] = name;
                        user["password"] = password;
                        user["uid"] = uid;
                        users.append(user);
                    }
                    result["data"]["users"] = users;
                }
            }
            catch (const std::exception& e) {
                logger.logError("Control", "获取用户列表失败: " + string(e.what()));
                result["status"] = "error";
                result["message"] = "获取用户列表失败: " + string(e.what());
            }
            catch (...) {
                logger.logError("Control", "获取用户列表时发生未知错误");
                result["status"] = "error";
                result["message"] = "获取用户列表时发生未知错误";
            }
        }
        else if (cmd == "listroom") {
            try {
                auto roomDetails = GetRoomListDetails();
                if (roomDetails.empty()) {
                    logger.logInfo("Control", "当前没有聊天室");
                    result["status"] = "success";
                    result["message"] = "当前没有聊天室";
                }
                else {
                    logger.logInfo("Control", "聊天室列表已获取");
                    result["status"] = "success";
                    result["message"] = "聊天室列表获取成功";
                    Json::Value rooms(Json::arrayValue);
                    for (const auto& [id, name, password] : roomDetails) {
                        Json::Value room;
                        room["id"] = id;
                        room["name"] = name;
                        room["password"] = password;
                        rooms.append(room);
                    }
                    result["data"]["rooms"] = rooms;
                }
            }
            catch (const std::exception& e) {
                logger.logError("Control", "获取聊天室列表失败: " + string(e.what()));
                result["status"] = "error";
                result["message"] = "获取聊天室列表失败: " + string(e.what());
            }
            catch (...) {
                logger.logError("Control", "获取聊天室列表时发生未知错误");
                result["status"] = "error";
                result["message"] = "获取聊天室列表时发生未知错误";
            }
        }
        else if (cmd == "rmuser" && !args.empty()) {
            try {
                int userId = stoi(args);
                try {
                    if (manager::BanUser(userId)) {
                        logger.logInfo("Control", "用户已成功删除，UID: " + to_string(userId));
                        result["status"] = "success";
                        result["message"] = "用户已成功删除";
                        result["data"]["userId"] = userId;
                    }
                    else {
                        logger.logError("Control", "删除用户失败，可能找不到UID: " + to_string(userId));
                        result["status"] = "error";
                        result["message"] = "删除用户失败，可能找不到指定用户";
                        result["data"]["userId"] = userId;
                    }
                }
                catch (const std::exception& e) {
                                        logger.logError("Control", "删除用户时发生异常，UID: " + to_string(userId) + "，错误: " + string(e.what()));
                    result["status"] = "error";
                    result["message"] = "删除用户时发生异常: " + string(e.what());
                }
                catch (...) {
                    logger.logError("Control", "删除用户时发生未知错误，UID: " + to_string(userId));
                    result["status"] = "error";
                    result["message"] = "删除用户时发生未知错误";
                }
            }
            catch (const std::exception&) {
                logger.logError("Control", "无效的用户ID格式");
                result["status"] = "error";
                result["message"] = "无效的用户ID格式";
            }
            catch (...) {
                logger.logError("Control", "rmuser 命令时发生未知错误");
                result["status"] = "error";
                result["message"] = "rmuser 命令时发生未知错误";
            }
        }
        else if (cmd == "setadmin" && !args.empty()) {
            try {
                int userId = stoi(args);
                try {
                    if (manager::SetAdmin(userId)) {
                        logger.logInfo("Control", "用户已设为管理员，UID: " + to_string(userId));
                        result["status"] = "success";
                        result["message"] = "用户已设为管理员";
                        result["data"]["userId"] = userId;
                    }
                    else {
                        logger.logError("Control", "设为管理员失败，可能找不到UID: " + to_string(userId));
                        result["status"] = "error";
                        result["message"] = "设为管理员失败，可能找不到指定用户";
                        result["data"]["userId"] = userId;
                    }
                }
                catch (const std::exception& e) {
                    logger.logError("Control", "设为管理员时发生异常，UID: " + to_string(userId) + "，错误: " + string(e.what()));
                    result["status"] = "error";
                    result["message"] = "设为管理员时发生异常: " + string(e.what());
                }
                catch (...) {
                    logger.logError("Control", "设为管理员时发生未知错误，UID: " + to_string(userId));
                    result["status"] = "error";
                    result["message"] = "设为管理员时发生未知错误";
                }
            }
            catch (const std::exception&) {
                logger.logError("Control", "无效的用户ID格式");
                result["status"] = "error";
                result["message"] = "无效的用户ID格式";
            }
            catch (...) {
                logger.logError("Control", "rmuser 命令时发生未知错误");
                result["status"] = "error";
                result["message"] = "rmuser 命令时发生未知错误";
            }
        }
        else if (cmd == "help") {
            string helpText = "可用指令:\n"
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
                "  help - 显示此帮助信息";
            logger.logInfo("Control", helpText);
            result["status"] = "success";
            result["message"] = "获取帮助信息成功";
            result["data"]["helpText"] = helpText;
        }
        else {
            logger.logError("Control", "不合法的指令: " + command);
            result["status"] = "error";
            result["message"] = "不合法的指令: " + command;
        }
    }
    catch (const exception& e) {
        logger.logFatal("Control", "命令执行失败: " + string(e.what()));
        result["status"] = "error";
        result["message"] = "命令执行失败: " + string(e.what());
    }
    catch (...) {
        logger.logFatal("Control", "命令执行时发生未知错误");
        result["status"] = "error";
        result["message"] = "命令执行时发生未知错误";
    }

    return result;
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
        // 启动时加载配置（包括聊天室配置及服务器配置）
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