const chatBox = document.getElementById("chatBox");
const messageInput = document.getElementById("messageInput");
const usernameDisplay = document.getElementById("usernameDisplay");
const loginButton = document.getElementById("loginButton");
const themeToggle = document.getElementById("themeToggle");
const notificationSelect = document.getElementById("notificationSelect");

let currentUsername = "";
let notificationMode = notificationSelect.value;
const notifiedMessages = new Set();
let lastRenderedTimestamp = 0;
let isPollingActive = true;

// 使用WeakMap存储DOM元素引用，便于垃圾回收
const messageElements = new WeakMap();

// 优化后的通知模式变更监听
function handleNotificationModeChange() {
    notificationMode = this.value;
}
notificationSelect.addEventListener('change', handleNotificationModeChange);

// 优化后的键盘事件处理
function handleKeyDown(event) {
    if (event.key === 'Enter') {
        if (!event.shiftKey) {
            event.preventDefault();
            sendMessage();
        } else {
            event.preventDefault();
            const currentRows = parseInt(messageInput.rows);
            const maxRows = parseInt(messageInput.getAttribute('maxrows')) || 6;

            if (currentRows < maxRows) {
                messageInput.rows = currentRows + 1;
            }

            const startPos = messageInput.selectionStart;
            const endPos = messageInput.selectionEnd;
            messageInput.value = messageInput.value.substring(0, startPos) + "\n" +
                messageInput.value.substring(endPos);
            messageInput.selectionStart = messageInput.selectionEnd = startPos + 1;
        }
    }
}
messageInput.addEventListener('keydown', handleKeyDown);

// 优化cookie获取
function getCookie(name) {
    const value = `; ${document.cookie}`;
    const parts = value.split(`; ${name}=`);
    if (parts.length === 2) return parts.pop().split(';').shift();
    return null;
}

const clientid = getCookie("clientid");
const uid = getCookie("uid");
const serverUrl = window.location.origin;

// 修复的Base64编解码函数 - 保留LaTeX符号
function encodeBase64(str) {
    // 保留换行符处理
    const withLineBreaks = str.replace(/\n/g, '__BR__');
    const encoder = new TextEncoder();
    return btoa(String.fromCharCode(...encoder.encode(withLineBreaks)));
}

function decodeBase64(base64Str) {
    try {
        const binaryStr = atob(base64Str);
        const bytes = new Uint8Array(binaryStr.length);
        for (let i = 0; i < binaryStr.length; i++) {
            bytes[i] = binaryStr.charCodeAt(i);
        }
        // 保留LaTeX符号，只转换换行符
        return new TextDecoder().decode(bytes).replace(/__BR__/g, '<br>');
    } catch (e) {
        console.error("Base64解码失败:", e);
        return base64Str;
    }
}
function utf8ToGbk(utf8Str) {
    // 将 UTF-8 字符串转换为 Unicode 码点数组
    const unicodeArray = Encoding.stringToCode(utf8Str);
    // 转换为 GBK 字节数组
    const gbkBytes = Encoding.convert(unicodeArray, 'GBK', 'UNICODE');
    // 将字节数组转换为 ISO-8859-1 字符串（每个字节一个字符）
    return String.fromCharCode(...gbkBytes);
}

function gbkToUtf8(gbkStr) {
    // 将伪 GBK 字符串转为字节数组
    const gbkBytes = [];
    for (let i = 0; i < gbkStr.length; i++) {
        gbkBytes.push(gbkStr.charCodeAt(i));
    }
    // 转换为 Unicode 字符数组
    const unicodeArray = Encoding.convert(gbkBytes, 'UNICODE', 'GBK');
    // 返回 UTF-8 字符串
    return Encoding.codeToString(unicodeArray);
}

// 增强的LaTeX检测
function containsLaTeX(text) {
    return /\$(.*?)\$|\\\((.*?)\\\)|\\\[(.*?)\\\]/.test(text);
}

// 优化任务栏闪烁
let flashInterval = null;
function flashTaskbar() {
    if (document.hasFocus() || flashInterval) return;

    const originalTitle = document.title;
    let flashCount = 0;

    flashInterval = setInterval(() => {
        document.title = document.title === "【新消息】" ? originalTitle : "【新消息】";
        flashCount++;

        if (flashCount >= 10) {
            clearInterval(flashInterval);
            flashInterval = null;
            document.title = originalTitle;
        }
    }, 500);
}

// 优化消息获取和渲染
let lastMessageCount = 0;
let isRendering = false;

// Track rendered LaTeX elements
const renderedLaTeXMessages = new Set();

// Modify fetchChatMessages to fetch only new messages incrementally
async function fetchChatMessages() {
    if (isRendering) return;
    isRendering = true;

    try {
        const path = window.location.pathname;
        const roomID = path.match(/^\/chat(\d+)$/)[1];
        const response = await fetch(`${serverUrl}/chat/${roomID}/messages?lastTimestamp=${lastTimestamp}`);

        if (!response.ok) {
            console.error("Error fetching messages:", response.statusText);
            return;
        }

        const messages = await response.json();

        // 检查 messages 是否为 null 或 undefined
        if (!messages || !Array.isArray(messages) || messages.length === 0) {
            return;
        }

        const isScrolledToBottom = chatBox.scrollHeight - chatBox.clientHeight <= chatBox.scrollTop + 1;
        const isTextSelected = window.getSelection().toString() !== '';
        if (isTextSelected) {
            isRendering = false;
            return;
        }

        const previousScrollTop = chatBox.scrollTop;
        let containsMath = false;

        // Use a document fragment to minimize DOM operations
        const fragment = document.createDocumentFragment();

        messages.forEach(msg => {
            //console.log("Timestamp received:", msg.timestamp);
            const msgTimestamp = new Date(msg.timestamp).getTime();
            if (msgTimestamp > lastTimestamp) {
                lastTimestamp = msgTimestamp;
            }

            const decoded = decodeBase64(msg.message);
            if (containsLaTeX(decoded) && !renderedLaTeXMessages.has(msg.timestamp)) {
                containsMath = true;
                renderedLaTeXMessages.add(msg.timestamp);
            }

            const messageElement = createMessageElement(msg);
            if (messageElement) {
                fragment.appendChild(messageElement);
                if (msg.isNew) checkForNotification(msg);
            }
        });

        // Append new messages without clearing the chat box
        chatBox.appendChild(fragment);

        // Render LaTeX only for new content
        if (containsMath && window.MathJax?.typesetPromise) {
            await new Promise(resolve => setTimeout(resolve, 50)); // Ensure DOM update
            await MathJax.typesetPromise();
        }


        if (!isScrolledToBottom) {
            chatBox.scrollTop = previousScrollTop;
        } else {
            chatBox.scrollTop = chatBox.scrollHeight;
        }
    } catch (error) {
        console.error("Error fetching messages:", error);
    } finally {
        isRendering = false;
    }
}


function createMessageElement(msg) {
    // 将时间戳从秒转换为毫秒
    const messageTime = new Date(msg.timestamp * 1000).toLocaleTimeString();
    const isOwnMessage = (msg.user === currentUsername);
    const messageClass = isOwnMessage ? 'message user' : 'message';

    // 修改处：获取解码后的消息，并对系统消息（msg.label === 'GM'）转换为GBK
    let decodedMessage = decodeBase64(msg.message);

    const renderMarkdown = /[#*_`$]/.test(decodedMessage);
    const processedMessage = renderMarkdown ? marked.parse(decodedMessage) : decodedMessage;

    const messageDiv = document.createElement('div');
    messageDiv.className = `${messageClass} ${msg.isNew ? 'fade-in' : ''}`;

    let messageStyle = '';
    switch (msg.label) {
        case 'GM':
            messageStyle = document.body.classList.contains("dark-mode") ?
                'background-color: black; color: white;' :
                'background-color: white; color: black;';
            break;
        case 'U':
            messageStyle = document.body.classList.contains("dark-mode") ?
                'background-color: rgba(0, 204, 255, 0.15); color: white;' :
                'background-color: rgba(0, 204, 255, 0.10); color: black;';
            break;
        case 'BAN':
            messageStyle = document.body.classList.contains("dark-mode") ?
                'background-color: white; color: white;' :
                'background-color: black; color: black;';
            break;
        default:
            messageStyle = document.body.classList.contains("dark-mode") ?
                'background-color: black; color: white;' :
                'background-color: white; color: black;';
    }

    messageDiv.style.cssText = `${messageStyle}; white-space: normal; word-wrap: break-word;`;

    const headerDiv = document.createElement('div');
    headerDiv.className = 'header';
    headerDiv.style.backgroundColor = 'transparent';

    const usernameDiv = document.createElement('div');
    usernameDiv.className = 'username';
    usernameDiv.textContent = msg.user;
    usernameDiv.style.color = messageStyle.includes('white') ? 'white' : 'black';

    const timestampDiv = document.createElement('div');
    timestampDiv.className = 'timestamp';
    timestampDiv.textContent = messageTime;
    timestampDiv.style.color = messageStyle.includes('white') ? 'white' : 'black';

    headerDiv.appendChild(usernameDiv);
    headerDiv.appendChild(timestampDiv);
    messageDiv.appendChild(headerDiv);

    const bodyDiv = document.createElement('div');
    // 如果是图片消息，则设置对应的 class 名称
    bodyDiv.className = msg.imageUrl ? 'image-message' : 'message-body';

    // 如果有图片，先添加图片及其事件绑定
    if (msg.imageUrl) {
        const brStart = document.createElement('br');
        const img = document.createElement('img');
        img.src = msg.imageUrl;
        img.alt = "Image";
        img.style.maxWidth = '100%';
        img.style.height = 'auto';
        img.style.cursor = 'pointer';
        // 绑定点击事件
        img.addEventListener('click', function (event) {
            event.stopPropagation();
            event.preventDefault();
            toggleFullScreen(this);
        });
        const brEnd = document.createElement('br');
        bodyDiv.appendChild(brStart);
        bodyDiv.appendChild(img);
        bodyDiv.appendChild(brEnd);
    }

    // 使用 DOM 方法添加 Markdown 渲染内容，避免 innerHTML 覆盖前面添加的元素
    const markdownContainer = document.createElement('div');
    markdownContainer.innerHTML = processedMessage;
    bodyDiv.appendChild(markdownContainer);

    messageDiv.appendChild(bodyDiv);
    return messageDiv;
}



function checkForNotification(msg) {
    const msgId = `${msg.timestamp}_${msg.user}_${msg.message}`;
    if (notifiedMessages.has(msgId)) return;

    const decodedMessage = decodeBase64(msg.message);
    const isOwnMessage = (msg.user === currentUsername);

    if (notificationMode === 'always' ||
        (notificationMode === 'mention' && !isOwnMessage && decodedMessage.includes(currentUsername))) {
        flashTaskbar();
    }

    notifiedMessages.add(msgId);
    // 限制通知消息集合大小
    if (notifiedMessages.size > 100) {
        const oldest = Array.from(notifiedMessages).slice(0, 20);
        oldest.forEach(id => notifiedMessages.delete(id));
    }
}

// 图片处理优化
const imageInput = document.getElementById("imageInput");
const imagePreview = document.getElementById("imagePreview");
let imageObjectUrl = null;

function selectImage() {
    imageInput.click();
}

function handleImageInput() {
    if (imageObjectUrl) {
        URL.revokeObjectURL(imageObjectUrl);
        imageObjectUrl = null;
    }

    const file = imageInput.files[0];
    if (file) {
        imageObjectUrl = URL.createObjectURL(file);
        imagePreview.innerHTML = `<img src="${imageObjectUrl}" alt="Image preview" 
            style="max-width: 100px; height: auto; margin-left: 10px;" />`;
    } else {
        imagePreview.innerHTML = '';
    }
}
imageInput.addEventListener('change', handleImageInput);

async function uploadImage(file) {
    const path = window.location.pathname;
    const roomID = path.match(/^\/chat(\d+)$/)[1];

    const formData = new FormData();
    formData.append('file', file);

    try {
        const response = await fetch(`${serverUrl}/chat/${roomID}/upload`, {
            method: 'POST',
            body: formData
        });

        if (response.ok) {
            return await response.json();
        }
        throw new Error(`Upload failed: ${response.status}`);
    } catch (error) {
        console.error("Error uploading image:", error);
        return null;
    }
}

// 优化后的发送消息函数
async function sendMessage() {
    let messageText = messageInput.value.trim();
    const imageFile = imageInput.files[0];

    if (!messageText && !imageFile) return;

    // 如果只有图片但消息为空，则附带一个 "__BR__"
    if (!messageText && imageFile) {
        messageText = "__BR__";
    }

    messageText = messageText.replace(/\n/g, '__BR__');
    const path = window.location.pathname;
    const roomID = path.match(/^\/chat(\d+)$/)[1];

    let imageUrl = '';
    if (imageFile) {
        const result = await uploadImage(imageFile);
        imageUrl = result?.imageUrl || '';
    }

    const message = {
        message: encodeBase64(messageText),
        uid: uid,
        imageUrl: imageUrl,
        timestamp: new Date().toISOString()
    };

    try {
        const response = await fetch(`${serverUrl}/chat/${roomID}/messages`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(message)
        });

        if (response.ok) {
            messageInput.value = '';
            messageInput.rows = 1;
            imageInput.value = '';
            if (imageObjectUrl) {
                URL.revokeObjectURL(imageObjectUrl);
                imageObjectUrl = null;
            }
            imagePreview.innerHTML = '';
            await fetchChatMessages();
            chatBox.scrollTop = chatBox.scrollHeight;
        } else if (response.status === 400) {
            document.cookie = "clientid=; expires=Thu, 01 Jan 1970 00:00:00 GMT";
            document.cookie = "uid=; expires=Thu, 01 Jan 1970 00:00:00 GMT";
            window.location.href = "/login";
        } else {
            throw new Error(`Failed to send: ${response.status}`);
        }
    } catch (error) {
        console.error("Error sending message:", error);
    }
}


// 优化全屏图片查看
function toggleFullScreen(imgElement) {
    console.log("114514");
    const existingOverlay = document.getElementById("imageFullscreen");
    if (existingOverlay) {
        existingOverlay.remove();
        return;
    }

    const overlay = document.createElement("div");
    overlay.id = "imageFullscreen";
    overlay.style.cssText = `
        position: fixed; top: 0; left: 0; width: 100vw; height: 100vh;
        background-color: rgba(0, 0, 0, 0.8); display: flex;
        align-items: center; justify-content: center; z-index: 9999;
    `;

    const fullImg = document.createElement("img");
    fullImg.src = imgElement.src;
    fullImg.style.cssText = `
        max-width: 90%; max-height: 90%; 
        object-fit: contain; /* 等比例拉伸 */
        box-shadow: 0 0 20px rgba(255,255,255,0.5);
        cursor: pointer;
    `;

    overlay.appendChild(fullImg);
    overlay.addEventListener("click", () => overlay.remove(), { once: true });
    document.body.appendChild(overlay);
}

// 优化用户信息获取
async function fetchUsername() {
    try {
        const response = await fetch(`${serverUrl}/user/username?uid=${uid}`);
        if (response.ok) {
            const data = await response.json();
            currentUsername = data.username;
            usernameDisplay.textContent = currentUsername;
            loginButton.style.display = 'none';
        } else {
            throw new Error(`Failed to fetch username: ${response.status}`);
        }
    } catch (error) {
        console.error("Error fetching username:", error);
        loginButton.style.display = 'inline-block';
    }
}

// 主题管理优化
let isUserChangingTheme = false;

function detectSystemTheme() {
    const systemDarkMode = window.matchMedia('(prefers-color-scheme: dark)').matches;
    document.body.classList.toggle("dark-mode", systemDarkMode);
}

function handleThemeToggle() {
    document.body.classList.toggle("dark-mode");
    isUserChangingTheme = true;
}

function handleSystemThemeChange(e) {
    if (!isUserChangingTheme) {
        document.body.classList.toggle("dark-mode", e.matches);
    }
}

themeToggle.addEventListener("click", handleThemeToggle);
detectSystemTheme();
window.matchMedia('(prefers-color-scheme: dark)').addEventListener('change', handleSystemThemeChange);

// 优化轮询机制
let fetchInterval = 500;
let errorCount = 0;
let pollingTimer = null;

function adjustPollingInterval() {
    if (document.hidden) {
        fetchInterval = 10000;
    } else if (document.hasFocus()) {
        fetchInterval = 500;
    } else {
        fetchInterval = 500;
    }
}

// Update polling mechanism to use fetchChatMessages
async function fetchWithRetry() {
    if (!isPollingActive) return;

    clearTimeout(pollingTimer);

    try {
        await fetchChatMessages();
        errorCount = 0;
        fetchInterval = 500;
    } catch (error) {
        console.error("Polling error:", error);
        errorCount++;
        fetchInterval = 500;
    } finally {
        adjustPollingInterval();
        pollingTimer = setTimeout(fetchWithRetry, fetchInterval);
    }
}

// 页面可见性管理
function handleVisibilityChange() {
    if (document.visibilityState === 'visible') {
        isPollingActive = true;
        fetchWithRetry();
    } else {
        isPollingActive = false;
        clearTimeout(pollingTimer);
    }
}

// 初始化
document.addEventListener('DOMContentLoaded', async () => {
    // 检查聊天室权限
    async function checkChatroomAccess() {
        const path = window.location.pathname;
        const match = path.match(/^\/chat(\d+)$/);
        if (!match) {
            showAccessDenied();
            return false;
        }

        const chatroomId = parseInt(match[1], 10);
        try {
            const response = await fetch('/list', { credentials: 'include' });
            if (!response.ok) throw new Error('Failed to fetch room list');

            const joinedRooms = await response.json();
            const hasAccess = joinedRooms.some(room => parseInt(room.id, 10) === chatroomId);

            if (!hasAccess) showAccessDenied();
            return hasAccess;
        } catch (error) {
            console.error('Access check failed:', error);
            showAccessDenied();
            return false;
        }
    }

    function showAccessDenied() {
        document.body.innerHTML = `
            <div style="text-align: center; margin-top: 50px;">
                <h1 style="font-size: 48px; color: #FF0000;">404</h1>
                <p style="font-size: 24px; color: #555;">你没有加入这个聊天室！</p>
            </div>
        `;
        document.title = '404 - 页面未找到';
        isPollingActive = false;
    }

    const hasAccess = await checkChatroomAccess();



    // 新增：更新顶部栏中央聊天室名称
    if (hasAccess) {
        const path = window.location.pathname;
        const match = path.match(/^\/chat(\d+)$/);
        if (match) {
            const roomID = match[1];
            const chatroomNameElement = document.getElementById("chatroomName");
            if (chatroomNameElement) {
                // Fetch the chatroom name using the API
                fetch(`${serverUrl}/chatroomname?roomId=${roomID}`)
                    .then(response => {
                        if (!response.ok) {
                            throw new Error("Failed to fetch chatroom name");
                        }
                        return response.json();
                    })
                    .then(data => {
                        chatroomNameElement.textContent = data.name || `无名的聊天室`;
                    })
                    .catch(error => {
                        console.error("Error fetching chatroom name:", error);
                        chatroomNameElement.textContent = `无名的聊天室`; // Fallback to room ID
                    });
            }
        }
    }

    if (hasAccess) {
        await fetchUsername();
        fetchWithRetry();
        document.addEventListener('visibilitychange', handleVisibilityChange);
        window.addEventListener('focus', fetchWithRetry);
    }
});

// 清理函数
function cleanup() {
    isPollingActive = false;
    clearTimeout(pollingTimer);

    if (imageObjectUrl) {
        URL.revokeObjectURL(imageObjectUrl);
    }

    if (flashInterval) {
        clearInterval(flashInterval);
    }

    messageInput.removeEventListener('keydown', handleKeyDown);
    notificationSelect.removeEventListener('change', handleNotificationModeChange);
    imageInput.removeEventListener('change', handleImageInput);
    themeToggle.removeEventListener('click', handleThemeToggle);
    document.removeEventListener('visibilitychange', handleVisibilityChange);
    window.removeEventListener('focus', fetchWithRetry);
}



// 页面卸载时清理
window.addEventListener('beforeunload', cleanup);
window.addEventListener('unload', cleanup);

// Incremental message fetching
let lastTimestamp = 0;

// 为Logo添加点击事件
document.addEventListener('DOMContentLoaded', () => {
    const logoImg = document.querySelector('.header-left img');
    if (logoImg) {
        logoImg.addEventListener('click', () => {
            logoImg.classList.add('logo-clicked');
            setTimeout(() => logoImg.classList.remove('logo-clicked'), 500);
            window.location.href = '/';
        });
    }
});