
# 🌐 seve'chatroom
C++高性能网络聊天室

VS2022编写的sln版截至到v0.1.2，请使用cmake构建。

## how to build?

windows用户可以直接下载Release

---

#### 📦 项目依赖

* CMake ≥ 3.14
* C++17 编译器（如 g++, clang++, MSVC）
* OpenSSL 开发库
* SQLite3 开发库
* Git（可选）

---

#### 🐧 Linux 下构建指南

##### 1️⃣ 安装依赖

Debian/Ubuntu:

```bash
sudo apt update
sudo apt install build-essential cmake libssl-dev libsqlite3-dev
```

Fedora/RHEL:

```bash
sudo dnf install gcc-c++ cmake openssl-devel sqlite-devel
```

Arch:

```bash
sudo pacman -S base-devel cmake openssl sqlite
```

##### 2️⃣ 克隆项目并构建

```bash
git clone https://github.com/Dreamersseve/NeoChatroom.git
cd NeoChatroom
cd NeoChatroomCmake
mkdir build && cd build
cmake ..
make -j$(nproc)
```

##### 3️⃣ 运行程序

**注意，请将html和config.json放在同目录下，程序不会自动生成**
```bash
./NeoChatroom
```

---

#### 🪟 Windows 下构建指南

##### 1️⃣ 安装工具

* [Visual Studio](https://visualstudio.microsoft.com/)（需要勾选 **C++ CMake 工具集**）
* [CMake](https://cmake.org/download/)（建议安装并添加到 PATH）
* [vcpkg](https://github.com/microsoft/vcpkg)（推荐用于管理 OpenSSL 和 SQLite3）

##### 2️⃣ 使用 vcpkg 安装依赖

```powershell
git clone https://github.com/microsoft/vcpkg.git
.\vcpkg\bootstrap-vcpkg.bat
.\vcpkg\vcpkg install openssl sqlite3
```

> 记下 `vcpkg` 路径，稍后 CMake 配置时需要用到。

##### 3️⃣ 使用 CLion 配置项目

在Clion中打开NeoChatroomCmake文件夹

加载Cmake List.txt

（注意添加-DCMAKE_TOOLCHAIN_FILE=[vcpkg路径]/scripts/buildsystems/vcpkg.cmake)作为cmake构建选项

> 请替换 `[vcpkg路径]` 为你的实际路径，比如：`C:/dev/vcpkg`。

然后进行构建
##### 4️⃣ 运行程序


你可以在 `build/Release` 或 `x64/Release` 目录下找到 `NeoChatroom.exe`，双击运行或在终端中执行：
**注意，请将html和config.json放在同目录下，程序不会自动生成**

```powershell
.\Release\NeoChatroom.exe
```

---

#### 🧪 测试运行

确认如下内容：

* `config.json` 存在于运行目录
* `database.db` 存在或可自动生成
* `html/` 静态文件目录正确

---

#### 📎 附加说明

* 编译输出的 SSL 证书 (`server.crt`, `server.key`) 可用于 HTTPS 支持
* 如果你遇到找不到依赖库，使用 `cmake-gui` 或确保系统库路径配置正确




[![License](https://img.shields.io/github/license/Dreamersseve/NeoChatroom?style=flat-square)](https://github.com/Dreamersseve/NeoChatroom/blob/main/LICENSE)
[![Stars](https://img.shields.io/github/stars/Dreamersseve/NeoChatroom?style=flat-square)](https://github.com/Dreamersseve/NeoChatroom/stargazers)
[![Last Commit](https://img.shields.io/github/last-commit/Dreamersseve/NeoChatroom?style=flat-square)](https://github.com/Dreamersseve/NeoChatroom/commits/main)
[![Platform](https://img.shields.io/badge/platform-Windows-blue?style=flat-square)](#)

> 一个高性能、轻量级、原生实现的局域网聊天室解决方案，专为教学机房及内网环境设计。

---

## ✨ 功能简介

![](https://cdn.luogu.com.cn/upload/image_hosting/b65eyz4w.png)
![](https://cdn.luogu.com.cn/upload/image_hosting/6qt8dkg7.png)
![](https://cdn.luogu.com.cn/upload/image_hosting/pc5t3u9r.png)
![](https://cdn.luogu.com.cn/upload/image_hosting/7s4jjvk1.png)
![](https://cdn.luogu.com.cn/upload/image_hosting/xqs01vxa.png)

- 支持多个聊天室并发运行
- 聊天室可设置**锁定/隐藏/密码保护**
- 消息支持 **Markdown** 与 **LaTeX** 渲染
- 支持图片发送与多行消息显示
- 后端提供管理界面，可进行房间控制与数据查看

---

##  技术架构

- **后端**：`C++` + `sqlite` + `cpp-httplib`，多线程设计，支持高并发
- **前端**：纯原生 JavaScript + HTML + CSS，无依赖、极简部署

---

##  特点

- 🚀 **极致轻量**：后端内存占用 < 7MB，前端带宽需求低于 10kbps
- ⚡ **高性能**：i5-12400 实测下，1000 并发用户仅消耗 <3% CPU
- 🖱️ **一键部署**：可在任何一台 Windows 电脑中直接运行，无需配置环境
- 🔒 **基础安全性**：支持 HTTPS、基础注入防护、密码验证
- 🌍 **局域网适配**：为机房/内网通信量身打造，避免使用公网聊天室带来的隐私风险


