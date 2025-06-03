
# 🌐 seve'chatroom
C++高性能网络聊天室


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


