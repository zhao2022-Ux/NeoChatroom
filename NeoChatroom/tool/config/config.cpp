#include<string>
#include<vector>
#include"../../include/log.h"
using namespace std;
#include "../../include/config.h"

//item-----------------
	string item::getname() {return name;}
	string item::getval() { return val; }
	int item::gettype() { return type; }
	string item::getstype() {
		if (type == 1) return "str";
		if (type == 2) return "item";
		if (type == 0) return "num";
		return "unkown";
	}
	void item::init() {
		type = -1;
		name = val = UKword;
		fa = -1;
		son.clear();
	}
	//清除子项
	void item::clearson() {
		son.clear();
	}
	item::item(){
		type = -1;
		name = val = UKword;
		fa = -1,cur = -1;
		son.clear();
	}
	item* item::itcur() {
		return this;
	}
	bool item::settype(string TypeString){
		if (TypeString == "num") type = 0;
		else if (TypeString == "str") type = 1;
		else if (TypeString == "item") type = 2;
		else {
			type = -1;
			return false;
		}
		return true;
	}
	//设置项目名，最大长度为50
	bool item::setname(string Name) {
		if (Name.length() > 50) {
			name = UKword;
			return false;
		}
		name = Name;
		return true;
	}
	//设置项目值，最大长度为1000
	bool item::setval(string Val) {
		if (Val.length() > 1000) {
			val = UKword;
			return false;
		}
		val = Val;
		return true;
	}
	//新增儿子，通过cur
	bool item::addson(item Son) {
		type = 2;
		Son.fa = cur;
		son.push_back(Son.cur);
		return true;
	}
	// 拷贝构造函数
	item::item(const item& other) : type(other.type), name(other.name), val(other.val), son(other.son), fa(other.fa), cur(other.cur) {}

	// 赋值运算符
	item& item:: operator=(const item& other) {
		if (this != &other) { // 检查自赋值
			type = other.type;
			name = other.name;
			val = other.val;
			son = other.son;
			fa = other.fa;
			cur = other.cur;
		}
		return *this;
	}
	vector<int> item::getson() {
		return son;
	}
//--------------item
#include "../../include/tool.h"
/*
格式:

"man" : "item": "3"
	"height" : "num" : "170"
	"name" : "str" : "hdkk"
	"girlfriend" : "item" : "1"
		"" : "str" : "jijuemei"
	

"labei" : "str" : "!:! !"! !;!"
*/

//config-----------------------
	//获取list
	vector<item> config::getlist() {
		return list;
	}


	void config::init(int size) {
		list.clear();
		list.reserve(size);
		vis.reserve(size);
	}
	config::config() {
		init(0);
	}
	//从目录指定行中读取单个物品
	item config::readitem(string path,string filename,int nowline) {
		string line = ConfigFile.readFromLine(path, filename, nowline);
		if (vis.size() <= nowline)
			vis.insert(vis.end(), vis.size() - nowline + 1, 0);
		vis[nowline] = true;
		item res;
		auto i = 0,flag = 0;
		string name, type, val;
		for (; i < (int)line.length(); i++) {
			if (line[i] == '\"')
				flag++;

			if(flag == 1)	name.push_back(line[i]);
			if (flag == 2) break;
		}
		while (i < (int)line.length()) {
			if (line[i] == ':')
				break;
			i++;
		}
		for (; i < (int)line.length(); i++) {
			if (line[i] == '\"')
				flag++;

			if (flag == 3)	type.push_back(line[i]);
			if (flag == 4) break;
		}
		name.erase(0, 1), type.erase(0, 1);
		res.setname(name), res.settype(type);

		while (i < (int)line.length()) {
			if (line[i] == ':')
				break;
			i++;
		}

		for (; i < (int)line.length(); i++) {
			if (line[i] == '\"')
				flag++;

			if (flag == 5)	val.push_back(line[i]);
			if (flag == 6) break;
		}
		if (flag != 6) {
			Logger& logger = Logger::getInstance();
			logger.logError ("config", "读取配置文件" + path + "/" + filename + "的第" + to_string(nowline) + "行时发生错误");
		}
		val.erase(0, 1);
		res.setval(val);
		res.cur = (int)list.size();
		list.push_back(res);
		if (res.gettype() == 2) {
			int sonnum;
			str::safeatoi(val.c_str(), sonnum);
			nowline++;
			for (int j = 1; j <= sonnum; j++, nowline++) {
				res.addson(readitem(path, filename, nowline));
			}
		}

		list[res.cur] = res;

		return res;
	}
	using namespace std;
#include<iostream>
	//解析整个文件
	void config::read(string path,string filename) {
		int linenum = ConfigFile.readToCache(path,filename);
		init(linenum+1);
		for (int i = 1; i <= linenum; i++) {
			if (i >= vis.size()) vis.insert(vis.end(), i - vis.size() + 1, 0);
			if (!vis[i]) readitem(path,filename,i);
			
		}
	}
	//写入单个物品
	void config::writeitem(string path, string filename,item goalitem,int deep) {
		if (goalitem.cur >= vis.size()) vis.insert(vis.end(), goalitem.cur - vis.size() + 1, 0);
		if (vis[goalitem.cur]) return;
		string goaltext = "\"" + goalitem.getname() + "\" : \"" + goalitem.getstype() + "\" : \"" + goalitem.getval() + "\"";
		string tabtext;
		for (int i = 0; i <= deep; i++) {
			tabtext += "    ";
		}
		ConfigFile.appendToLastLine(path, filename, tabtext + goaltext);

		if (goalitem.gettype() == 2) {
			int sonnum;
			str::safeatoi(goalitem.getval(), sonnum);
			for (int j = 0; j < (int)goalitem.son.size(); j++) {
				writeitem(path, filename, list[goalitem.son[j]],deep+1);
				vis[goalitem.son[j]] = true;
			}
		}
	}
	//写入整个项目
	void config::write(string path, string filename) {
		vis.clear();
		vis.reserve(list.size());
		vis.insert(vis.end(), (list.size()>0? list.size():1) - 1, 0);
		for (int i = 0; i < (int)list.size(); i++) {
			if (i >= vis.size()) vis.insert(vis.end(), i - vis.size() + 1, 0);

			if (vis[i]) continue;
			
			writeitem(path, filename, list[i],0);
			vis[i] = true;
		}
	}
//---------------config