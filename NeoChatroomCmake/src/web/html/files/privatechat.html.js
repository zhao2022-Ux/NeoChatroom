// 全局变量
let currentUsername = ""; 
let selectedUser = null; 
let lastMessageTimestamp = 0; 
let messagePollingInterval = null; 
let userList = []; 
let checkUnreadInterval = null; 
let allMessages = {}; // 存储所有聊天消息，按用户分组

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
// ��新登录状态
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

// 选择用户开始聊天
function selectUser(user) {
    selectedUser = user;
    
    // 更新UI以反映选择的用户
    document.querySelectorAll('.user-item').forEach(item => {
        item.classList.remove('active');
        if (item.dataset.username === user.username) {
            item.classList.add('active');
            // 隐藏未读消息标记
            const badge = item.querySelector('.user-badge');
            if (badge) {
                badge.style.display = 'none';
            }
        }
    });
    
    // 显示聊天区域，隐藏欢迎屏幕
    document.getElementById('welcomeScreen').style.display = 'none';
    document.getElementById('chatArea').style.display = 'flex';
    
    // 更新聊天区域的用户信息
    document.getElementById('chatUserAvatar').textContent = getAvatarInitial(user.username);
    document.getElementById('chatUserName').textContent = user.username;
    
    console.log(`选择用户: ${user.username}`);
    
    // 直接加载与该用户的对话
    loadDirectMessages(user.username);
    
    // 聚焦输入框
    document.getElementById('messageInput').focus();
    
    // 标记与该用户的消息为已读
    markMessagesAsRead(currentUsername, user.username);
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
        userStatus.textContent = user.labei === 'GM' ? '管理员' : '';  // 移除在线提示
        
        userInfo.appendChild(userName);
        userInfo.appendChild(userStatus);
        
        userItem.appendChild(avatar);
        userItem.appendChild(userInfo);
        
        // 添加未读消息指示器
        const unreadBadge = document.createElement('div');
        unreadBadge.className = 'user-badge';
        unreadBadge.style.display = 'none';
        unreadBadge.textContent = '!';
        userItem.appendChild(unreadBadge);
        
        // 添加点击事件处理
        userItem.addEventListener('click', () => {
            selectUser(user);
        });
        
        fragment.appendChild(userItem);
    });
    
    userListElement.appendChild(fragment);
    
    // 开始检查未读消息
    startCheckingUnread();
}

// 添加检查未读消息的函数
function startCheckingUnread() {
    // 清除现有的定时器
    if (checkUnreadInterval) {
        clearInterval(checkUnreadInterval);
    }
    
    // 立即检查一次
    checkUnreadMessages();
    
    // 设置定时器，每10秒检查一次
    checkUnreadInterval = setInterval(checkUnreadMessages, 10000);
}

// 检查所有用户的未读消息
function checkUnreadMessages() {
    if (!currentUsername) return;
    
    // 首先检查当前用户是否有未读消息
    fetch(`/private/user-unread?user=${encodeURIComponent(currentUsername)}`, {
        method: 'GET',
        credentials: 'include'
    })
    .then(response => {
        if (!response.ok) throw new Error('Network response was not ok');
        return response.json();
    })
    .then(data => {
        if (data.has_unread) {
            // 如果有未读消息，逐个检查每个用户
            userList.forEach(user => {
                if (user.username !== currentUsername) {
                    checkUnreadWithUser(user.username);
                }
            });
        }
    })
    .catch(error => {
        console.error('Error checking unread messages:', error);
    });
}

// 检查与特定用户的未读消息
function checkUnreadWithUser(username) {
    fetch(`/private/check-unread?from=${encodeURIComponent(currentUsername)}&to=${encodeURIComponent(username)}`, {
        method: 'GET',
        credentials: 'include'
    })
    .then(response => {
        if (!response.ok) throw new Error('Network response was not ok');
        return response.json();
    })
    .then(data => {
        // 更新UI显示未读消息标记
        const userItem = document.querySelector(`.user-item[data-username="${username}"]`);
        if (userItem) {
            const badge = userItem.querySelector('.user-badge');
            if (badge) {
                badge.style.display = data.has_unread ? 'flex' : 'none';
            }
        }
    })
    .catch(error => {
        console.error('Error checking unread with user:', error);
    });
}

// 聊天功能相关
// 加载当前用户的所有私聊消息
async function loadAllUserMessages() {
    if (!currentUsername) return;
    
    try {
        // 获取与当前用户相关的所有消息，不指定特定对话者
        const response = await fetch(`/private/messages?lastTimestamp=${lastMessageTimestamp}`, {
            method: 'GET',
            credentials: 'include'
        });
        
        if (!response.ok) {
            throw new Error(`获取消息失败: ${response.status}`);
        }
        
        const messages = await response.json();
        
        if (Array.isArray(messages) && messages.length > 0) {
            // 处理新消息
            processNewMessages(messages);
            
            // 更新最后消息时间戳
            const lastMessage = messages[messages.length - 1];
            if (lastMessage && lastMessage.timestamp) {
                lastMessageTimestamp = lastMessage.timestamp;
            }
            
            // 如果有选中的用户，更新聊天界面
            if (selectedUser) {
                updateChatView(selectedUser.username);
            }
            
            // 更新所有未读消息指示器
            updateUnreadIndicators();
        }
    } catch (error) {
        console.error('加载消息失败:', error);
    }
}

// 处理新接收到的消息
function processNewMessages(messages) {
    // 按照消息的发送者和接收者进行分组
    messages.forEach(message => {
        const sender = message.user;
        let receiver = "";
        
        // 从metadata中提取接收者
        if (message.metadata) {
            if (typeof message.metadata === 'string') {
                try {
                    const metadata = JSON.parse(message.metadata);
                    receiver = metadata.to || "";
                } catch (e) {
                    console.error('解析metadata失败:', e);
                }
            } else if (typeof message.metadata === 'object') {
                receiver = message.metadata.to || "";
            }
        }
        
        // 确定这条消息属于哪个对话
        let chatPartner = "";
        if (sender === currentUsername) {
            chatPartner = receiver;
        } else if (receiver === currentUsername) {
            chatPartner = sender;
        }
        
        // 如果能确定对话伙伴，将消息添加到相应的对话中
        if (chatPartner) {
            if (!allMessages[chatPartner]) {
                allMessages[chatPartner] = [];
            }
            
            // 检查是否已存���相同的消息（避免重复）
            const isDuplicate = allMessages[chatPartner].some(
                m => m.timestamp === message.timestamp && 
                     m.user === message.user && 
                     m.message === message.message
            );
            
            if (!isDuplicate) {
                allMessages[chatPartner].push(message);
                
                // 按时间戳排序
                allMessages[chatPartner].sort((a, b) => a.timestamp - b.timestamp);
            }
        }
    });
}

// 直接加载两个用户之间消息的函数
async function loadDirectMessages(otherUsername) {
    if (!currentUsername || !otherUsername) return;
    
    try {
        console.log(`加载与${otherUsername}的对话...`);
        
        // 获取与特定用户的对话
        const response = await fetch(`/private/messages?from=${encodeURIComponent(currentUsername)}&to=${encodeURIComponent(otherUsername)}&lastTimestamp=0`, {
            method: 'GET',
            credentials: 'include'
        });
        
        if (!response.ok) {
            throw new Error(`获取消息失败: ${response.status}`);
        }
        
        const data = await response.json();
        
        // 检查响应格式
        const messages = Array.isArray(data) ? data : 
                        (data.messages ? data.messages : []);
        
        console.log(`收到 ${messages.length} 条消息`, messages);
        
        if (messages.length > 0) {
            // 直接更新这个对话的消息
            allMessages[otherUsername] = messages;
            
            // 更新最后消息时间戳
            const lastMessage = messages[messages.length - 1];
            if (lastMessage && lastMessage.timestamp) {
                lastMessageTimestamp = lastMessage.timestamp;
            }
            
            // 如果当前选中的用户是我们加载消息的用户，更新聊天界面
            if (selectedUser && selectedUser.username === otherUsername) {
                updateChatView(otherUsername);
            }
        } else {
            // 处理没有消息的情况
            allMessages[otherUsername] = [];
            if (selectedUser && selectedUser.username === otherUsername) {
                document.getElementById('chatMessages').innerHTML = '<div class="empty-message">暂无消息</div>';
            }
        }
    } catch (error) {
        console.error('加载消息失败:', error);
        if (selectedUser && selectedUser.username === otherUsername) {
            document.getElementById('chatMessages').innerHTML = '<div class="empty-message">加载消息失败，请重试</div>';
        }
    }
}

// 更新聊天视图函数，确保正确显示消息
function updateChatView(username) {
    const chatMessages = document.getElementById('chatMessages');
    chatMessages.innerHTML = '';
    
    if (!allMessages[username] || allMessages[username].length === 0) {
        chatMessages.innerHTML = '<div class="empty-message">暂无消息</div>';
        return;
    }
    
    console.log(`渲染 ${allMessages[username].length} 条消息`);
    
    // 渲染该用户的所有消息
    allMessages[username].forEach(message => {
        renderSingleMessage(message);
    });
    
    // 滚动到底部
    scrollToBottom();
    
    // 标记为已读
    if (selectedUser) {
        markMessagesAsRead(currentUsername, selectedUser.username);
    }
}

// 发送消息
async function sendMessage() {
    if (!selectedUser || !currentUsername) return;
    
    const messageInput = document.getElementById('messageInput');
    const message = messageInput.value.trim();
    
    if (!message) return;
    
    // 清空输入框
    messageInput.value = '';
    
    console.log(`准备发送消息给: ${selectedUser.username}, 内容: ${message}`);
    
    try {
        // 立即在UI中显示消息（乐观更新）
        const optimisticMessage = {
            user: currentUsername,
            message: Base64.encode(message),
            timestamp: Math.floor(Date.now() / 1000), // 当前时间戳，秒为单位
            is_read: 0,
            metadata: {
                to: selectedUser.username
            }
        };
        
        // 添加到当前会话的消息中
        if (!allMessages[selectedUser.username]) {
            allMessages[selectedUser.username] = [];
        }
        allMessages[selectedUser.username].push(optimisticMessage);
        
        // 更新UI
        updateChatView(selectedUser.username);
        
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
        
        console.log('消息发送成功，立即加载新消息');
        
        // 延迟一小段时间后重新加载消息，确保服务器有时间处理
        setTimeout(() => {
            loadDirectMessages(selectedUser.username);
        }, 500);
    } catch (error) {
        console.error('发送消息失败:', error);
        alert('发送消息失败，请重试');
    }
}

// 渲染单条消息到聊天界面
function renderSingleMessage(message) {
    if (!message) {
        console.error('渲染空消息');
        return;
    }
    
    console.log('渲染消息:', message);
    
    if (!message.user || !message.message) {
        console.error('消息缺少必要字段:', message);
        return;
    }
    
    const chatMessages = document.getElementById('chatMessages');
    
    // 创建消息容器
    const messageElement = document.createElement('div');
    
    // 确定消息类型（发送/接收）
    const isSentByMe = message.user === currentUsername;
    messageElement.className = `message ${isSentByMe ? 'sent' : 'received'}`;
    
    // 提取元数据中的接收者信息
    let receiver = "";
    if (message.metadata) {
        if (typeof message.metadata === 'string') {
            try {
                const metadata = JSON.parse(message.metadata);
                receiver = metadata.to || "";
            } catch (e) {
                console.error('解析metadata��败:', e, message.metadata);
            }
        } else if (typeof message.metadata === 'object') {
            receiver = message.metadata.to || "";
        }
    }
    
    // 解码消息内容
    let messageText = "";
    try {
        messageText = Base64.decode(message.message);
    } catch (e) {
        console.error('解码消息内容失败:', e, message.message);
        messageText = message.message; // 如果解码失败，显示原始内容
    }
    
    // 设置消息内容
    messageElement.textContent = messageText;
    
    // 添加时间戳
    const timeElement = document.createElement('span');
    timeElement.className = 'message-time';
    timeElement.textContent = formatTimestamp(message.timestamp);
    messageElement.appendChild(timeElement);
    
    // 添加到聊天区域
    chatMessages.appendChild(messageElement);
}

// 滚动到聊天窗口底部
function scrollToBottom() {
    const chatMessages = document.getElementById('chatMessages');
    chatMessages.scrollTop = chatMessages.scrollHeight;
}

// 更新未读指示器
function updateUnreadIndicators() {
    userList.forEach(user => {
        if (user.username !== currentUsername) {
            checkUnreadWithUser(user.username);
        }
    });
}

// 修复markMessagesAsRead函数
function markMessagesAsRead(fromUser, toUser) {
    fetch('/private/mark-read', {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json'
        },
        body: JSON.stringify({
            from: fromUser,
            to: toUser
        }),
        credentials: 'include'
    }).catch(error => {
        console.error('标记消息为已读���败:', error);
    });
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

// 初始化事��监听
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
    
    // 初始加载所有消息
    await loadAllUserMessages();
    
    console.log('页面初始化完成');
    
    // 设置定期轮询
    messagePollingInterval = setInterval(loadAllUserMessages, 5000);
}

// 页��加载完成后初始化
document.addEventListener('DOMContentLoaded', initPage);

// 页面卸载前清理
window.addEventListener('beforeunload', () => {
    if (messagePollingInterval) {
        clearInterval(messagePollingInterval);
    }
    if (checkUnreadInterval) {
        clearInterval(checkUnreadInterval);
    }
});

// 搜索用户
function searchUsers(query) {
    if (!query) {
        renderUserList(userList);
        return;
    }
    
    query = query.toLowerCase();
    const filteredUsers = userList.filter(user => 
        user.username.toLowerCase().includes(query)
    );
    
    renderUserList(filteredUsers);
}