#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <filesystem>
#include "../include/tool.h"
namespace FILE_ {
    using namespace std;
    namespace fs = std::filesystem;

        // 向指定路径下的文件的最新一行写入内容
        bool file::appendToLastLine(const string& path, const string& filename, const string& text) {
            // 拼接路径
            string fullPath = path + "/" + filename;

            // 以追加模式打开文件（会把内容追加到文件末尾）
            ofstream outfile(fullPath, ios::app);
            if (!outfile.is_open()) {
                return false;
            }

            // 写入新内容到文件末尾
            outfile << text << endl;
            outfile.close();

            return true;
        }

        // 向指定路径下的文件的指定行写入内容（效率较低不建议使用）
        bool file::writeToLine(const string& path, const string& filename, int lineNumber, const string& text) {
            // 拼接路径
            string fullPath = path + "/" + filename;

            ifstream infile(fullPath);
            if (!infile.is_open()) {
                return false;
            }

            vector<string> lines;
            string line;
            // 读取文件的每一行
            while (getline(infile, line)) {
                lines.push_back(line);
            }
            infile.close();

            // 如果指定的行号大于文件的总行数，返回false
            if (lineNumber < 1 || lineNumber > lines.size()) {
                return false;
            }

            // 更新指定的行
            lines[lineNumber - 1] = text;

            // 重新写入文件
            ofstream outfile(fullPath, ofstream::trunc);
            if (!outfile.is_open()) {
                return false;
            }

            // 将所有行写入文件
            for (const auto& l : lines) {
                outfile << l << endl;
            }
            outfile.close();

            return true;
        }


        vector<string> cache;
        string nowpath = "@";
        //将一个文件全部读取到缓存
        int file::readToCache(const string& path, const string& filename) {
            bool flag = false;
            // 拼接路径
            string fullPath = path + "/" + filename;
            nowpath = fullPath;
            ifstream infile(fullPath);
            if (!infile.is_open()) {
                return 0;
            }
            
            string line;
            int linenum = 0;
            // 逐行读取，直到找到指定的行
            while (getline(infile, line)) {
                cache.push_back(line);
                linenum++;
            }

            infile.close();
            return linenum;
        }

        // 读取指定路径下文件指定行的字符串
        string file::readFromLine(const string& path, const string& filename, int lineNumber) {
            bool flag = false;
            // 拼接路径
            string fullPath = path + "/" + filename;

            if (fullPath == nowpath) flag = true;//如果在缓存，直接用缓存
            else cache.clear();
            nowpath = fullPath;

            if (flag) {
                if (lineNumber > (int)cache.size()) return "";
                return cache[lineNumber - 1];
            }

            ifstream infile(fullPath);
            if (!infile.is_open()) {
                return "";
            }

            string line;
            int currentLine = 1;

            // 逐行读取，直到找到指定的行
            while (getline(infile, line)) {
                if (currentLine == lineNumber) {
                    infile.close();
                    return line;
                }
                currentLine++;
            }

            infile.close();
            return "";
        }

        // 在指定路径下新建一个文件（如果文件已经存在，则返回false）
        bool file::createNewFile(const string& path, const string& filename) {
            // 拼接路径
            string fullPath = path + "/" + filename;

            // 检查文件是否已经存在
            try {
                if (fs::exists(fullPath)) {
                    return true; // 文件已存在
                }
            }
            catch (const fs::filesystem_error e_) {
                info::printerror("文件创建失败:找不到指定的路径"+fullPath);
                return false;
            }


            // 创建新文件
            ofstream outfile(fullPath);
            if (!outfile.is_open()) {
                return false;
            }

            // 可以在新文件中写入初始内容，或者仅创建空文件
            outfile.close();

            return true;
        }
        //清空指定路径的文件
        bool file::ClearFile(const string& path, const string& filename) {
            // 拼接路径
            string fullPath = path + "/" + filename;

            // 打开文件（以写模式打开会清空文件内容）
            ofstream outfile(fullPath, ios::out | ios::trunc);
            if (!outfile.is_open()) {
                return false;
            }

            // 文件已被清空，关闭文件
            outfile.close();

            return true;
        }
    
}

namespace time_ {
    using namespace std;
    //获取当前的格式化时间
    string getCurrentTime() {
        auto now = chrono::system_clock::now();
        auto in_time_t = chrono::system_clock::to_time_t(now);
        struct tm timeinfo;
        localtime_s(&timeinfo, &in_time_t);

        stringstream ss;
        ss << put_time(&timeinfo, "%Y-%m-%d %X");  // 格式化时间
        return ss.str();
    }
}
namespace info {
    using namespace std;
    void printinfo(string infotext) {
        cout << "[" + time_::getCurrentTime() + "]" << "[info][" << infotext << "]" << endl;
    }
    void printwarning(string infotext) {
        cout << "[" + time_::getCurrentTime() + "]" << "[warning][" << infotext << "]" << endl;
    }
    void printerror(string infotext) {
        cout << "[" + time_::getCurrentTime() + "]" << "[ERROR][" << infotext << "]" << endl;
    }
}

namespace str {
    using namespace std;
    //safeatoi
    bool safeatoi(const std::string& str, int& result) {
        std::stringstream ss(str);
        ss >> std::noskipws;
        int temp;

        // 尝试从字符串流中读取整数
        if (ss >> temp) {
            char c;
            if (!(ss >> c && ss.fail() && !ss.eof())) {
                result = temp;
                return true;
            }
        }
        result = 0; 
        return false;
    }

    

}

namespace SHA256 {
#include<openssl/sha.h>
#include <openssl/evp.h>
#include <openssl/err.h>
    std::string sha256(const std::string& str) {
        // 创建一个 EVP_MD_CTX 上下文
        EVP_MD_CTX* mdctx = EVP_MD_CTX_new();
        if (mdctx == nullptr) {
            std::cerr << "EVP_MD_CTX_new failed" << std::endl;
            ERR_print_errors_fp(stderr);
            exit(EXIT_FAILURE);
        }

        // 初始化 SHA-256 摘要
        if (EVP_DigestInit_ex(mdctx, EVP_sha256(), nullptr) != 1) {
            std::cerr << "EVP_DigestInit_ex failed" << std::endl;
            ERR_print_errors_fp(stderr);
            EVP_MD_CTX_free(mdctx);
            exit(EXIT_FAILURE);
        }

        // 更新摘要上下文与数据
        if (EVP_DigestUpdate(mdctx, str.c_str(), str.size()) != 1) {
            std::cerr << "EVP_DigestUpdate failed" << std::endl;
            ERR_print_errors_fp(stderr);
            EVP_MD_CTX_free(mdctx);
            exit(EXIT_FAILURE);
        }

        // 获取摘要结果
        unsigned char hash[EVP_MAX_MD_SIZE];
        unsigned int hash_len;
        if (EVP_DigestFinal_ex(mdctx, hash, &hash_len) != 1) {
            std::cerr << "EVP_DigestFinal_ex failed" << std::endl;
            ERR_print_errors_fp(stderr);
            EVP_MD_CTX_free(mdctx);
            exit(EXIT_FAILURE);
        }

        EVP_MD_CTX_free(mdctx); // 释放上下文

        // 将哈希值转换为十六进制字符串
        std::string hex_string;
        hex_string.reserve(hash_len * 2); // 预留足够的空间
        for (unsigned int i = 0; i < hash_len; ++i) {
            char buf[3]; // 每个字节需要两个十六进制字符和一个空终止符（但这里我们不用空终止符）
            snprintf(buf, sizeof(buf), "%02x", hash[i]);
            hex_string.append(buf, 2); // 直接追加两个字符到字符串中
        }

        return hex_string;
    }
}
namespace Base64 {
#include<algorithm>

#include <cassert>
    static const std::string alphabet_map = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    static const uint8_t reverse_map[] = {
        255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
        255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
        255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 62, 255, 255, 255, 63,
        52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 255, 255, 255, 255, 255, 255,
        255, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14,
        15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 255, 255, 255, 255, 255,
        255, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
        41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 255, 255, 255, 255, 255
    };

    std::string base64_encode(const std::string& text) {
        std::string encode;
        uint32_t i, j = 0;

        // 按字节遍历字符串
        for (i = 0; i + 3 <= text.length(); i += 3) {
            encode.push_back(alphabet_map[(unsigned char)text[i] >> 2]);
            encode.push_back(alphabet_map[((unsigned char)text[i] << 4) & 0x30 | ((unsigned char)text[i + 1] >> 4)]);
            encode.push_back(alphabet_map[((unsigned char)text[i + 1] << 2) & 0x3C | ((unsigned char)text[i + 2] >> 6)]);
            encode.push_back(alphabet_map[(unsigned char)text[i + 2] & 0x3F]);
        }

        // 处理剩余的字节（不足3个字节）
        if (i < text.length()) {
            uint32_t tail = (int)text.length() - i;
            if (tail == 1) {
                encode.push_back(alphabet_map[(unsigned char)text[i] >> 2]);
                encode.push_back(alphabet_map[((unsigned char)text[i] << 4) & 0x30]);
                encode.append("==");
            }
            else if (tail == 2) {
                encode.push_back(alphabet_map[(unsigned char)text[i] >> 2]);
                encode.push_back(alphabet_map[((unsigned char)text[i] << 4) & 0x30 | ((unsigned char)text[i + 1] >> 4)]);
                encode.push_back(alphabet_map[((unsigned char)text[i + 1] << 2) & 0x3C]);
                encode.push_back('=');
            }
        }

        return encode;
    }

    std::string base64_decode(const std::string& code) {
        assert(code.length() % 4 == 0);

        std::string plain;
        uint32_t i, j = 0;
        uint8_t quad[4];

        for (i = 0; i < code.length(); i += 4) {
            for (uint32_t k = 0; k < 4; k++) {
                quad[k] = reverse_map[code[i + k]];
            }

            assert(quad[0] < 64 && quad[1] < 64);

            plain.push_back((quad[0] << 2) | (quad[1] >> 4));

            if (quad[2] >= 64) break;
            else if (quad[3] >= 64) {
                plain.push_back((quad[1] << 4) | (quad[2] >> 2));
                break;
            }
            else {
                plain.push_back((quad[1] << 4) | (quad[2] >> 2));
                plain.push_back((quad[2] << 6) | quad[3]);
            }
        }

        return plain;
    }
}
#include <unordered_set>
#include <fstream>
#include <string>


namespace Keyword {
#include <unordered_set>

    // 用于读取文件中所有的敏感词
    unordered_set<string> load_keywords(const string& filename) {
        unordered_set<string> keywords;
        ifstream file(filename);
        string line;
        while (getline(file, line)) {
            if (!line.empty()) {
                keywords.insert(line);
            }
        }
        return keywords;
    }

    // 替换字符串中的敏感词
    void replace_sensitive_words(string& input, const unordered_set<string>& keywords) {
        for (const auto& keyword : keywords) {
            size_t pos = 0;
            while ((pos = input.find(keyword, pos)) != string::npos) {
                input.replace(pos, keyword.length(), string(keyword.length(), '*'));
                pos += keyword.length();
            }
        }
    }

    // 转义字符串中的XML注入关键字
    void escape_xml(std::string& input) {
        std::stringstream escaped;  // 使用字符串流来累积处理过的字符
        for (char c : input) {
            switch (c) {
            case '<':
                escaped << "&lt;";
                break;
            case '>':
                escaped << "&gt;";
                break;
            case '&':
                escaped << "&amp;";
                break;
            case '\'':
                escaped << "&apos;";
                break;
            case '\"':
                escaped << "&quot;";
                break;
            default:
                escaped << c;  // 其他字符保持不变
            }
        }
        input = escaped.str();  // 将处理后的结果赋回原字符串
    }

    std::unordered_set<std::string> keywords = load_keywords("keyword.txt");
    // 处理字符串：替换敏感词并转义XML关键字
    string process_string(const string& input) {

        string result = input;

        // 替换敏感词
        replace_sensitive_words(result, keywords);

        // 转义XML注入关键字
        escape_xml(result);

        return result;
    }
}
