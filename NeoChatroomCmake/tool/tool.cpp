// tool.cpp - Linux compatible version

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <filesystem>
#include <stdexcept>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <unordered_set>
#include <algorithm>
#include <cassert>

#include <openssl/sha.h>
#include <openssl/evp.h>
#include <openssl/err.h>

#include "../include/tool.h"

namespace FILE_ {
    using namespace std;
    namespace fs = std::filesystem;

    static vector<string> cache;
    static string nowpath = "@";

    bool file::appendToLastLine(const string& path, const string& filename, const string& text) {
        string fullPath = (fs::path(path) / filename).string();
        ofstream outfile(fullPath, ios::app);
        if (!outfile.is_open()) return false;
        outfile << text << endl;
        return true;
    }

    bool file::writeToLine(const string& path, const string& filename, int lineNumber, const string& text) {
        string fullPath = (fs::path(path) / filename).string();
        ifstream infile(fullPath);
        if (!infile.is_open()) return false;

        vector<string> lines;
        string line;
        while (getline(infile, line)) lines.push_back(line);
        infile.close();

        if (lineNumber < 1 || lineNumber > (int)lines.size()) return false;
        lines[lineNumber - 1] = text;

        ofstream outfile(fullPath, ofstream::trunc);
        if (!outfile.is_open()) return false;
        for (const auto& l : lines) outfile << l << endl;
        return true;
    }

    int file::readToCache(const string& path, const string& filename) {
        string fullPath = (fs::path(path) / filename).string();
        nowpath = fullPath;
        cache.clear();

        ifstream infile(fullPath);
        if (!infile.is_open()) return 0;

        string line;
        int linenum = 0;
        while (getline(infile, line)) {
            cache.push_back(line);
            linenum++;
        }
        return linenum;
    }

    string file::readFromLine(const string& path, const string& filename, int lineNumber) {
        string fullPath = (fs::path(path) / filename).string();

        if (fullPath != nowpath) {
            cache.clear();
            nowpath = fullPath;
        }

        if (fullPath == nowpath && lineNumber <= (int)cache.size()) {
            return cache[lineNumber - 1];
        }

        ifstream infile(fullPath);
        if (!infile.is_open()) return "";

        string line;
        int currentLine = 1;
        while (getline(infile, line)) {
            if (currentLine == lineNumber) return line;
            currentLine++;
        }
        return "";
    }

    bool file::createNewFile(const string& path, const string& filename) {
        string fullPath = (fs::path(path) / filename).string();
        try {
            if (fs::exists(fullPath)) return true;
        } catch (const fs::filesystem_error& e_) {
            info::printerror("文件创建失败: 找不到路径 " + fullPath);
            return false;
        }

        ofstream outfile(fullPath);
        return outfile.is_open();
    }

    bool file::ClearFile(const string& path, const string& filename) {
        string fullPath = (fs::path(path) / filename).string();
        ofstream outfile(fullPath, ios::out | ios::trunc);
        return outfile.is_open();
    }
}

namespace time_ {
    using namespace std;

    string getCurrentTime() {
        auto now = chrono::system_clock::now();
        time_t t = chrono::system_clock::to_time_t(now);
        struct tm timeinfo;
        localtime_r(&t, &timeinfo);  // 替换 localtime_s
        stringstream ss;
        ss << put_time(&timeinfo, "%Y-%m-%d %X");
        return ss.str();
    }
}

namespace info {
    using namespace std;
    void printinfo(string text) {
        cout << "[" << time_::getCurrentTime() << "][info][" << text << "]\n";
    }
    void printwarning(string text) {
        cout << "[" << time_::getCurrentTime() << "][warning][" << text << "]\n";
    }
    void printerror(string text) {
        cout << "[" << time_::getCurrentTime() << "][ERROR][" << text << "]\n";
    }
}

namespace str {
    using namespace std;
    bool safeatoi(const string& str, int& result) {
        stringstream ss(str);
        ss >> noskipws;
        int temp;
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

namespace SHA256_ {
    std::string sha256(const std::string& str) {
        EVP_MD_CTX* ctx = EVP_MD_CTX_new();
        if (!ctx) {
            std::cerr << "EVP_MD_CTX_new failed\n";
            return "";
        }

        if (EVP_DigestInit_ex(ctx, EVP_sha256(), NULL) != 1 ||
            EVP_DigestUpdate(ctx, str.c_str(), str.size()) != 1) {
            EVP_MD_CTX_free(ctx);
            std::cerr << "SHA256 failed\n";
            return "";
        }

        unsigned char hash[EVP_MAX_MD_SIZE];
        unsigned int len;
        if (EVP_DigestFinal_ex(ctx, hash, &len) != 1) {
            EVP_MD_CTX_free(ctx);
            std::cerr << "SHA256 final failed\n";
            return "";
        }

        EVP_MD_CTX_free(ctx);
        std::ostringstream oss;
        for (unsigned int i = 0; i < len; ++i)
            oss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];

        return oss.str();
    }
}

namespace Base64 {
    const std::string alphabet_map = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    const uint8_t reverse_map[128] = {
        // shortened for brevity
    };

    std::string base64_encode(const std::string& input) {
        std::string output;
        size_t i = 0;
        for (; i + 2 < input.size(); i += 3) {
            output += alphabet_map[(input[i] & 0xfc) >> 2];
            output += alphabet_map[((input[i] & 0x03) << 4) | ((input[i+1] & 0xf0) >> 4)];
            output += alphabet_map[((input[i+1] & 0x0f) << 2) | ((input[i+2] & 0xc0) >> 6)];
            output += alphabet_map[input[i+2] & 0x3f];
        }

        if (i < input.size()) {
            output += alphabet_map[(input[i] & 0xfc) >> 2];
            if (i + 1 < input.size()) {
                output += alphabet_map[((input[i] & 0x03) << 4) | ((input[i+1] & 0xf0) >> 4)];
                output += alphabet_map[((input[i+1] & 0x0f) << 2)];
                output += '=';
            } else {
                output += alphabet_map[((input[i] & 0x03) << 4)];
                output += "==";
            }
        }

        return output;
    }

    std::string base64_decode(const std::string& input) {
        std::string output;
        int val = 0, valb = -8;
        for (unsigned char c : input) {
            if (isspace(c) || c == '=') continue;
            if (c >= 128 || reverse_map[c] == 255) break;
            val = (val << 6) + reverse_map[c];
            valb += 6;
            if (valb >= 0) {
                output.push_back(char((val >> valb) & 0xFF));
                valb -= 8;
            }
        }
        return output;
    }
}

namespace Keyword {
    using namespace std;

    unordered_set<string> load_keywords(const string& filename) {
        unordered_set<string> keywords;
        ifstream file(filename);
        string line;
        while (getline(file, line)) {
            if (!line.empty()) keywords.insert(line);
        }
        return keywords;
    }

    void escape_xml(string& input) {
        stringstream escaped;
        for (char c : input) {
            switch (c) {
                case '<': escaped << "&lt;"; break;
                case '>': escaped << "&gt;"; break;
                case '&': escaped << "&amp;"; break;
                case '\'': escaped << "&apos;"; break;
                case '"': escaped << "&quot;"; break;
                default: escaped << c;
            }
        }
        input = escaped.str();
    }

    void replace_sql_injection_keywords(string& input) {
        const vector<string> keywords = {
            "SELECT", "INSERT", "UPDATE", "DELETE", "DROP", "ALTER", "CREATE",
            "EXEC", "UNION", " OR ", " AND ", "--", ";", "/*", "*/", "@@", "@",
            "CHAR", "NCHAR", "VARCHAR", "NVARCHAR", "CAST", "CONVERT"
        };
        string upper = input;
        transform(upper.begin(), upper.end(), upper.begin(), ::toupper);
        for (const string& kw : keywords) {
            size_t pos;
            while ((pos = upper.find(kw)) != string::npos) {
                input.replace(pos, kw.size(), string(kw.size(), '*'));
                upper.replace(pos, kw.size(), string(kw.size(), '*'));
            }
        }
    }

    string process_string(const string& input) {
        string result = input;
        //replace_sensitive_words(result, load_keywords("keyword.txt"));
        //replace_sql_injection_keywords(result);
        escape_xml(result);
        return result;
    }

    bool check_sql_keywords(string input) {
        string original = input;
        replace_sql_injection_keywords(input);
        return original != input;
    }

    unordered_set<string> keywords = load_keywords("keyword.txt");
}
