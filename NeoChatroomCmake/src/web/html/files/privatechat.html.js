// 全局变量
let currentUsername = ""; // 当前登录用户名
let selectedUser = null; // 当前选中的用户
let lastMessageTimestamp = 0; // 上次获取的消息时间戳
let messagePollingInterval = null; // 消息轮询定时器
let userList = []; // 用户列表缓存

// 工具函数
// Base64 编解码函数
const Base64 = {
    encode: function(str) {
        return btoa(encodeURIComponent(str).replace(/%([0-9A-F]{2})/g, function(match, p1) {
            return String.fromCharCode('0x' + p1);
        }));
    },
    decode: function(str) {
        try {
            return decodeURIComponent(Array.prototype.map.call(atob(str), function(c) {
                return '%' + ('00' + c.charCodeAt(0).toString(16)).slice(-2);
            }).join(''));
        } catch (e) {
            console.error('Base64解码错误:', e);
            return str;
        }
    }
};

// 获取 cookie 的工具函数
function getCookie(name) {
    const value = "; " + document.cookie;
    const parts = value.split("; " + name + "=");
    if (parts.length === 2) return parts.pop().split(";").shift();
    return null;
}

// 格式化时间戳
function formatTimestamp(timestamp) {
    const date = new Date(timestamp * 1000);
    const today = new Date();
    
    // 检查是否是今天
    if (date.toDateString() === today.toDateString()) {
        return date.toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' });
    }
    
    // 检查是否是昨天
    const yesterday = new Date(today);
    yesterday.setDate(yesterday.getDate() - 1);
    if (date.toDateString() === yesterday.toDateString()) {
        return '昨天 ' + date.toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' });
    }
    
    // 否则显示完整日期
    return date.toLocaleDateString() + ' ' + date.toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' });
}

// 获取用户头像首字母
function getAvatarInitial(username) {
    if (!username) return "?";
    return username.charAt(0).toUpperCase();
}

// 用户认证相关
// 更新登录状态
async function updateLoginStatus() {
    const uid = getCookie("uid");
    const serverUrl = window.location.origin;
    
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
        console.error('更新登录状态失败:', error);
        document.getElementById('loginButton').style.display = 'inline-block';
        currentUsername = '';
        document.getElementById('usernameDisplay').textContent = '';
        return false;
    }
}

// 检查登录状态
async function checkLoginStatus() {
    const uid = getCookie("uid");
    const serverUrl = window.location.origin;
    
    try {
        const response = await fetch(`${serverUrl}/user/username?uid=${uid}`);
        
        // 未授权时重定向到登录页
        if (response.status === 401 || response.status === 403) {
            window.location.href = '/login';
            return false;
        }
        
        if (!response.ok) {
            console.error('检查登录状态失败:', response.status);
            return false;
        }
        
        return true;
    } catch (error) {
        console.error('检查登录状态错误:', error);
        return false;
    }
}

// 用户列表相关
// 获取用户列表
async function fetchUserList() {
    if (!await checkLoginStatus()) return;
    
    try {
        // 分页加载更多用户数据
        let allUsers = [];
        let startUid = 1;
        let hasMore = true;
        const pageSize = 100;  // 一次加载更多用户
        
        // 显示加载状态
        document.getElementById('userList').innerHTML = '<div class="empty-message">加载中...</div>';
        
        while (hasMore) {
            const endUid = startUid + pageSize - 1;
            const response = await fetch(`/api/users?start=${startUid}&end=${endUid}&size=${pageSize}`, {
                method: 'GET',
                credentials: 'include'
            });
            
            if (!response.ok) {
                throw new Error(`获取用户列表失败: ${response.status}`);
            }
            
            const data = await response.json();
            const users = data.users || [];
            
            if (users.length === 0) break;
            
            allUsers = [...allUsers, ...users];
            
            // 如果获取的用户数量小于请求的数量，说明没有更多用户了
            if (users.length < pageSize) {
                hasMore = false;
            } else {
                startUid += pageSize;
            }
            
            // 防止请求过多，限制最多获取1000个用户
            if (allUsers.length >= 1000) {
                hasMore = false;
            }
        }
        
        // 过滤掉自己和被封禁的用户
        userList = allUsers.filter(user => 
            user.username !== currentUsername && 
            user.labei !== 'BAN'
        );
        
        renderUserList(userList);
    } catch (error) {
        console.error('获取用户列表失败:', error);
        document.getElementById('userList').innerHTML = '<div class="empty-message">获取用户列表失败</div>';
    }
}

// 渲染用户列表
function renderUserList(users) {
    const userListElement = document.getElementById('userList');
    userListElement.innerHTML = '';
    
    if (!users || users.length === 0) {
        userListElement.innerHTML = '<div class="empty-message">没有找到用户</div>';
        return;
    }
    
    const fragment = document.createDocumentFragment();
    
    users.forEach((user, index) => {
        const userItem = document.createElement('div');
        userItem.className = 'user-item';
        userItem.dataset.username = user.username;
        userItem.style.animationDelay = `${index * 0.03}s`;
        
        const avatar = document.createElement('div');
        avatar.className = 'user-avatar';
        avatar.textContent = getAvatarInitial(user.username);
        
        const userInfo = document.createElement('div');
        userInfo.className = 'user-info';
        
        const userName = document.createElement('div');
        userName.className = 'user-name';
        userName.textContent = user.username;
        
        const userStatus = document.createElement('div');
        userStatus.className = 'user-status';
        userStatus.textContent = user.labei === 'GM' ? '管理员' : '在线';
        
        userInfo.appendChild(userName);
        userInfo.appendChild(userStatus);
        
        userItem.appendChild(avatar);
        userItem.appendChild(userInfo);
        
        // 添加点击事件处理
        userItem.addEventListener('click', () => {
            selectUser(user);
        });
        
        fragment.appendChild(userItem);
    });
    
    userListElement.appendChild(fragment);
}

// 搜索用户
function searchUsers(term) {
    if (!term) {
        renderUserList(userList);
        return;
    }
    
    const searchTerm = term.toLowerCase();
    const filteredUsers = userList.filter(user => 
        user.username.toLowerCase().includes(searchTerm) || 
        user.uid.toString().includes(searchTerm)
    );
    
    renderUserList(filteredUsers);
}

// 聊天功能相关
// 选择用户开始聊天
function selectUser(user) {
    selectedUser = user;
    
    // 更新UI以反映选择的用户
    document.querySelectorAll('.user-item').forEach(item => {
        item.classList.remove('active');
        if (item.dataset.username === user.username) {
            item.classList.add('active');
        }
    });
    
    // 显示聊天区域，隐藏欢迎屏幕
    document.getElementById('welcomeScreen').style.display = 'none';
    document.getElementById('chatArea').style.display = 'flex';
    
    // 更新聊天区域的用户信息
    document.getElementById('chatUserAvatar').textContent = getAvatarInitial(user.username);
    document.getElementById('chatUserName').textContent = user.username;
    
    // 清空消息区域
    document.getElementById('chatMessages').innerHTML = '<div class="empty-message">加载消息中...</div>';
    
    // 重置消息时间戳
    lastMessageTimestamp = 0;
    
    // 加载与该用户的聊天记录
    loadChatHistory();
    
    // 清除之前的轮询
    if (messagePollingInterval) {
        clearInterval(messagePollingInterval);
    }
    
    // 开始轮询新消息
    messagePollingInterval = setInterval(loadNewMessages, 3000);
    
    // 聚焦输入框
    document.getElementById('messageInput').focus();
}

// 加载与当前选中用户的聊天历史
async function loadChatHistory() {
    if (!selectedUser || !currentUsername) return;
    
    try {
        const response = await fetch(`/private/messages?from=${encodeURIComponent(currentUsername)}&to=${encodeURIComponent(selectedUser.username)}`, {
            method: 'GET',
            credentials: 'include'
        });
        
        if (!response.ok) {
            throw new Error(`获取消息失败: ${response.status}`);
        }
        
        const messages = await response.json();
        
        if (Array.isArray(messages) && messages.length > 0) {
            renderMessages(messages);
            
            // 更新最后消息时间戳
            const lastMessage = messages[messages.length - 1];
            if (lastMessage && lastMessage.timestamp) {
                lastMessageTimestamp = lastMessage.timestamp;
            }
        } else {
            document.getElementById('chatMessages').innerHTML = '<div class="empty-message">暂无消息</div>';
        }
    } catch (error) {
        console.error('加载聊天历史失败:', error);
        document.getElementById('chatMessages').innerHTML = '<div class="empty-message">加载消息失败</div>';
    }
}

// 加载新消息
async function loadNewMessages() {
    if (!selectedUser || !currentUsername) return;
    
    try {
        const response = await fetch(`/private/messages?from=${encodeURIComponent(currentUsername)}&to=${encodeURIComponent(selectedUser.username)}&lastTimestamp=${lastMessageTimestamp}`, {
            method: 'GET',
            credentials: 'include'
        });
        
        if (!response.ok) {
            throw new Error(`获取新消息失败: ${response.status}`);
        }
        
        const messages = await response.json();
        
        if (Array.isArray(messages) && messages.length > 0) {
            // 如果是初次加载，清空"暂无消息"提示
            if (document.getElementById('chatMessages').innerHTML.includes('暂无消息')) {
                document.getElementById('chatMessages').innerHTML = '';
            }
            
            renderNewMessages(messages);
            
            // 更新最后消息时间戳
            const lastMessage = messages[messages.length - 1];
            if (lastMessage && lastMessage.timestamp) {
                lastMessageTimestamp = lastMessage.timestamp;
            }
        }
    } catch (error) {
        console.error('加载新消息失败:', error);
    }
}

// 渲染消息列表
function renderMessages(messages) {
    const chatMessages = document.getElementById('chatMessages');
    chatMessages.innerHTML = '';
    
    messages.forEach(renderSingleMessage);
    
    // 滚动到底部
    scrollToBottom();
}

// 渲染新消息
function renderNewMessages(messages) {
    messages.forEach(renderSingleMessage);
    
    // 滚动到底部
    scrollToBottom();
}

// 渲染单条消息
function renderSingleMessage(message) {
    const chatMessages = document.getElementById('chatMessages');
    
    const messageElement = document.createElement('div');
    messageElement.className = 'message';
    
    // 判断消息类型
    const isCurrentUserMessage = message.user === currentUsername;
    messageElement.classList.add(isCurrentUserMessage ? 'sent' : 'received');
    
    // 解码消息内容
    let messageContent;
    try {
        messageContent = Base64.decode(message.message);
    } catch (e) {
        messageContent = message.message;
        console.error('消息解码失败:', e);
    }
    
    // 创建消息文本
    const messageText = document.createElement('div');
    messageText.className = 'message-text';
    messageText.textContent = messageContent;
    
    // 创建时间标签
    const messageTime = document.createElement('span');
    messageTime.className = 'message-time';
    messageTime.textContent = formatTimestamp(message.timestamp);
    
    // 组装消息元素
    messageElement.appendChild(messageText);
    messageElement.appendChild(messageTime);
    
    // 添加到消息列表
    chatMessages.appendChild(messageElement);
}

// 滚动到聊天区域底部
function scrollToBottom() {
    const chatMessages = document.getElementById('chatMessages');
    chatMessages.scrollTop = chatMessages.scrollHeight;
}

// 发送消息
async function sendMessage() {
    if (!selectedUser || !currentUsername) return;
    
    const messageInput = document.getElementById('messageInput');
    const message = messageInput.value.trim();
    
    if (!message) return;
    
    // 清空输入框
    messageInput.value = '';
    
    try {
        // 发送消息到服务器
        const response = await fetch('/private/send', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify({
                to: selectedUser.username,
                message: Base64.encode(message)
            }),
            credentials: 'include'
        });
        
        if (!response.ok) {
            throw new Error(`发送消息失败: ${response.status}`);
        }
        
        // 立即刷新消息列表，以显示新发送的消息
        loadNewMessages();
    } catch (error) {
        console.error('发送消息失败:', error);
        alert('发送消息失败，请重试');
    }
}

// 界面功能相关
// 设置主题切换
function setupThemeToggle() {
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
}

// 初始化事件监听
function setupEventListeners() {
    // 搜索框事件
    const searchInput = document.getElementById('userSearchInput');
    searchInput.addEventListener('input', (e) => {
        searchUsers(e.target.value);
    });
    
    // 发送按钮事件
    const sendButton = document.getElementById('sendButton');
    sendButton.addEventListener('click', sendMessage);
    
    // 输入框按Enter发送
    const messageInput = document.getElementById('messageInput');
    messageInput.addEventListener('keydown', (e) => {
        if (e.key === 'Enter' && !e.shiftKey) {
            e.preventDefault();
            sendMessage();
        }
    });
    
    // 输入框自动调整高度
    messageInput.addEventListener('input', function() {
        this.style.height = 'auto';
        this.style.height = (this.scrollHeight) + 'px';
    });
}

// 页面初始化
async function initPage() {
    // 检查登录状态
    if (!await checkLoginStatus()) {
        window.location.href = '/login';
        return;
    }
    
    // 更新登录状态
    await updateLoginStatus();
    
    // 设置主题切换
    setupThemeToggle();
    
    // 设置事件监听
    setupEventListeners();
    
    // 获取用户列表
    await fetchUserList();
}

// 页面加载完成后初始化
document.addEventListener('DOMContentLoaded', initPage);

// 页面卸载前清理
window.addEventListener('beforeunload', () => {
    if (messagePollingInterval) {
        clearInterval(messagePollingInterval);
    }
});
