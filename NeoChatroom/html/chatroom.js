const chatBox = document.getElementById("chatBox");
const messageInput = document.getElementById("messageInput");
const usernameDisplay = document.getElementById("usernameDisplay");
const loginButton = document.getElementById("loginButton");
const themeToggle = document.getElementById("themeToggle");
const notificationSelect = document.getElementById("notificationSelect");

let currentUsername = "";  // 保存当前用户名，用于判断消息归属
let notificationMode = notificationSelect.value; // 'none', 'mention', 'always'
const notifiedMessages = new Set(); // 用于保存已通知过的消息ID，避免重复提醒

// 当通知模式变更时，记录用户选择（无需提前申请通知权限）
notificationSelect.addEventListener('change', function () {
    notificationMode = this.value;
});

// 添加键盘事件监听器
messageInput.addEventListener('keydown', function (event) {
    // 如果按下的是回车键并且不是按住 shift 键时
    if (event.key === 'Enter' && !event.shiftKey) {
        event.preventDefault(); // 防止换行
        sendMessage(); // 触发发送消息
    }
});

// 获取 cookie 中的 clientid 和 uid
function getCookie(name) {
    const value = document.cookie;
    const parts = value.split(`; ${name}=`);
    if (parts.length === 2) return parts.pop().split(';').shift();
    return null;
}

const clientid = getCookie("clientid");
const uid = getCookie("uid");

// 获取当前服务器的基础 URL
const serverUrl = window.location.origin;

// Base64 编码函数
function encodeBase64(str) {
    const encoder = new TextEncoder();
    const uint8Array = encoder.encode(str);
    let binaryString = '';
    uint8Array.forEach(byte => {
        binaryString += String.fromCharCode(byte);
    });
    return btoa(binaryString);
}

// Base64 解码函数
function decodeBase64(base64Str) {
    const binaryString = atob(base64Str);
    const uint8Array = new Uint8Array(binaryString.length);
    for (let i = 0; i < binaryString.length; i++) {
        uint8Array[i] = binaryString.charCodeAt(i);
    }
    const decoder = new TextDecoder();
    return decoder.decode(uint8Array);
}

let lastRenderedTimestamp = 0;  // 用于存储最后渲染的时间戳

// 模拟任务栏闪烁：当页面不聚焦时，交替修改 document.title
function flashTaskbar() {
    // 若当前页面处于激活状态，则不需要闪烁
    if (document.hasFocus()) {
        return;
    }
    const originalTitle = document.title;
    let flashCount = 0;
    const flashInterval = setInterval(() => {
        document.title = document.title === "【新消息】" ? originalTitle : "【新消息】";
        flashCount++;
        // 闪烁持续约 5 秒后结束
        if (flashCount >= 10) {
            clearInterval(flashInterval);
            document.title = originalTitle;
        }
    }, 500);
}

async function fetchChatMessages() {
    try {
        const response = await fetch(`${serverUrl}/chat/messages`);
        if (response.ok) {
            const messages = await response.json();
            const isScrolledToBottom = chatBox.scrollHeight - chatBox.clientHeight <= chatBox.scrollTop + 1;
            const isTextSelected = window.getSelection().toString() !== '';

            // 若用户正在选择文本，暂不刷新消息
            if (isTextSelected) {
                return;
            }

            const previousScrollTop = chatBox.scrollTop;
            let newMessages = [];

            // 仅渲染时间戳大于 lastRenderedTimestamp 的消息
            messages.forEach(msg => {
                const msgTimestamp = new Date(msg.timestamp).getTime();
                if (msgTimestamp > lastRenderedTimestamp) {
                    msg.isNew = true;
                    lastRenderedTimestamp = msgTimestamp;
                }
                newMessages.push(msg);
            });

            chatBox.innerHTML = newMessages.map(msg => {
                let messageStyle = '';
                let messageContent = '';
                let background_color, textcolor;

                switch (msg.labei) {
                    case 'GM':
                        messageStyle = 'background-color: white; color: black;';
                        background_color = 'white'; textcolor = 'black';
                        break;
                    case 'U':
                        messageStyle = 'background-color: rgba(0, 204, 255, 0.10); color: black;';
                        background_color = 'rgba(0, 204, 255, 0.10)'; textcolor = 'black';
                        break;
                    case 'BAN':
                        messageStyle = 'background-color: black; color: black;';
                        background_color = 'black'; textcolor = 'black';
                        break;
                    default:
                        messageStyle = 'background-color: white; color: black;';
                        background_color = 'white'; textcolor = 'black';
                }

                // 深色模式下调整样式
                if (document.body.classList.contains("dark-mode")) {
                    if (msg.labei === 'GM') {
                        messageStyle = 'background-color: black; color: white;';
                        background_color = 'black'; textcolor = 'white';
                    } else if (msg.labei === 'U') {
                        messageStyle = 'background-color: rgba(0, 204, 255, 0.15); color: white;';
                        background_color = 'rgba(0, 204, 255, 0.15)'; textcolor = 'white';
                    } else if (msg.labei === 'BAN') {
                        messageStyle = 'background-color: white; color: white;';
                        background_color = 'white'; textcolor = 'white';
                    } else {
                        messageStyle = 'background-color: black; color: white;';
                        background_color = 'black'; textcolor = 'white';
                    }
                }

                // 解码消息内容
                const decodedMessage = decodeBase64(msg.message);
                const messageTime = new Date(msg.timestamp).toLocaleTimeString();
                const isOwnMessage = (msg.user === currentUsername);
                const messageClass = isOwnMessage ? 'message user' : 'message';

                if (msg.imageUrl) {
                    messageContent = `
        <div class="${messageClass} ${msg.isNew ? 'fade-in' : ''}" style="${messageStyle}; white-space: normal; word-wrap: break-word;">
            <div class="header" style="background-color: transparent;">
                <div class="username" style="color:${textcolor};">${msg.user}</div>
                <div class="timestamp" style="color:${textcolor};">${messageTime}</div>
            </div>
            <div class="image-message">
                 <br><img src="${msg.imageUrl}" alt="Image" style="max-width: 100%; height: auto; cursor: pointer;" onclick="toggleFullScreen(this)" /><br>
                ${decodedMessage}
            </div>
        </div>`;
                } else {
                    messageContent = `
        <div class="${messageClass} ${msg.isNew ? 'fade-in' : ''}" style="${messageStyle}; white-space: normal; word-wrap: break-word;">
            <div class="header" style="background-color: transparent;">
                <div class="username" style="color:${textcolor};">${msg.user}</div>
                <div class="timestamp" style="color:${textcolor};">${messageTime}</div>
            </div>
            <div class="message-body">
                 ${decodedMessage}
            </div>
        </div>`;
                }

                // 如果是新消息且未处理过，则根据开关设置触发提醒（任务栏图标闪烁）
                const msgId = `${msg.timestamp}_${msg.user}_${msg.message}`;
                if (msg.isNew && !notifiedMessages.has(msgId)) {
                    if (notificationMode === 'always' ||
                        (notificationMode === 'mention' && !isOwnMessage && decodedMessage.includes(currentUsername))) {
                        flashTaskbar();
                    }
                    notifiedMessages.add(msgId);
                }

                return messageContent;
            }).join('');

            // 恢复滚动位置
            if (!isScrolledToBottom) {
                chatBox.scrollTop = previousScrollTop;
            } else {
                chatBox.scrollTop = chatBox.scrollHeight;
            }
        } else {
            console.error("Error fetching chat messages:", response.statusText);
        }
    } catch (error) {
        console.error("Error fetching chat messages:", error);
    }
}

const imageInput = document.getElementById("imageInput");
const imagePreview = document.getElementById("imagePreview");

function selectImage() {
    imageInput.click();
}

imageInput.addEventListener('change', function () {
    const file = imageInput.files[0];
    if (file) {
        const imageUrl = URL.createObjectURL(file);
        imagePreview.innerHTML = `<img src="${imageUrl}" alt="Image preview" style="max-width: 100px; height: auto; margin-left: 10px;" />`;
    } else {
        imagePreview.innerHTML = '';
    }
});

async function uploadImage(file) {
    console.log("Uploading file: ", file);
    const formData = new FormData();
    formData.append('file', file);
    try {
        const response = await fetch(`${serverUrl}/chat/upload`, {
            method: 'POST',
            body: formData
        });
        if (response.ok) {
            const data = await response.json();
            return data.imageUrl;
        } else {
            console.error("Error uploading image:", response.statusText);
            return null;
        }
    } catch (error) {
        console.error("Error uploading image:", error);
        return null;
    }
}

async function sendMessage() {
    const messageText = messageInput.value.trim();
    const imageFile = imageInput.files[0];

    if (messageText === '' && !imageFile) {
        return;
    }

    let imageUrl = '';
    if (imageFile) {
        imageUrl = await uploadImage(imageFile);
    }

    const message = {
        message: encodeBase64(messageText),
        uid: uid,
        imageUrl: imageUrl,
        timestamp: new Date().toISOString()
    };

    try {
        const response = await fetch(`${serverUrl}/chat/messages`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(message)
        });

        if (response.ok) {
            messageInput.value = '';
            imageInput.value = '';
            imagePreview.innerHTML = '';
            await fetchChatMessages();
            chatBox.scrollTop = chatBox.scrollHeight;
        } else if (response.status === 400) {
            deleteCookie("clientid");
            deleteCookie("uid");
            window.location.href = "/login";
        } else {
            alert('消息发送失败');
            console.error("Error sending message:", await response.text());
        }
    } catch (error) {
        console.error("Error sending message:", error);
        window.location.href = "/login";
    }
}

function toggleFullScreen(imgElement) {
    const existingOverlay = document.getElementById("imageFullscreen");
    if (existingOverlay) {
        existingOverlay.remove();
    } else {
        const overlay = document.createElement("div");
        overlay.id = "imageFullscreen";
        overlay.style.position = "fixed";
        overlay.style.top = 0;
        overlay.style.left = 0;
        overlay.style.width = "100vw";
        overlay.style.height = "100vh";
        overlay.style.backgroundColor = "rgba(0, 0, 0, 0.8)";
        overlay.style.display = "flex";
        overlay.style.alignItems = "center";
        overlay.style.justifyContent = "center";
        overlay.style.zIndex = 9999;
        const fullImg = document.createElement("img");
        fullImg.src = imgElement.src;
        fullImg.style.maxWidth = "90%";
        fullImg.style.maxHeight = "90%";
        fullImg.style.boxShadow = "0 0 20px rgba(255,255,255,0.5)";
        overlay.appendChild(fullImg);
        overlay.addEventListener("click", function () {
            overlay.remove();
        });
        document.body.appendChild(overlay);
    }
}

async function fetchUsername() {
    try {
        const response = await fetch(`${serverUrl}/user/username?uid=${uid}`);
        if (response.ok) {
            const data = await response.json();
            currentUsername = data.username;
            usernameDisplay.textContent = `${currentUsername}`;
            loginButton.style.display = 'none';
        } else {
            console.error("Error fetching username:", response.statusText);
            loginButton.style.display = 'inline-block';
        }
    } catch (error) {
        console.error("Error fetching username:", error);
        loginButton.style.display = 'inline-block';
    }
}

function detectSystemTheme() {
    const systemDarkMode = window.matchMedia && window.matchMedia('(prefers-color-scheme: dark)').matches;
    if (systemDarkMode) {
        document.body.classList.add("dark-mode");
    } else {
        document.body.classList.remove("dark-mode");
    }
}

themeToggle.addEventListener("click", function () {
    document.body.classList.toggle("dark-mode");
    isUserChangingTheme = true;
});

let isUserChangingTheme = false;
detectSystemTheme();

window.matchMedia('(prefers-color-scheme: dark)').addEventListener('change', function (e) {
    if (!isUserChangingTheme) {
        if (e.matches) {
            document.body.classList.add("dark-mode");
        } else {
            document.body.classList.remove("dark-mode");
        }
    }
});

fetchChatMessages();
setInterval(fetchChatMessages, 500);
fetchUsername();


document.addEventListener('DOMContentLoaded', async () => {
    // 获取当前页面URL路径并提取聊天室ID
    function getChatroomIdFromPath() {
        const path = window.location.pathname; // 获取当前路径
        const match = path.match(/^\/chat(\d+)$/); // 匹配类似 /chat5 的路径
        if (match) {
            return parseInt(match[1], 10); // 提取并返回ID，转换为数字
        }
        return null; // 如果没有匹配到，返回null
    }

    // 检查用户是否在指定聊天室中
    async function isUserInChatroom(chatroomId) {
        try {
            const response = await fetch(`/list`, {
                method: 'GET',
                credentials: 'include'
            });
            if (!response.ok) throw new Error('无法获取聊天室列表，用户可能未登录。');

            const joinedRooms = await response.json(); // 假设返回的是一个聊天室数组
            return joinedRooms.some(room => parseInt(room.id, 10) === chatroomId); // 检查用户是否在聊天室中
        } catch (error) {
            console.error('检查用户是否在聊天室中出错:', error);
            return false; // 出错时默认返回false
        }
    }

    // 替换页面为404内容
    function replacePageWith404() {
        document.body.innerHTML = `
            <div style="text-align: center; margin-top: 50px;">
                <h1 style="font-size: 48px; color: #FF0000;">404</h1>
                <p style="font-size: 24px; color: #555;">你没有加入这个聊天室！</p>
            </div>
        `;
        document.title = '404 - 页面未找到'; // 更新页面标题
    }

    // 主逻辑
    const chatroomId = getChatroomIdFromPath();
    if (!chatroomId) {
        console.error('无法从路径中提取聊天室ID');
        replacePageWith404();
        return;
    }

    const isInChatroom = await isUserInChatroom(chatroomId);
    if (!isInChatroom) {
        replacePageWith404(); // 用户不在聊天室中，替换页面为404
    }
});


