#ifndef FILE_H
#define FILE_H

#include <string>
#include <vector>
namespace FILE_ {
    using namespace std;
    class file {
    private:
        vector<string> cache;
        string nowpath;
    public:
        // 向指定路径下的文件的最新一行写入内容
        bool appendToLastLine(const string& path, const string& filename, const string& text);

        // 向指定路径下的文件的指定行写入内容（效率较低不建议使用）
        bool writeToLine(const string& path, const string& filename, int lineNumber, const string& text);

        //将一个文件全部读取到缓存
        int readToCache(const string& path, const string& filename);

        // 读取指定路径下文件指定行的字符串
        string readFromLine(const string& path, const string& filename, int lineNumber);

        // 在指定路径下新建一个文件（如果文件已经存在，则返回false）
        bool createNewFile(const string& path, const string& filename);


        //清空指定路径的文件
        bool ClearFile(const string& path, const string& filename);
    };
    
}
#include <chrono>
#include <sstream>
#include <iomanip>  // 需要这个头文件来使用 put_time

namespace time_ {
    std::string getCurrentTime();
}
namespace info {
    
    void printinfo(std::string infotext);//打印信息
    void printwarning(std::string infotext);//打印警告
    void printerror(std::string infotext);//打印错误
}
namespace str {
    using namespace std;
    bool safeatoi(const std::string& str, int& result);//安全地将stirng转为int

    //string DeleteSpical(string str);//删除string中的空格和转义字符
}
#include "config.h"

namespace SHA256 {
//#include<openssl/sha.h>
    std::string sha256(const std::string& str);

}

namespace Base64 {
#include <string>

    // Base64 encode function
    std::string base64_encode(const std::string& text);

    // Base64 decode function
    std::string base64_decode(const std::string& code);
}
#include <unordered_set>
namespace Keyword {
    using namespace std;
    string process_string(const string& input);
}



#endif // FILE_H
       