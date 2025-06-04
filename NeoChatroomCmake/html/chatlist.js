
    // SHA-256 implementation
    function sha256(s) {
        const chrsz = 8
        const hexcase = 0

        function safe_add(x, y) {
            const lsw = (x & 0xFFFF) + (y & 0xFFFF)
            const msw = (x >> 16) + (y >> 16) + (lsw >> 16)
            return (msw << 16) | (lsw & 0xFFFF)
        }

        function S(X, n) {
            return (X >>> n) | (X << (32 - n))
        }

        function R(X, n) {
            return (X >>> n)
        }

        function Ch(x, y, z) {
            return ((x & y) ^ ((~x) & z))
        }

        function Maj(x, y, z) {
            return ((x & y) ^ (x & z) ^ (y & z))
        }

        function Sigma0256(x) {
            return (S(x, 2) ^ S(x, 13) ^ S(x, 22))
        }

        function Sigma1256(x) {
            return (S(x, 6) ^ S(x, 11) ^ S(x, 25))
        }

        function Gamma0256(x) {
            return (S(x, 7) ^ S(x, 18) ^ R(x, 3))
        }

        function Gamma1256(x) {
            return (S(x, 17) ^ S(x, 19) ^ R(x, 10))
        }

        function core_sha256(m, l) {
            const K = [0x428A2F98, 0x71374491, 0xB5C0FBCF, 0xE9B5DBA5, 0x3956C25B, 0x59F111F1, 0x923F82A4, 0xAB1C5ED5, 0xD807AA98, 0x12835B01, 0x243185BE, 0x550C7DC3, 0x72BE5D74, 0x80DEB1FE, 0x9BDC06A7, 0xC19BF174, 0xE49B69C1, 0xEFBE4786, 0xFC19DC6, 0x240CA1CC, 0x2DE92C6F, 0x4A7484AA, 0x5CB0A9DC, 0x76F988DA, 0x983E5152, 0xA831C66D, 0xB00327C8, 0xBF597FC7, 0xC6E00BF3, 0xD5A79147, 0x6CA6351, 0x14292967, 0x27B70A85, 0x2E1B2138, 0x4D2C6DFC, 0x53380D13, 0x650A7354, 0x766A0ABB, 0x81C2C92E, 0x92722C85, 0xA2BFE8A1, 0xA81A664B, 0xC24B8B70, 0xC76C51A3, 0xD192E819, 0xD6990624, 0xF40E3585, 0x106AA070, 0x19A4C116, 0x1E376C08, 0x2748774C, 0x34B0BCB5, 0x391C0CB3, 0x4ED8AA4A, 0x5B9CCA4F, 0x682E6FF3, 0x748F82EE, 0x78A5636F, 0x84C87814, 0x8CC70208, 0x90BEFFFA, 0xA4506CEB, 0xBEF9A3F7, 0xC67178F2]
            const HASH = [0x6A09E667, 0xBB67AE85, 0x3C6EF372, 0xA54FF53A, 0x510E527F, 0x9B05688C, 0x1F83D9AB, 0x5BE0CD19]
            const W = new Array(64)
            let a, b, c, d, e, f, g, h, i, j
            let T1, T2
            m[l >> 5] |= 0x80 << (24 - l % 32)
            m[((l + 64 >> 9) << 4) + 15] = l
            for (i = 0; i < m.length; i += 16) {
                a = HASH[0]
                b = HASH[1]
                c = HASH[2]
                d = HASH[3]
                e = HASH[4]
                f = HASH[5]
                g = HASH[6]
                h = HASH[7]
                for (j = 0; j < 64; j++) {
                    if (j < 16) {
                        W[j] = m[j + i]
                    } else {
                        W[j] = safe_add(safe_add(safe_add(Gamma1256(W[j - 2]), W[j - 7]), Gamma0256(W[j - 15])), W[j - 16])
                    }
                    T1 = safe_add(safe_add(safe_add(safe_add(h, Sigma1256(e)), Ch(e, f, g)), K[j]), W[j])
                    T2 = safe_add(Sigma0256(a), Maj(a, b, c))
                    h = g
                    g = f
                    f = e
                    e = safe_add(d, T1)
                    d = c
                    c = b
                    b = a
                    a = safe_add(T1, T2)
                }
                HASH[0] = safe_add(a, HASH[0])
                HASH[1] = safe_add(b, HASH[1])
                HASH[2] = safe_add(c, HASH[2])
                HASH[3] = safe_add(d, HASH[3])
                HASH[4] = safe_add(e, HASH[4])
                HASH[5] = safe_add(f, HASH[5])
                HASH[6] = safe_add(g, HASH[6])
                HASH[7] = safe_add(h, HASH[7])
            }
            return HASH
        }

        function str2binb(str) {
            const bin = []
            const mask = (1 << chrsz) - 1
            for (let i = 0; i < str.length * chrsz; i += chrsz) {
                bin[i >> 5] |= (str.charCodeAt(i / chrsz) & mask) << (24 - i % 32)
            }
            return bin
        }

        function Utf8Encode(string) {
            string = string.replace(/\r\n/g, '\n')
            let utfText = ''
            for (let n = 0; n < string.length; n++) {
                const c = string.charCodeAt(n)
                if (c < 128) {
                    utfText += String.fromCharCode(c)
                } else if ((c > 127) && (c < 2048)) {
                    utfText += String.fromCharCode((c >> 6) | 192)
                    utfText += String.fromCharCode((c & 63) | 128)
                } else {
                    utfText += String.fromCharCode((c >> 12) | 224)
                    utfText += String.fromCharCode(((c >> 6) & 63) | 128)
                    utfText += String.fromCharCode((c & 63) | 128)
                }
            }
            return utfText
        }

        function binb2hex(binarray) {
            const hex_tab = hexcase ? '0123456789ABCDEF' : '0123456789abcdef'
            let str = ''
            for (let i = 0; i < binarray.length * 4; i++) {
                str += hex_tab.charAt((binarray[i >> 2] >> ((3 - i % 4) * 8 + 4)) & 0xF) +
                    hex_tab.charAt((binarray[i >> 2] >> ((3 - i % 4) * 8)) & 0xF)
            }
            return str
        }
        s = Utf8Encode(s)
        return binb2hex(core_sha256(str2binb(s), s.length * chrsz))
    }

    // Show password prompt dialog
function typeinPassword() {
    return new Promise((resolve) => {
        // 创建密码对话框容器
        const passwordDialog = document.createElement('div');
        passwordDialog.style.cssText = `
            position: fixed;
            top: 0;
            left: 0;
            width: 100%;
            height: 100%;
            background: rgba(0, 0, 0, 0.5);
            display: flex;
            justify-content: center;
            align-items: center;
            z-index: 1000;
            opacity: 0;
            transition: opacity 0.3s ease;
        `;
        setTimeout(() => passwordDialog.style.opacity = 1, 10); // 淡入动画

        // 创建对话框内容区域
        const dialogContent = document.createElement('div');
        dialogContent.style.cssText = `
            background: white;
            padding: 20px;
            border-radius: 8px;
            box-shadow: 0 4px 10px rgba(0, 0, 0, 0.1);
            text-align: center;
            transform: scale(0.9);
            transition: transform 0.3s ease;
        `;
        setTimeout(() => dialogContent.style.transform = 'scale(1)', 10); // 缩放动画

        // 深色模式适配
        if (document.body.classList.contains('dark-mode')) {
            dialogContent.style.background = '#2a2a2a';
            dialogContent.style.color = '#fff';
        }

        // 创建密码输入框
        const passwordInput = document.createElement('input');
        passwordInput.type = 'password';
        passwordInput.placeholder = '请输入密码';
        passwordInput.style.cssText = `
            margin: 10px 0;
            padding: 8px;
            width: 200px;
            border: 1px solid #ddd;
            border-radius: 4px;
            transition: all 0.3s ease;
        `;
        if (document.body.classList.contains('dark-mode')) {
            passwordInput.style.background = '#1e1e1e';
            passwordInput.style.color = '#fff';
            passwordInput.style.borderColor = '#555';
        }
        passwordInput.addEventListener('focus', () => {
            passwordInput.style.borderColor = document.body.classList.contains('dark-mode') ? '#777' : '#007BFF';
        });
        passwordInput.addEventListener('blur', () => {
            passwordInput.style.borderColor = document.body.classList.contains('dark-mode') ? '#555' : '#ddd';
        });

        // 创建确认按钮
        const submitButton = document.createElement('button');
        submitButton.textContent = '确认';
        submitButton.style.cssText = `
            margin: 10px 5px;
            padding: 8px 16px;
            background: #007BFF;
            color: white;
            border: none;
            border-radius: 4px;
            cursor: pointer;
            transition: background-color 0.2s ease, transform 0.1s ease;
        `;
        submitButton.onmouseover = () => submitButton.style.backgroundColor = '#0056b3';
        submitButton.onmouseout = () => submitButton.style.backgroundColor = '#007BFF';
        submitButton.onclick = () => {
            const password = passwordInput.value;
            closeDialog(passwordDialog, resolve, password);
        };

        // 创建取消按钮
        const cancelButton = document.createElement('button');
        cancelButton.textContent = '取消';
        cancelButton.style.cssText = `
            margin: 10px 5px;
            padding: 8px 16px;
            background: #6c757d;
            color: white;
            border: none;
            border-radius: 4px;
            cursor: pointer;
            transition: background-color 0.2s ease, transform 0.1s ease;
        `;
        cancelButton.onmouseover = () => cancelButton.style.backgroundColor = '#5a6268';
        cancelButton.onmouseout = () => cancelButton.style.backgroundColor = '#6c757d';
        cancelButton.onclick = () => closeDialog(passwordDialog, resolve, null);

        // 将元素添加到对话框中
        dialogContent.appendChild(passwordInput);
        dialogContent.appendChild(document.createElement('br'));
        dialogContent.appendChild(submitButton);
        dialogContent.appendChild(cancelButton);
        passwordDialog.appendChild(dialogContent);

        // 添加到页面
        document.body.appendChild(passwordDialog);
        passwordInput.focus();

        // 支持按 Enter 键提交
        passwordInput.addEventListener('keypress', (e) => {
            if (e.key === 'Enter') {
                submitButton.click();
            }
        });
    });
}

// 关闭对话框的通用函数
function closeDialog(dialogElement, resolve, result) {
    dialogElement.style.opacity = 0; // 开始淡出动画
    setTimeout(() => {
        document.body.removeChild(dialogElement);
        resolve(result);
    }, 100); // 等待动画完成后再移除
}


    // 主题切换功能
    const themeToggle = document.getElementById('themeToggle');
    themeToggle.addEventListener('click', () => {
        document.body.classList.toggle('dark-mode');
        // 保存主题偏好
        const isDarkMode = document.body.classList.contains('dark-mode');
        localStorage.setItem('darkMode', isDarkMode);
    });

    // 检查保存的主题偏好
    if (localStorage.getItem('darkMode') === 'true') {
        document.body.classList.add('dark-mode');
    }

    // 获取 cookie 的工具函数
    function getCookie(name) {
        const value = "; " + document.cookie;
        const parts = value.split("; " + name + "=");
        if (parts.length === 2) return parts.pop().split(";").shift();
        return null;
    }

    // 删除 cookie 的工具函数
    function deleteCookie(name) {
        document.cookie = name + '=;expires=Thu, 01 Jan 1970 00:00:01 GMT;path=/;';
    }

    let currentUsername = "";
    const uid = getCookie("uid");
    const serverUrl = window.location.origin;

    // 更新登录状态
    async function updateLoginStatus() {
        try {
            const response = await fetch(`${serverUrl}/user/username?uid=${uid}`);
            if (response.ok) {
                const data = await response.json();
                currentUsername = data.username;
                document.getElementById('usernameDisplay').textContent = currentUsername;
                document.getElementById('loginButton').style.display = 'none';
                return true;
            } else {
                document.getElementById('loginButton').style.display = 'inline-block';
                currentUsername = '';
                document.getElementById('usernameDisplay').textContent = '';
                return false;
            }
        } catch (error) {
            document.getElementById('loginButton').style.display = 'inline-block';
            currentUsername = '';
            document.getElementById('usernameDisplay').textContent = '';
            return false;
        }
    }

    // 检查登录状态
    async function checkLoginStatus() {
        try {
            const response = await fetch(`${serverUrl}/user/username?uid=${uid}`);
            if (!response.ok) {
                window.location.href = '/login';
                return false;
            }
            return true;
        } catch (error) {
            window.location.href = '/login';
            return false;
        }
    }

    // 调用 /list 接口获取已加入的聊天室
    async function fetchJoinedChatrooms() {
        if (!await checkLoginStatus()) return;

        try {
            const response = await fetch('/list', {
                method: 'GET',
                credentials: 'include',
            });

            if (!response.ok) {
                window.location.href = '/login';
                return;
            }

            const rooms = await response.json();
            renderJoinedChatrooms(rooms);
        } catch (error) {
            console.error('获取已加入聊天室失败:', error);
            window.location.href = '/login';
        }
    }

    function renderJoinedChatrooms(rooms) {
        const container = document.getElementById('joined-chatrooms');
        container.innerHTML = '';

        if (!rooms || rooms.length === 0) {
            container.innerHTML = '<p class="empty-message">当前没有加入的聊天室。</p>';
            return;
        }

        rooms.forEach(room => {
            if (parseInt(room.id) < 1) return;  // 只显示 ID≥1

            const card = document.createElement('div');
            card.className = 'room-card';
            const roomName = room.name.trim() ? room.name : '无名称的聊天室';

            card.innerHTML = `
                <span class="room-id">ID: ${room.id}</span>
                <span class="room-name">${roomName}</span>
                <button class="quit-btn" title="退出聊天室">退出</button>
            `;

            // Add quit button handler
            const quitBtn = card.querySelector('.quit-btn');
            quitBtn.addEventListener('click', async (e) => {
                e.stopPropagation(); // Prevent card click event
                if (confirm(`确定要退出聊天室 "${roomName}" 吗？`)) {
                    await quitRoom(room.id, roomName);
                }
            });

            // Add card click handler for navigation
            card.addEventListener('click', (e) => {
                if (e.target !== quitBtn) { // Only navigate if not clicking quit button
                    window.location.href = `/chat${room.id}`;
                }
            });

            container.appendChild(card);
        });
    }

    // 调用 /allchatlist 接口获取所有聊天室
    async function fetchAllChatrooms() {
        if (!await checkLoginStatus()) return;

        try {
            const response = await fetch('/allchatlist', {
                method: 'GET',
                credentials: 'include',
            });

            if (!response.ok) {
                throw new Error('获取聊天室列表失败');
            }

            const rooms = await response.json();
            renderAllChatrooms(rooms);
        } catch (error) {
            console.error('获取所有聊天室失败:', error);
            window.location.href = '/login';
        }
    }

function renderAllChatrooms(rooms) {
    const container = document.getElementById('all-chatrooms');
    container.innerHTML = '';

    if (!rooms || rooms.length === 0) {
        container.innerHTML = '<p class="empty-message">当前没有可用的聊天室。</p>';
        return;
    }

    rooms.forEach(room => {
        if (parseInt(room.id) < 1) return;  // 过滤 ID<1

        // 根据 flags 判断是否隐藏或禁止
        const isHidden = (room.flags & 0x01) !== 0;
        const isForbidden = (room.flags & 0x02) !== 0;

        if (isHidden || isForbidden) return; // 跳过隐藏或禁止的聊天室

        const card = document.createElement('div');
        card.className = 'room-card';
        const roomName = room.name.trim() ? room.name : '无名称的聊天室';

        card.innerHTML = `
            <span class="room-id">ID: ${room.id}</span>
            <span class="room-name">${roomName}</span>
            <div class="login-overlay">
                <button class="join-btn">加入</button>
            </div>
        `;

        // 添加加入按钮功能
        const joinBtn = card.querySelector('.join-btn');
        joinBtn.addEventListener('click', async (e) => {
            e.stopPropagation();
            if (currentUsername) {
                await joinRoom(room.id, room.name);
            } else {
                window.location.href = '/login';
            }
        });

        container.appendChild(card);
    });
}


    async function showPasswordPrompt() {
            const password = prompt("请输入聊天室密码：");
            return password !== null ? password.trim() : null;
        }

    // 通用的加入聊天室函数，通过 /addroom 路由执行加入操作
    async function joinRoom(roomId, roomName) {
        if (!await checkLoginStatus()) return;

        try {
            let passwordHash = sha256("");
            let needRetry = false;
            let response;

            do {
                const url = `/joinquitroom?roomId=${roomId}&operation=join` + (passwordHash ? `&passwordHash=${passwordHash}` : '');
                response = await fetch(url, {
                    method: 'GET',
                    credentials: 'include',
                });

                if (response.status === 403) {
                    const errorText = await response.text();
                    if (errorText.trim() === "Password mismatch") {
                        const password = await showPasswordPrompt();
                        if (password === null) {
                            return;
                        }
                        passwordHash = sha256(password);
                        needRetry = true;
                        continue;
                    } else {
                        throw new Error("禁止访问: " + errorText);
                    }
                } else if (!response.ok) {
                    const errorText = await response.text();
                    throw new Error(errorText);
                }

                alert(`成功加入聊天室 ${roomName.trim() ? roomName : '无名称的聊天室'}`);
                await fetchJoinedChatrooms();
                return;
            } while (needRetry);

        } catch (error) {
            console.error('加入聊天室时出现错误:', error);
            alert('加入聊天室时出现错误: ' + error.message);
        }
    }

    // Add new quitRoom function
    async function quitRoom(roomId, roomName) {
        if (!await checkLoginStatus()) return;

        try {
            const response = await fetch(`/joinquitroom?roomId=${roomId}&operation=quit`, {
                method: 'GET',
                credentials: 'include'
            });

            if (!response.ok) {
                const errorText = await response.text();
                throw new Error(errorText);
            }

            alert(`已退出聊天室 ${roomName.trim() ? roomName : '无名称的聊天室'}`);
            await fetchJoinedChatrooms();
        } catch (error) {
            console.error('退出聊天室时出现错误:', error);
            alert('退出聊天室时出现错误: ' + error.message);
        }
    }

    // 处理"指定加入"面板的提交
    function setupJoinPanel() {
        const joinBtn = document.getElementById('join-room-btn');
        joinBtn.addEventListener('click', async () => {
            const input = document.getElementById('join-room-input');
            const roomId = parseInt(input.value.trim());

            if (isNaN(roomId) || roomId < 1) {
                alert('请输入有效的聊天室ID (≥1)');
                return;
            }

            if (!await checkLoginStatus()) return;

            await joinRoom(roomId, '');
            input.value = '';
        });
    }

    // 页面加载完毕后拉取数据、设置加入面板并更新登录状态
    document.addEventListener('DOMContentLoaded', async () => {
        if (!await checkLoginStatus()) return;

        await updateLoginStatus();
        await fetchJoinedChatrooms();
        await fetchAllChatrooms();
        setupJoinPanel();
    });