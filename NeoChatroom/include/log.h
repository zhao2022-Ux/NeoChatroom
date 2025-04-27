#ifndef LOGGER_H
#define LOGGER_H

#include <string>
#include <thread>
#include <mutex>
#include <queue>
#include <condition_variable>
#include "tool.h"  // 包含你的文件操作库

enum class LogLevel {
    INFO,
    WARNING,
    ERROR_,
    FATAL
};
const std::string LOGPATH = "./", LOGNAME = "log";
class Logger {
private:
    FILE_::file logfile;
    std::string logPath;  // 日志文件路径
    std::string logFilename;  // 日志文件名
    std::mutex mtx;  // 用于多线程同步
    std::condition_variable cv;  // 用于线程通知
    std::queue<std::string> logQueue;  // 日志队列
    bool stopLogging = false;  // 停止日志线程的标志
    std::thread logThread;  // 日志线程
    size_t logCount = 0;  // 待处理日志的数量

    // 将日志级别转换为字符串
    std::string logLevelToString(LogLevel level);

    // 生成日志信息字符串
    std::string createLogMessage(LogLevel level, const std::string& origin, const std::string& message);

    // 处理日志写入，确保多线程安全
    void writeLogToFile(const std::string& logMessage);

    // 日志线程函数
    void logThreadFunction();

    // 私有构造函数，确保外部不能直接创建 Logger 实例
    Logger(const std::string& path, const std::string& filename);

public:
    // 静态成员变量，确保全局只有一个 Logger 实例
    static Logger* instance;

    // 获取全局唯一的 Logger 实例
    static Logger& getInstance();

    // 启动日志线程
    void start();

    // 停止日志线程，等待所有日志处理完毕后退出
    void stop();

    // 设置新的日志路径
    void setLogPath(const std::string& newPath);

    // 设置新的日志文件名
    void setLogFilename(const std::string& newFilename);

    // 写入日志（信息级别）
    void logInfo(const std::string& origin, const std::string& message);

    // 写入日志（警告级别）
    void logWarning(const std::string& origin, const std::string& message);

    // 写入日志（错误级别）
    void logError(const std::string& origin, const std::string& message);

    // 写入日志（致命错误级别）
    void logFatal(const std::string& origin, const std::string& message);
};

// 获取全局唯一的 Logger 实例
//Logger& Logger::getInstance();

#endif // LOGGER_H
