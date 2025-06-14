#ifndef LOGGER_H
#define LOGGER_H

#include <string>
#include <thread>
#include <mutex>
#include <queue>
#include <condition_variable>
#include "tool.h"

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
    std::string logPath;  // ��־�ļ�·��
    std::string logFilename;  // ��־�ļ���
    std::mutex mtx;  // ���ڶ��߳�ͬ��
    std::condition_variable cv;  // �����߳�֪ͨ
    std::queue<std::string> logQueue;  // ��־����
    bool stopLogging = false;  // ֹͣ��־�̵߳ı�־
    std::thread logThread;  // ��־�߳�
    size_t logCount = 0;  // ��������־������

    // ����־����ת��Ϊ�ַ���
    std::string logLevelToString(LogLevel level);

    // ������־��Ϣ�ַ���
    std::string createLogMessage(LogLevel level, const std::string& origin, const std::string& message);

    // ������־д�룬ȷ�����̰߳�ȫ
    void writeLogToFile(const std::string& logMessage);

    // ��־�̺߳���
    void logThreadFunction();

    // ˽�й��캯����ȷ���ⲿ����ֱ�Ӵ��� Logger ʵ��
    Logger(const std::string& path, const std::string& filename);

public:
    // ��̬��Ա������ȷ��ȫ��ֻ��һ�� Logger ʵ��
    static Logger* instance;

    // ��ȡȫ��Ψһ�� Logger ʵ��
    static Logger& getInstance();

    // ������־�߳�
    void start();

    // ֹͣ��־�̣߳��ȴ�������־������Ϻ��˳�
    void stop();

    // �����µ���־·��
    void setLogPath(const std::string& newPath);

    // �����µ���־�ļ���
    void setLogFilename(const std::string& newFilename);

    // д����־����Ϣ����
    void logInfo(const std::string& origin, const std::string& message);

    // д����־�����漶��
    void logWarning(const std::string& origin, const std::string& message);

    // д����־�����󼶱�
    void logError(const std::string& origin, const std::string& message);

    // д����־���������󼶱�
    void logFatal(const std::string& origin, const std::string& message);
};

// ��ȡȫ��Ψһ�� Logger ʵ��
//Logger& Logger::getInstance();

#endif // LOGGER_H
