#include "../include/tool.h"
#include "../include/config.h"
#include "../include/datamanage.h"
#include <map>
#include <mutex>

using namespace std;

namespace manager {

    std::mutex mtx;  // 用于保护共享数据的互斥锁

    //检查char是否安全
    bool SafeWord(char word) {
        if (('0' <= word && word <= '9')
            || ('a' <= word && word <= 'z')
            || ('A' <= word && word <= 'Z')
            || word == '_') return true;

        return false;
    }

    //检查用户名是否合法
    bool CheckUserName(string name) {
        if (name.length() > 50) return false;
        for (int i = 0; i < name.length(); i++) {
            if (!SafeWord(name[i])) return false;
        }
        return true;
    }

    //存储数据的文件的路径
    int usernum = 0;

    string user::getname() { return name; }
    string user::getcookie() { return cookie; }
    string user::getlabei() { return labei; }
    string user::getpassword() { return password; }


    void user::setcookie(string new_cookie) {
        lock_guard<mutex> lock(mtx); // 加锁保护共享数据
        cookie = new_cookie;
    }


    void user::ban() {
        name = banedName;
        labei = BanedLabei;
    }

    bool user::setname(string str) {
        if (!CheckUserName(str)) {
            return false;
        }
        name = str;
        return true;
    }

    // 这玩意返回的是TM int!!! int!!! int!!!
    int user::getuid() { return uid; }

    bool user::operator <(user x) {
        return uid < x.uid;
    }

    user::user(string name_, string password_, string cookie_, string labei_) {
        name = name_, password = password_, cookie = cookie_, labei = labei_;
        uid = 0;
    }

    void user::setuid(int value) {
        if (value != -1) {
            uid = value;
            usernum = max(value, usernum);
        }
        else uid = ++usernum;
    }

    map<int, user> userList;  //映射uid->用户
    map<string, int> nameList; //映射用户名->uid
    //向数据中新增用户
    bool AddUser(string name, string psw, string cookie, string labei, int uuid) {
        lock_guard<mutex> lock(mtx); // 加锁保护共享数据

        user newuser(name, psw, cookie, labei);
        if (nameList.find(name) != nameList.end()) return false; //重名

        newuser.setuid(uuid);
        nameList[newuser.getname()] = newuser.getuid();
        userList[newuser.getuid()] = newuser;
        return true;
    }

    //通过uid查找用户
    user* FindUser(int uid) {
        lock_guard<mutex> lock(mtx); // 加锁保护共享数据

        auto it = userList.find(uid);
        if (it != userList.end()) {
            return &(it->second);
        }
        return nullptr;
    }

    //通过id查找用户
    user* FindUser(string name) {
        //lock_guard<mutex> lock(mtx); // 加锁保护共享数据

        auto it = nameList.find(name);
        //mtx.unlock();
        if (it != nameList.end()) {
            return FindUser(nameList[name]);
        }
        return nullptr;
    }

    //移除用户
    bool RemoveUser(int uid) {
        lock_guard<mutex> lock(mtx); // 加锁保护共享数据

        user* duser = FindUser(uid);
        if (duser == nullptr) return false;
        duser->ban();
        return true;
    }

    //读取时的缓冲区
    config DataFile;
    std::vector<item> list;

    //打印异常
    void LogError(string path, string filename, int line) {
        Logger& logger = Logger::getInstance();
        logger.logError("config", "读取用户信息" + path + "/" + filename + " 行" + to_string(line) + "时发生错误");
    }

    //读取整个文件
    void ReadUserData(string path, string filename) {
       

        // 读取数据
        DataFile.read(path, filename);
        list = DataFile.getlist(); //读取并获取整个配置文件列表

        vector<user> users_to_add;  // 用于暂时存储需要添加的用户信息

        for (int i = 0; i < list.size(); i++) {
            item now = list[i];
            if (now.gettype() == 2) { // 找到根
                string name, uuid, cookie, labei, password;
                auto son = now.getson();
                if (son.size() != 4) {
                    LogError(path, filename, i);
                    continue;
                }

                // 解析配置文件
                if (list[son[0]].gettype() != 1 || list[son[0]].getname() != "name") {
                    LogError(path, filename, i + 1);
                    continue;
                }
                name = list[son[0]].getval();

                if (list[son[1]].gettype() != 1 || list[son[1]].getname() != "password") {
                    LogError(path, filename, i + 2);
                    continue;
                }
                password = list[son[1]].getval();

                if (list[son[2]].gettype() != 1 || list[son[2]].getname() != "cookie") {
                    LogError(path, filename, i + 3);
                    continue;
                }
                cookie = list[son[2]].getval();

                if (list[son[3]].gettype() != 1 || list[son[3]].getname() != "labei") {
                    LogError(path, filename, i + 4);
                    continue;
                }
                labei = list[son[3]].getval();

                uuid = now.getname();
                int uid;
                if (str::safeatoi(uuid, uid) == 0) {
                    LogError(path, filename, i);
                    continue;
                }

                // 将新用户信息存入临时容器
                users_to_add.push_back(user(name, password, cookie, labei));
                users_to_add.back().setuid(uid);  // 设置UID
            }
        }

        {
            // 现在再加锁并添加用户
            lock_guard<mutex> lock(mtx); // 加锁保护共享数据

            // 先清空所有数据
            nameList.clear();
            userList.clear();
        }

        
        for (auto& newuser : users_to_add) {
            AddUser(newuser.getname(), newuser.getpassword(), newuser.getcookie(), newuser.getlabei(), newuser.getuid());
        }
    }

    //保存当前用户列表
    void WriteUserData(string path, string filename) {
        lock_guard<mutex> lock(mtx); // 加锁保护共享数据

        DataFile.init(usernum * 5);
        for (int i = 0; i <= usernum; i++) { //枚举唯一的uuid
            if (userList.find(i) == userList.end()) continue; //避免缺失
            item now;
            user nowuser = userList[i]; //父项目
            now.cur = (int)DataFile.list.size(), now.type = 2, now.val = "4";
            now.name = to_string(nowuser.getuid());
            DataFile.list.push_back(now);
            item son;

            son.cur = (int)DataFile.list.size(), son.type = 1, son.val = nowuser.getname(), son.name = "name";
            DataFile.list.push_back(son);
            now.addson(DataFile.list.back());
            //解析每个子项

            son.cur = (int)DataFile.list.size(), son.type = 1, son.val = nowuser.getpassword(), son.name = "password";
            DataFile.list.push_back(son);
            now.addson(DataFile.list.back());

            son.cur = (int)DataFile.list.size(), son.type = 1, son.val = nowuser.getcookie(), son.name = "cookie";
            DataFile.list.push_back(son);
            now.addson(DataFile.list.back());

            son.cur = (int)DataFile.list.size(), son.type = 1, son.val = nowuser.getlabei(), son.name = "labei";
            DataFile.list.push_back(son);
            now.addson(DataFile.list.back());

            DataFile.list[now.cur] = now; //更新父项
        }
        DataFile.write(path, filename); //保存
    }
}
