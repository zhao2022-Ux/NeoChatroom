#pragma once

#include "../include/Server.h"
#include <vector>
#include <string>
#include <ctime>
#include <deque>
#include "../../json/json.h"
#include "../include/datamanage.h"
#include "../include/tool.h"
using namespace std;
#include <filesystem>
#include "chatroom.h"

extern vector<chatroom> room;

extern bool used[MAXROOM];
int addroom();

void start_manager();

int editroom(int x, string Roomtittle = "");

int setroomtype(int x, int type);