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

using namespace std;

// 默认服务器配置
static std::string CURRENT_HOST = "0.0.0.0";
static int CURRENT_PORT = 80;

// Configuration file path
const string CONFIG_FILE = "./config.json";

// Forward declaration for login system starter
void start_loginSystem();

// Dummy implementation for UTF8 conversion (modify if needed)
std::string convertToUTF8(const std::string& input) {
    return input;
}

// -------------------------------------------------------------------
// Configuration file related functions using jsoncpp
// -------------------------------------------------------------------
void saveConfig() {
    Json::Value root;
    Json::Value rooms(Json::arrayValue);
    // 遍历所有已使用的聊天室，将信息写入配置文件
    for (int i = 0; i < MAXROOM; i++) {
        if (used[i]) {
            Json::Value roomObj;
            roomObj["id"] = i;
            // 假定聊天室有 gettittle() 方法获取当前名称
            roomObj["name"] = room[i].gettittle();
            // 保存聊天室密码（采用 GetPassword 获取）
            roomObj["password"] = room[i].GetPassword();
            rooms.append(roomObj);
        }
    }
    root["rooms"] = rooms;

    // 保存服务器配置：HOST 和 PORT
    root["server"]["host"] = CURRENT_HOST;
    root["server"]["port"] = CURRENT_PORT;

    ofstream ofs(CONFIG_FILE);
    if (ofs.is_open()) {
        ofs << root;
        ofs.close();
    }
}

void loadConfig() {
    ifstream ifs(CONFIG_FILE);
    if (!ifs.is_open()) {
        // 配置文件不存在，无需加载
        return;
    }
    Json::Value root;
    ifs >> root;
    ifs.close();

    // 先加载服务器配置（如果存在则更新全局变量并设置到服务器实例上）
    if (!root["server"].isNull()) {
        if (root["server"].isMember("host"))
            CURRENT_HOST = root["server"]["host"].asString();
        if (root["server"].isMember("port"))
            CURRENT_PORT = root["server"]["port"].asInt();

        // 获取服务器实例并设置HOST和PORT
        Server& server = Server::getInstance();
        server.setHOST(CURRENT_HOST);
        server.setPORT(CURRENT_PORT);
    }

    // 加载聊天室配置
    const Json::Value rooms = root["rooms"];
    for (const auto& roomObj : rooms) {
        int id = roomObj["id"].asInt();
        string name = roomObj["name"].asString();
        if (id >= 0 && id < MAXROOM) {
            // 如果该聊天室未创建，则尝试创建
            if (!used[id]) {
                int newId = addroom();
                while (newId < id) {
                    newId = addroom();
                }
            }
            // 根据配置设置聊天室名称
            editroom(id, name);
            // 若配置中包含密码字段，则设置聊天室密码
            if (roomObj.isMember("password")) {
                string pwd = roomObj["password"].asString();
                room[id].setPassword(pwd);
            }

            // 自动启动聊天室
            if (!room[id].start()) {
                Logger::getInstance().logError("Control", "无法启动聊天室，ID: " + to_string(id));
            }
        }
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

        if (command == "userdata save") {
            manager::WriteUserData("./", manager::datafile);
            logger.logInfo("Control", "数据已保存");
        }
        else if (command == "userdata load") {
            manager::ReadUserData("./", manager::datafile);
            logger.logInfo("Control", "数据已加载");
        }
        else if (command == "start") {  // 完整匹配 start 命令
            logger.logInfo("Control", "服务器已开启");
            start_manager();
            start_loginSystem();
            manager::ReadUserData("./", manager::datafile);
            // 加载配置，包括聊天室密码和服务器配置
            loadConfig();
            server.start();
        }
        else if (cmd == "create") {
            int newRoomId = addroom();
            if (newRoomId != -1) {
                // 自动启动新创建的聊天室
                if (room[newRoomId].start()) {
                    logger.logInfo("Control", "聊天室已创建并启动，ID: " + to_string(newRoomId));
                    saveConfig();  // 创建后保存配置
                }
                else {
                    logger.logError("Control", "聊天室已创建但无法启动，ID: " + to_string(newRoomId));
                }
            }
            else {
                logger.logError("Control", "无法创建聊天室，已达到最大数量");
            }
        }
        else if (cmd == "delete" && !args.empty()) {
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
        else if (cmd == "settype" && !args.empty()) {
            size_t spacePos = args.find(' ');
            if (spacePos != string::npos) {
                string idPart = args.substr(0, spacePos);
                int neetype = atoi(args.substr(spacePos + 1).c_str());
                int roomId = stoi(idPart);
                if (roomId >= 0 && roomId < MAXROOM && used[roomId]) {
                    setroomtype(roomId, neetype);
                    logger.logInfo("Control", "聊天室类型已更新 " + to_string(roomId) + "，新类型: " + to_string(neetype));
                    // （可选）如果类型需要保存，则调用 saveConfig() 
                }
                else {
                    logger.logError("Control", "无效的聊天室 ID: " + to_string(roomId));
                }
            }
            else {
                logger.logError("Control", "settype 命令格式错误，需要提供 ID 和新名称");
            }
        }
        else if (cmd == "rename" && !args.empty()) {
            size_t spacePos = args.find(' ');
            if (spacePos != string::npos) {
                string idPart = args.substr(0, spacePos);
                string newName = args.substr(spacePos + 1);
                int roomId = stoi(idPart);
                if (roomId >= 0 && roomId < MAXROOM && used[roomId]) {
                    editroom(roomId, newName);
                    logger.logInfo("Control", "聊天室已重命名，ID: " + to_string(roomId) + "，新名称: " + newName);
                    saveConfig();  // 重命名后保存配置
                }
                else {
                    logger.logError("Control", "无效的聊天室 ID: " + to_string(roomId));
                }
            }
            else {
                logger.logError("Control", "rename 命令格式错误，需要提供 ID 和新名称");
            }
        }
        else if (cmd == "stop") {
            manager::WriteUserData("./", manager::datafile);
            exit(0);
        }
        else if (cmd == "say" && !args.empty()) {
            size_t msgPos = args.find(' ');
            if (msgPos != string::npos) {
                int roomId = stoi(args.substr(0, msgPos));
                string message = args.substr(msgPos + 1);
                if (roomId >= 0 && roomId < MAXROOM && used[roomId]) {
                    string UTF8msg = message;
                    string GBKmsg = WordCode::Utf8ToGbk(UTF8msg.c_str());
                    room[roomId].systemMessage(convertToUTF8(GBKmsg));
                    logger.logInfo("Control", "消息已发送到聊天室 " + to_string(roomId));
                }
                else {
                    logger.logError("Control", "无效的聊天室 ID: " + to_string(roomId));
                }
            }
            else {
                logger.logError("Control", "命令格式错误: say <roomId> <message>");
            }
        }
        else if (cmd == "clear" && !args.empty()) {
            int roomId = stoi(args);
            if (roomId >= 0 && roomId < MAXROOM && used[roomId]) {
                room[roomId].clearMessage();
                logger.logInfo("Control", "聊天室 " + to_string(roomId) + " 的消息列表已清空");
            }
            else {
                logger.logError("Control", "无效的聊天室 ID: " + to_string(roomId));
            }
        }
        else if (cmd == "ban" && !args.empty()) {
            string ip = args;
            server.banIP(args);
        }
        else if (cmd == "deban" && !args.empty()) {
            string ip = args;
            server.debanIP(args);
        }
        else if (cmd == "setpassword" && !args.empty()) {
            size_t spacePos = args.find(' ');
            if (spacePos != string::npos) {
                string idPart = args.substr(0, spacePos);
                string password = args.substr(spacePos + 1);
                int roomId = stoi(idPart);
                if (roomId >= 0 && roomId < MAXROOM && used[roomId]) {
                    room[roomId].setPassword(password);
                    logger.logInfo("Control", "聊天室密码已设置，ID: " + to_string(roomId));
                    saveConfig();
                }
                else {
                    logger.logError("Control", "无效的聊天室 ID: " + to_string(roomId));
                }
            }
            else {
                logger.logError("Control", "setpassword 命令格式错误，需要提供 ID 和密码");
            }
        }
        else if (cmd == "listuser") {
            auto userDetails = manager::GetUserDetails();
            if (userDetails.empty()) {
                logger.logInfo("Control", "当前没有用户");
            } else {
                logger.logInfo("Control", "用户列表:");
                for (const auto& [name, password, uid] : userDetails) {
                    logger.logInfo("Control", "用户名: " + name + ", 密码: " + password + ", UID: " + to_string(uid));
                }
            }
        }
        else if (cmd == "listroom") {
            auto roomDetails = GetRoomListDetails(); 
            if (roomDetails.empty()) {
                logger.logInfo("Control", "当前没有聊天室");
            } else {
                logger.logInfo("Control", "聊天室列表:");
                for (const auto& [id, name, password] : roomDetails) {
                    logger.logInfo("Control", "ID: " + to_string(id) + ", 名称: " + name + ", 密码: " + password);
                }
            }
        }
        else if (cmd == "help") {
            logger.logInfo("Control", "可用指令:\n"
                "  userdata save - 保存用户数据\n"
                "  userdata load - 加载用户数据\n"
                "  start - 启动服务器\n"
                "  stop - 停止服务器\n"
                "  create - 创建并启动一个新的聊天室\n"
                "  delete <roomId> - 删除指定聊天室\n"
                "  rename <roomId> <name> - 为指定聊天室重命名\n"
                "  settype <roomId> <type> - 为指定聊天室更改类型 2-禁止访问 3-隐藏\n"
                "  say <roomId> <message> - 向指定聊天室发送消息\n"
                "  clear <roomId> - 清空指定聊天室的消息\n"
                "  ban <ip> - 封禁指定IP\n"
                "  deban <ip> - 解封指定IP\n"
                "  setpassword <roomId> <password> - 为指定聊天室设置密码\n"
                "  listuser - 列出所有用户\n"
                "  listroom - 列出所有聊天室\n"
                "  help - 显示此帮助信息");
                }
        else {
            logger.logError("Control", "不合法的指令: " + command);
        }
    }
    catch (const exception& e) {
        logger.logFatal("Control", "命令执行失败: " + string(e.what()));
    }
}

void command() {
    while (true) {
        string cmd;
        getline(cin, cmd);
        // 每条命令均在分离线程中处理
        thread cmd_thread(command_runner, cmd, 0);
        cmd_thread.detach();
    }
}

void run() {
    // 初始化：创建几个默认聊天室
    //addroom(); // 创建第一个聊天室
    //addroom(); // 创建第二个聊天室
    // 启动时加载配置（包括聊天室配置及服务器配置）
    loadConfig();
    thread maint(command);
    maint.join();
    Logger& logger = Logger::getInstance();
    logger.logInfo("Control", "控制线程已结束");
    return;
}
