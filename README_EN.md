[简体中文](./README.md) | English

> \[!IMPORTANT]
>
> ### Serious Warning
>
> * You must comply with the [GNU Affero General Public License (AGPL-3.0)](https://www.gnu.org/licenses/agpl-3.0.html).
> * Any modifications, adaptations, distributions, or derivative projects **must also use the AGPL-3.0 license** and **must include this project’s license and copyright**
> * **Commercial use or any profit-making activities are strictly prohibited.** Legal action will be taken if violations are found.
> * It is **forbidden to alter the original copyright information** in derivative projects (you may add your own contribution information).
> * Thank you for your respect and understanding.

<p>
<strong><h2>🌐Seve'chatroom</h2></strong>
A high-performance C++-based LAN chatroom | A lightweight, high-efficiency, native solution for LAN chat, specially designed for computer labs and intranet environments.
</p>

[![License](https://img.shields.io/github/license/Dreamersseve/NeoChatroom?style=flat-square)](https://github.com/Dreamersseve/NeoChatroom/blob/main/LICENSE)
[![Stars](https://img.shields.io/github/stars/Dreamersseve/NeoChatroom?style=flat-square)](https://github.com/Dreamersseve/NeoChatroom/stargazers)
[![Last Commit](https://img.shields.io/github/last-commit/Dreamersseve/NeoChatroom?style=flat-square)](https://github.com/Dreamersseve/NeoChatroom/commits/main)
[![Platform](https://img.shields.io/badge/platform-Windows-blue?style=flat-square)](#)

### 👀 Demo

![Image](https://github.com/user-attachments/assets/8d90d690-0c2c-48a5-8c22-ef9750a582b3)
![Image](https://github.com/user-attachments/assets/9f01c8dd-fc27-4351-bc14-3dd71993b0f2)
![Image](https://github.com/user-attachments/assets/4d8a3f60-7d4b-4cab-8373-5e97a9ddbdcd)
![Image](https://github.com/user-attachments/assets/b1763248-9065-46ed-9710-df4d67768065)
![Image](https://github.com/user-attachments/assets/1a730ceb-a76d-4f71-a0d4-e845f78dc14f)

[🌐Seve'chatroom](https://chatroom.seveoi.icu)

### 🎉 Features

* [x] Support for multiple concurrent chatrooms.

* [x] Chatrooms can be **locked/hidden/password-protected**.

* [x] Supports **Markdown** and **LaTeX** rendering in messages.

* [x] Image messaging and multi-line display support.

* [x] Backend provides admin panel for room management and data inspection.

* 🚀 **Ultra Lightweight**: Backend uses <7MB RAM; frontend requires <10kbps bandwidth.

* ⚡ **High Performance**: On i5-12400, 1000 concurrent users consume <3% CPU.

* 🖱️ **One-Click Deployment**: Runs directly on any Windows machine—no environment setup needed.

* 🔒 **Basic Security**: Supports HTTPS, basic injection protection, and password authentication.

* 🌍 **LAN-Oriented**: Built for intranet/computer lab use, avoiding public chatroom privacy risks.

### ⚙️ Quick Deployment

Download the latest binary from [GitHub Releases](https://github.com/Dreamersseve/NeoChatroom/releases) and run it.

### 📦 Deployment Instructions

#### 🐧 Build on Linux

##### 1️⃣ Install Dependencies

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

##### 2️⃣ Clone & Build

```bash
git clone https://github.com/Dreamersseve/NeoChatroom.git
cd NeoChatroom
cd NeoChatroomCmake
mkdir build && cd build
cmake ..
make -j$(nproc)
```

##### 3️⃣ Run

**Note: Make sure `html` and `config.json` are in the same directory; they will NOT be auto-generated.**

```bash
./NeoChatroom
```

---

#### 🪟 Build on Windows

##### 1️⃣ Install Tools

* [Visual Studio](https://visualstudio.microsoft.com/) (check **C++ CMake tools** during install)
* [CMake](https://cmake.org/download/) (recommend adding to PATH)
* [vcpkg](https://github.com/microsoft/vcpkg) (recommended for OpenSSL and SQLite3)

##### 2️⃣ Install Dependencies with vcpkg

```powershell
git clone https://github.com/microsoft/vcpkg.git
.\vcpkg\bootstrap-vcpkg.bat
.\vcpkg\vcpkg install openssl sqlite3
```

> Note the `vcpkg` path for later CMake config.

##### 3️⃣ Configure with CLion

Open the `NeoChatroomCmake` folder in CLion.

Load `CMakeLists.txt`

Add the following to CMake options:
`-DCMAKE_TOOLCHAIN_FILE=[vcpkg path]/scripts/buildsystems/vcpkg.cmake`

> Replace `[vcpkg path]` with your actual path, e.g., `C:/dev/vcpkg`.

Then build the project.

##### 4️⃣ Run the Program

You can find `NeoChatroom.exe` in the `build/Release` or `x64/Release` directory.
Double-click or run it from terminal:

**Note: Make sure `html` and `config.json` are in the same directory; they will NOT be auto-generated.**

```powershell
.\Release\NeoChatroom.exe
```

---

#### 🧪 Test Checklist

Make sure the following are present:

* `config.json` exists in the working directory
* `database.db` exists or can be auto-generated
* `html/` static directory is correctly placed

### 🛫️ Tech Stack | Architecture

* CMake
* C++17 Compiler
* OpenSSL
* SQLite3
* Git

- **Backend**: `C++` + `SQLite` + `cpp-httplib`, multithreaded, designed for high concurrency.
- **Frontend**: Pure Vanilla JavaScript + HTML + CSS; no dependencies, ultra-simple deployment.

### 🙏 Acknowledgements

Huge thanks to all contributors, testers, and feedback providers who supported the development of [Seve'chatroom](https://github.com/Dreamersseve/NeoChatroom).

Including (in alphabetical order):

* [@Dreamersseve](https://github.com/Dreamersseve) — Project Author
* [@zhao2022-Ux](https://github.com/zhao2022-Ux)
* Outstanding open-source libraries like [OpenSSL](https://www.openssl.org/), [SQLite](https://www.sqlite.org/)
* Every individual who offered advice, tested, or reported issues

If you have contributed or helped and are not listed here, please contact me—I will add you promptly.

## Star History | Contributors

[![Star History Chart](https://api.star-history.com/svg?repos=Dreamersseve/NeoChatroom\&type=Date)](https://star-history.com/#Dreamersseve/NeoChatroom&Date)
[![](https://contrib.rocks/image?repo=Dreamersseve/NeoChatroom)](https://github.com/Dreamersseve/NeoChatroom/graphs/contributors)

---
