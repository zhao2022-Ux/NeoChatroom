#pragma once
#ifndef CONFIG_H
#define CONFIG_H
#include<string>
#include<vector>
#include"log.h"
using namespace std;
class config;
namespace manager {
	void WriteUserData(string path, string filename);
}
class item {
protected:
	const string UKword = "!unkown!", HDword = "!head!";
	int8_t type;
	string name;
	string val;
	vector<int> son;
	
	/*
	type: num str item
	*/
public:
	friend void manager::WriteUserData(string path, string filename);

	vector<int> getson();
	string getname();
	string getval();
	int gettype();
	string getstype();
	int fa, cur;
	void init();
	void clearson();
	item();
	item* itcur();
	bool settype(string TypeString);
	bool setname(string Name);
	bool setval(string Val);
	bool addson(item Son);

	// 拷贝构造函数
	item(const item& other);

	// 赋值运算符
	item& operator=(const item& other);
	friend class config;
	
};
#include "tool.h"
/*
格式:

"man" : "item": "3"
	"height" : "num" : "170"
	"name" : "str" : "hdkk"
	"girlfriend" : "item" : "1"
		"" : "str" : "jijuemei"


"labei" : "str" : "!:! !"! !;!"
*/

class config {
	FILE_::file ConfigFile;
	static const int maxn = 1000000;
	vector<item> list;
	vector<bool> vis;
	/*
	孩子们，这个傻逼vis设计有很tm大问题，如果下标越界了，在所有调用的地方加这一句话

	if (goalitem.cur >= vis.size()) vis.insert(vis.end(), goalitem.cur - vis.size() + 1, 0);
	*/
	//
//                       _oo0oo_
//                      o8888888o
//                      88" . "88
//                      (| -_- |)
//                      0\  =  /0
//                    ___/`---'\___
//                  .' \\|     |// '.
//                 / \\|||  :  |||// \
//                / _||||| -:- |||||- \
//               |   | \\\  -  /// |   |
//               | \_|  ''\---/''  |_/ |
//               \  .-\__  '-'  ___/-. /
//             ___'. .'  /--.--\  `. .'___
//          ."" '<  `.___\_<|>_/___.' >' "".
//         | | :  `- \`.;`\ _ /`;.`/ - ` : | |
//         \  \ `_.   \_ __\ /__ _/   .-` /  /
//     =====`-.____`.___ \_____/___.-`___.-'=====
//                       `=---='
//
//
//     ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
//               佛祖保佑         永无BUG
//
//
//

public:
	friend void manager::WriteUserData(string path, string filename);
	vector<item> getlist();
	config();
	void init(int size);
	item readitem(string path, string filename, int nowline);
	void read(string path, string filename);
	void writeitem(string path, string filename, item goalitem, int deep);
	void write(string path, string filename);


};

#endif