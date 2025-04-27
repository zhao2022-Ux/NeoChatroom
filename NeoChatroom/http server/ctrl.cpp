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
    // Iterate through all possible chatrooms and add used ones to config
    for (int i = 0; i < MAXROOM; i++) {
        if (used[i]) {
            Json::Value roomObj;
            roomObj["id"] = i;
            // Assume each chatroom has a method getName() to return its current name.
            roomObj["name"] = room[i].gettittle();
            rooms.append(roomObj);
        }
    }
    root["rooms"] = rooms;
    ofstream ofs(CONFIG_FILE);
    if (ofs.is_open()) {
        ofs << root;
        ofs.close();
    }
}

void loadConfig() {
    ifstream ifs(CONFIG_FILE);
    if (!ifs.is_open()) {
        // Config file does not exist; nothing to load.
        return;
    }
    Json::Value root;
    ifs >> root;
    ifs.close();
    const Json::Value rooms = root["rooms"];
    for (const auto& roomObj : rooms) {
        int id = roomObj["id"].asInt();
        string name = roomObj["name"].asString();
        if (id >= 0 && id < MAXROOM) {
            // If the room is not yet created, try to create it.
            if (!used[id]) {
                int newId = addroom();
                // newId might not match id but we assume that order is not critical.
                if (newId < 0) {
                    // Failed to create chatroom even though config exists, so ignore.
                    continue;
                }
            }
            // Set (or reset) the room name from the config.
            editroom(id, name);
        }
    }
}

// -------------------------------------------------------------------
// Command runner functions: loop and execute commands.
// Configuration saving is triggered on create, delete, and rename commands.
// -------------------------------------------------------------------
void command_runner(string command, int roomid) {
    // Trim trailing spaces from command input.
    while (!command.empty() && command.back() == ' ') {
        command.pop_back();
    }

    Logger& logger = Logger::getInstance();
    Server& server = Server::getInstance(HOST);
    try {
        // Split command into the primary command and its arguments.
        string cmd = command;
        string args = "";
        size_t spacePos = command.find(' ');
        if (spacePos != string::npos) {
            cmd = command.substr(0, spacePos);
            args = command.substr(spacePos + 1);
            // Remove extra leading and trailing spaces from args.
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
        else if (command == "start") {  // Exact match for start
            logger.logInfo("Control", "服务器已开启");
            start_manager();
            start_loginSystem();
            manager::ReadUserData("./", manager::datafile);
            // Load chatroom configuration on startup.
            loadConfig();
            server.start();
        }
        else if (cmd == "create") {
            int newRoomId = addroom();
            if (newRoomId != -1) {
                // Automatically start the room after creation.
                if (room[newRoomId].start()) {
                    logger.logInfo("Control", "聊天室已创建并启动，ID: " + to_string(newRoomId));
                    saveConfig();  // Save config after creation.
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
                saveConfig();  // Save config after deletion.
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
                    logger.logInfo("Control", "聊天室类型已更新" + to_string(roomId) + "，新类型: " + to_string(neetype));
                    // (Optional) If type is part of the saved config, call saveConfig() here.
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
                    saveConfig();  // Save config after renaming.
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
        // Spawn a detached thread to handle each command.
        thread cmd_thread(command_runner, cmd, 0);
        cmd_thread.detach();
    }
}

void run() {
    // Initialize by creating a couple of default chatrooms.
    addroom(); // Create the first room.
    addroom(); // Create the second room.
    // Load configuration on startup.
    loadConfig();
    thread maint(command);
    maint.join();
    Logger& logger = Logger::getInstance();
    logger.logInfo("Control", "控制线程已结束");
    return;
}