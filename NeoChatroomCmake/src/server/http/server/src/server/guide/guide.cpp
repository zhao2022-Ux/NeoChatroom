#include "../../../../../../lib/httplib.h"
#include "json/json.h"
#include "../Server.h"
#include "../../data/datamanage.h"
#include "../../tool/log.h"
using namespace std;
//检查登录
string login_sucess(string name, string pwd ,manager::user& nowuser) {
    auto userptr = manager::FindUser(name);
    if (userptr == nullptr) return "USER_NOT_FOUND";

    //cout << nowuser.getpassword() << " " << pwd << endl;
    manager::user Rawuser = *userptr;

    if (Rawuser.getpassword() == pwd) {
        
        nowuser = *userptr;
        
        return "ACCEPT";
    }
    return "WRONG_PIASSWORD";
}
//检查重名
bool check_uid_same(string name) {
    if (manager::FindUser(name) == nullptr) return true;
    else return false;
}
//处理客户端登录请求
auto login = [](const httplib::Request& req, httplib::Response& res, const Json::Value& jsonData) {

    res.set_header("Access-Control-Allow-Origin", "*"); // 允许所有来源访问
    res.set_header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE"); // 允许的 HTTP 方法
    res.set_header("Access-Control-Allow-Headers", "Content-Type"); // 允许的头部字段


    std::string username = jsonData["username"].asString();
    std::string password = jsonData["hashedPassword"].asString();
    manager::user nowuser;
    std::string Flagtext = login_sucess(username, password, nowuser);
    //chk(username);
    Logger& logger = Logger::getInstance();


    if (Flagtext == "ACCEPT") {
        res.status = 200;
        res.set_content("Login successful", "text/plain");

        Server& server = Server::getInstance();
        server.setToken(res, to_string(nowuser.getuid()), password);
        //cout << password << endl;
        logger.logInfo("LoginSys", req.remote_addr + " " + username + "-" + password + " 成功登录:");
    }
    else {
        res.status = 401;
        res.set_content(Flagtext, "text/plain");
        logger.logInfo("LoginSys", req.remote_addr + " " + username + "-" + password + " 在试图登录时 客户端错误:" + Flagtext);
    }

};
//处理注册请求，默认前端接口数据格式绝对合法
auto Register = [](const httplib::Request& req, httplib::Response& res, const Json::Value& jsonData) {

    res.set_header("Access-Control-Allow-Origin", "*"); // 允许所有来源访问
    res.set_header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE"); // 允许的 HTTP 方法
    res.set_header("Access-Control-Allow-Headers", "Content-Type"); // 允许的头部字段


    std::string username = jsonData["username"].asString();
    std::string password = jsonData["password"].asString();
    manager::user newuser;

    //std::cout << username << " " << password << std::endl;

    Logger& logger = Logger::getInstance();
    if(check_uid_same(username) == true){
        if (!manager::AddUser(username, password, "", manager::Usuallabei)) {
            res.status = 500;//服务器无法完成请求
            res.set_content("unkown error", "text/plain");
            logger.logError("LoginSys", req.remote_addr + " 在试图以 " + username + "注册时发生未知错误");
        }
        else {
            res.set_content("Register successful", "text/plain");

            logger.logInfo("LoginSys", req.remote_addr + " " + username + "-" + password + " 成功注册");
        }
    }
    else {
        res.status = 403;
        res.set_content("samed name", "text/plain");

        logger.logInfo("LoginSys", req.remote_addr + " 在试图以 " + username + "注册时出现了用户名重名");
    }

};
//#include "../html/"
void start_loginSystem() {


        Server& server = Server::getInstance(HOST);


        server.serveFile("/login", "html/login.html");
        server.handlePostRequest("/register", Register);
        server.handlePostRequest("/login", login);

        //server.start();

}
/*
Server& server = Server::getInstance("127.0.0.1");

    server.serveFile("/login", "login.html");

    server.handlePostRequest("/login", login);

    server.handlePostRequest("/register", [](const httplib::Request& req, httplib::Response& res, const Json::Value& jsonData) {
        std::string username = jsonData["username"].asString();
        std::string password = jsonData["password"].asString();

        // 在这里，你可以进行注册操作
        // 例如，将用户信息存储到数据库中
        res.set_content("Registration successful", "text/plain");
        });
    server.start();
*/