// 全局变量
let currentUsername = "";
let selectedUser = null;
let lastMessageTimestamp = 0;
let messagePollingInterval = null;
let userList = [];
let checkUnreadInterval = null;
let allMessages = {}; // 存储所有聊天消息，按用户分组

// 新增高性能轮询相关全局变量
let privatePollingTimer = null;
let privateIsPollingActive = true;
let privateFetchInterval = 500;

// 优化相关变量
let isFirstUserListRender = true; // 是否首次渲染用户列表
let lastUserListState = new Map(); // 缓存用户列表状态
let lastChatMessagesCount = 0; // 上次聊天消息数量
let readStatusCache = new Set(); // 已读状态缓存，避免重复设置
let isFirstMessageLoad = true; // 是否首次加载消息

// 新增所有用户���表相关变量
let allUserListState = {
    users: [],
    startUid: 1,
    endUid: 10,
    pageSize: 10,
    totalLoaded: 0,
    hasMoreUsers: true,
    searchTerm: '',
    loading: false
};

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

        // 只在首次加载时显示加载状态
        if (isFirstUserListRender) {
            document.getElementById('userList').innerHTML = '<div class="empty-message">加载中...</div>';
        }

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

// 所有用户列表相关功能
// 获取所有用户列表(分页)
async function fetchAllUserList(start, end, pageSize = 10) {
    if (!await checkLoginStatus()) return [];
    
    allUserListState.loading = true;
    updateAllUserListLoadingState(true);
    
    try {
        const response = await fetch(`/api/users?start=${start}&end=${end}&size=${pageSize}`, {
            method: 'GET',
            credentials: 'include',
        });

        if (response.status === 401 || response.status === 403) {
            console.error('获取用户列表失败：未授权');
            return [];
        }

        if (!response.ok) {
            console.error('获取用户列表失败:', response.status);
            return [];
        }

        const data = await response.json();
        allUserListState.totalLoaded = data.totalCount || 0;
        allUserListState.hasMoreUsers = data.users && data.users.length >= pageSize;
        
        return data.users || [];
    } catch (error) {
        console.error('获取用户列表时发生错误:', error);
        return [];
    } finally {
        allUserListState.loading = false;
        updateAllUserListLoadingState(false);
    }
}

// 更新用户列表加载状态
function updateAllUserListLoadingState(isLoading) {
    const userList = document.getElementById('allUserList');
    if (!userList) return;
    
    if (isLoading) {
        // 添加加载动画
        if (!document.getElementById('all-user-list-loader')) {
            const loader = document.createElement('div');
            loader.id = 'all-user-list-loader';
            loader.textContent = '加载中...';
            loader.style.textAlign = 'center';
            loader.style.padding = '10px';
            loader.style.color = '#888';
            userList.appendChild(loader);
        }
    } else {
        // 移除加载动画
        const loader = document.getElementById('all-user-list-loader');
        if (loader) {
            loader.remove();
        }
    }
}

// 渲染所有用户列表
function renderAllUserList(users) {
    const userList = document.getElementById('allUserList');
    userList.innerHTML = '';
    
    if (!users || users.length === 0) {
        userList.innerHTML = '<div class="empty-message">没有找到用户</div>';
        return;
    }
    
    // 使用文档片段优化性能
    const fragment = document.createDocumentFragment();
    
    // 过滤掉当前用户和被封禁的用户
    const filteredUsers = users.filter(user => 
        user.username !== currentUsername && 
        user.labei !== 'BAN'
    );
    
    if (filteredUsers.length === 0) {
        userList.innerHTML = '<div class="empty-message">没有找到可聊天的用户</div>';
        return;
    }
    
    filteredUsers.forEach((user, index) => {
        const card = document.createElement('div');
        card.className = 'user-item';
        card.dataset.username = user.username;
        card.style.animationDelay = `${index * 0.03}s`;
        
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
        userStatus.textContent = user.labei === 'GM' ? '管理员' : '';
        
        userInfo.appendChild(userName);
        userInfo.appendChild(userStatus);
        card.appendChild(avatar);
        card.appendChild(userInfo);
        
        // 添加点击事件处理
        card.addEventListener('click', () => {
            selectUser(user);
        });
        
        fragment.appendChild(card);
    });
    
    userList.appendChild(fragment);
    
    // 更新分页信息
    updatePaginationInfo();
}

// 更新分页信息
function updatePaginationInfo() {
    const paginationInfo = document.getElementById('pagination-info');
    if (!paginationInfo) return;
    
    paginationInfo.textContent = `显示 ${allUserListState.startUid}-${allUserListState.endUid} 共 ${allUserListState.totalLoaded} 个用户`;
    
    // 更新分页按钮状态
    const prevButton = document.getElementById('prev-page');
    const nextButton = document.getElementById('next-page');
    
    if (prevButton) {
        prevButton.disabled = allUserListState.startUid <= 1;
    }
    
    if (nextButton) {
        nextButton.disabled = !allUserListState.hasMoreUsers;
    }
}

// 加载上一页用户
async function loadPreviousPage() {
    if (allUserListState.startUid <= 1 || allUserListState.loading) return;
    
    const newEndUid = allUserListState.startUid - 1;
    const newStartUid = Math.max(1, newEndUid - allUserListState.pageSize + 1);
    
    allUserListState.startUid = newStartUid;
    allUserListState.endUid = newEndUid;
    
    const users = await fetchAllUserList(newStartUid, newEndUid, allUserListState.pageSize);
    if (users.length > 0) {
        allUserListState.users = users;
        renderAllUserList(users);
    }
}

// 加载下一页用户
async function loadNextPage() {
    if (!allUserListState.hasMoreUsers || allUserListState.loading) return;
    
    const newStartUid = allUserListState.endUid + 1;
    const newEndUid = newStartUid + allUserListState.pageSize - 1;
    
    allUserListState.startUid = newStartUid;
    allUserListState.endUid = newEndUid;
    
    const users = await fetchAllUserList(newStartUid, newEndUid, allUserListState.pageSize);
    if (users.length > 0) {
        allUserListState.users = users;
        renderAllUserList(users);
    } else {
        // 如果没有更多用户，回退到上一页
        allUserListState.hasMoreUsers = false;
        allUserListState.startUid = Math.max(1, newStartUid - allUserListState.pageSize);
        allUserListState.endUid = newEndUid - allUserListState.pageSize;
        updatePaginationInfo();
    }
}

// 搜索所有用户
function searchAllUsers(term) {
    allUserListState.searchTerm = term.toLowerCase();
    
    if (!term) {
        renderAllUserList(allUserListState.users);
        return;
    }
    
    const filteredUsers = allUserListState.users.filter(user => 
        user.username.toLowerCase().includes(term) || 
        (user.uid && user.uid.toString().includes(term))
    );
    
    renderAllUserList(filteredUsers);
}

// 初始化所有用户列表
async function initAllUserList() {
    // 绑定分页按钮事件
    const prevButton = document.getElementById('prev-page');
    const nextButton = document.getElementById('next-page');
    
    if (prevButton) {
        prevButton.addEventListener('click', loadPreviousPage);
    }
    
    if (nextButton) {
        nextButton.addEventListener('click', loadNextPage);
    }
    
    // 绑定搜索事件
    const searchInput = document.getElementById('allUserSearchInput');
    if (searchInput) {
        searchInput.addEventListener('input', (e) => {
            searchAllUsers(e.target.value);
        });
    }
    
    // 加载初始用户列表
    const users = await fetchAllUserList(
        allUserListState.startUid, 
        allUserListState.endUid, 
        allUserListState.pageSize
    );
    allUserListState.users = users;
    renderAllUserList(users);
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

    // 重置聊天消息计数
    lastChatMessagesCount = 0;

    // 直接加载与该用户的对话
    loadDirectMessages(user.username);

    // 聚焦输入框
    document.getElementById('messageInput').focus();

    // 【修复】只有当前用户是接收者时，才标记消息为已读
    // 这里需要检查是否有来自该用户的未读消息，如果有才标记为已读
    if (hasUnreadFromUser(user.username)) {
        updateMessagesReadStatusForReceiver(user.username);

        // 标记与该用户的消息为已读（避免重复设置）
        const readKey = `${user.username}-${currentUsername}`;
        if (!readStatusCache.has(readKey)) {
            markMessagesAsRead(user.username, currentUsername);
            readStatusCache.add(readKey);
        }
    }
}

// 【修复】检查是否有来自特定用户的未读消息 - 只检查对方发送给我的未读消息
function hasUnreadFromUser(username) {
    if (!allMessages[username] || allMessages[username].length === 0) return false;
    return allMessages[username].some(msg =>
        msg.user === username && // 消息是对方发送的
        Number(msg.is_read) === 0 // 且未读
    );
}

// 优化：渲染用户列表，避免不必要的重新渲染和动画
function renderUserList(users) {
    // 对用户列表排序：有未读消息的用户移到前面
    const sortedUsers = [...users].sort((a, b) => {
        if (hasUnread(a) && !hasUnread(b)) return -1;
        if (!hasUnread(a) && hasUnread(b)) return 1;
        return 0;
    });

    const userListElement = document.getElementById('userList');

    if (!sortedUsers || sortedUsers.length === 0) {
        userListElement.innerHTML = '<div class="empty-message">没有找到用户</div>';
        return;
    }

    // 检查用户列表状态是否发生变化
    const currentState = new Map();
    sortedUsers.forEach(user => {
        currentState.set(user.username, {
            hasUnread: hasUnread(user),
            labei: user.labei
        });
    });

    // 比较状态，如果没有变化且不是首次渲染，则跳过重新渲染
    if (!isFirstUserListRender && !isFirstMessageLoad && mapsEqual(lastUserListState, currentState)) {
        return;
    }

    // 更新状态缓存
    lastUserListState = new Map(currentState);

    // 清空现有列表
    userListElement.innerHTML = '';

    const fragment = document.createDocumentFragment();

    sortedUsers.forEach((user, index) => {
        const userItem = document.createElement('div');
        userItem.className = 'user-item';
        userItem.dataset.username = user.username;

        // 只在首次渲染时添加动画延迟
        if (isFirstUserListRender) {
            userItem.style.animationDelay = `${index * 0.03}s`;
        }

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
        userStatus.textContent = user.labei === 'GM' ? '管理员' : '';

        userInfo.appendChild(userName);
        userInfo.appendChild(userStatus);
        userItem.appendChild(avatar);
        userItem.appendChild(userInfo);

        // 添加未读消息指示器
        const unreadBadge = document.createElement('div');
        unreadBadge.className = 'user-badge';
        const userHasUnread = hasUnread(user);
        unreadBadge.style.display = userHasUnread ? 'flex' : 'none';
        unreadBadge.textContent = '!';
        userItem.appendChild(unreadBadge);

        // 添加点击事件处理
        userItem.addEventListener('click', () => {
            selectUser(user);
        });

        fragment.appendChild(userItem);
    });

    userListElement.appendChild(fragment);

    // 标记不再是首次渲染
    isFirstUserListRender = false;

    // 保留原有定时检查未读逻辑
    startCheckingUnread();
}

// 辅助函数：比较两个Map是否相等
function mapsEqual(map1, map2) {
    if (map1.size !== map2.size) return false;

    for (let [key, value] of map1) {
        if (!map2.has(key)) return false;
        const value2 = map2.get(key);
        if (value.hasUnread !== value2.hasUnread || value.labei !== value2.labei) {
            return false;
        }
    }
    return true;
}

// 【核心修复】判断用户对话中是否有未读消息 - 只检查对方发送给我的未读消息
function hasUnread(user) {
    if (!allMessages[user.username] || allMessages[user.username].length === 0) return false;

    // 【关键修复】只检查对方发送给我的未读消息，不检查我发送给对方的消息
    return allMessages[user.username].some(msg => {
        // 检查消息的接收者是否是当前用户
        let messageReceiver = "";

        // 从metadata中提取接收者
        if (msg.metadata) {
            if (typeof msg.metadata === 'string') {
                try {
                    const metadata = JSON.parse(msg.metadata);
                    messageReceiver = metadata.to || "";
                } catch (e) {
                    console.error('解析metadata失败:', e);
                }
            } else if (typeof msg.metadata === 'object') {
                messageReceiver = msg.metadata.to || "";
            }
        }

        // 只有当消息是对方发送给我的，且未读时，才算作未读消息
        return msg.user === user.username && // 消息发送者是对方
            messageReceiver === currentUsername && // 消息接收者是我
            Number(msg.is_read) === 0; // 且未读
    });
}

// 修改：使用高性能轮询获取私聊消息，减少不必要的渲染
async function fetchPrivateMessages() {
    if (!privateIsPollingActive) return;
    try {
        // 更新所有用户消息
        await loadAllUserMessages();

        // 【修复】不要在轮询中自动标记已读
        // 如果已选中对���，只更新聊天界面，不标记已读
        if (selectedUser) {
            await loadDirectMessages(selectedUser.username);
        }

        // 只在用户列表状态真正发生变化时才重新渲染
        renderUserList(userList);
    } catch (error) {
        console.error("Private polling error:", error);
    } finally {
        adjustPrivateFetchInterval();
        privatePollingTimer = setTimeout(fetchPrivateMessages, privateFetchInterval);
    }
}

// 调整私聊消息轮询间隔
function adjustPrivateFetchInterval() {
    if (document.hidden) {
        privateFetchInterval = 10000;
    } else if (document.hasFocus()) {
        privateFetchInterval = 500;
    } else {
        privateFetchInterval = 500;
    }
}

// 聊天功能相关
// 加载当前用户的所有私聊消息
async function loadAllUserMessages() {
    if (!currentUsername) return;

    try {
        const response = await fetch(`/private/messages?to=${encodeURIComponent(currentUsername)}&lastTimestamp=${lastMessageTimestamp}`, {
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

        // 首次加载完成后，标记不再是第一次
        if (isFirstMessageLoad) {
            isFirstMessageLoad = false;
            // 首次加载后立即渲染用户列表以显示未读状态
            renderUserList(userList);
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

            // 检查是否已存在相同的消息（避免重复）
            const isDuplicate = allMessages[chatPartner].some(
                m => m.timestamp === message.timestamp &&
                    m.user === message.user &&
                    m.message === message.message
            );

            if (!isDuplicate) {
                // 【修复】确保 is_read 字段保持服务器返回的原始状态
                if (message.is_read === undefined || message.is_read === null) {
                    message.is_read = 0; // 默认为未读
                }

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
            // 【修复】确保每条消息都保持服务器返回的原始 is_read 状态
            messages.forEach(message => {
                if (message.is_read === undefined || message.is_read === null) {
                    message.is_read = 0; // 默认为未读
                }
            });

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

// 【修复】更新前端消息数组中的已读状态 - 只为接收者更新
function updateMessagesReadStatusForReceiver(username) {
    if (allMessages[username]) {
        let hasChanges = false;
        // 【修复】只将对方发送给我的未读消息标记为已读
        allMessages[username].forEach(msg => {
            // 检查消息的接收者是否是当前用户
            let messageReceiver = "";

            // 从metadata中提取接收者
            if (msg.metadata) {
                if (typeof msg.metadata === 'string') {
                    try {
                        const metadata = JSON.parse(msg.metadata);
                        messageReceiver = metadata.to || "";
                    } catch (e) {
                        console.error('解析metadata失败:', e);
                    }
                } else if (typeof msg.metadata === 'object') {
                    messageReceiver = msg.metadata.to || "";
                }
            }

            // 只更新对方发送给我的未读消息
            if (msg.user === username && // 消息发送者是对方
                messageReceiver === currentUsername && // 消息接收者是我
                Number(msg.is_read) === 0) { // 且未读
                msg.is_read = 1;
                hasChanges = true;
            }
        });

        // 如果有更改且当前正在查看该用户的消息，则更新聊天界面
        if (hasChanges && selectedUser && selectedUser.username === username) {
            updateChatView(username);
        }

        // 更新用户列表中的未读指示器
        checkUnreadWithUser(username);
    }
}

// 优化：更新聊天视图函数，避免不必要的重新渲染
function updateChatView(username) {
    const chatMessages = document.getElementById('chatMessages');

    if (!allMessages[username] || allMessages[username].length === 0) {
        if (lastChatMessagesCount !== 0) {
            chatMessages.innerHTML = '<div class="empty-message">暂无消息</div>';
            lastChatMessagesCount = 0;
        }
        return;
    }

    const currentMessagesCount = allMessages[username].length;

    // 如果消息数量没有变化，不需要重新渲染
    if (currentMessagesCount === lastChatMessagesCount) {
        return;
    }

    console.log(`渲染 ${currentMessagesCount} 条消息`);

    // 如果是新增消息（不是完全重新加载），只渲染新消息
    if (currentMessagesCount > lastChatMessagesCount && lastChatMessagesCount > 0) {
        // 只渲染新增的消息
        const newMessages = allMessages[username].slice(lastChatMessagesCount);
        newMessages.forEach(message => {
            renderSingleMessage(message);
        });
    } else {
        // 完全重新渲染所有消息
        chatMessages.innerHTML = '';
        allMessages[username].forEach(message => {
            renderSingleMessage(message);
        });
    }

    lastChatMessagesCount = currentMessagesCount;

    // 滚动到底部
    scrollToBottom();
}

// 【修复】发送消息 - 不要调用已读相关函数
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
            timestamp: Math.floor(Date.now() / 1000),
            is_read: 0, // 【修复】新发送的消息对接收者来说是未读的
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

        console.log('消息发送成功');

        // 【修复】发送消息后不要调用任何已读相关函数
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

    // 解码消息内容
    let messageText = "";
    try {
        messageText = Base64.decode(message.message);
    } catch (e) {
        console.error('解码消息内容失败:', e, message.message);
        messageText = message.message;
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

// 【修复】标记已读函数 - 参数调整为 (发送者, 接收者)
function markMessagesAsRead(fromUser, toUser) {
    const readKey = `${fromUser}-${toUser}`;

    // 如果已经标记过已读，跳过
    if (readStatusCache.has(readKey)) {
        return;
    }

    fetch('/private/mark-read', {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json'
        },
        body: JSON.stringify({ from: fromUser, to: toUser }),
        credentials: 'include'
    }).then(response => {
        if (response.ok) {
            // 添加到缓存
            readStatusCache.add(readKey);

            // 更新UI中的未读标记
            const userItem = document.querySelector(`[data-username="${fromUser}"]`);
            if (userItem) {
                const badge = userItem.querySelector('.user-badge');
                if (badge) {
                    badge.style.display = 'none';
                }
            }

            // 更新本地缓存，将对应对话所有未读消息标记为已读
            updateMessagesReadStatusForReceiver(fromUser);
        } else {
            console.error("标记已读失败，状态码:", response.status);
        }
    }).catch(error => {
        console.error('标记消息为已���失败:', error);
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

// 防抖函数，优化搜索性能
function debounce(func, wait) {
    let timeout;
    return function executedFunction(...args) {
        const later = () => {
            clearTimeout(timeout);
            func(...args);
        };
        clearTimeout(timeout);
        timeout = setTimeout(later, wait);
    };
}

// 初始化事件监听
function setupEventListeners() {
    // 搜索框事件（添加防抖）
    const searchInput = document.getElementById('userSearchInput');
    const debouncedSearch = debounce((value) => {
        searchUsers(value);
    }, 300);

    searchInput.addEventListener('input', (e) => {
        debouncedSearch(e.target.value);
    });

    // 所有用户搜索框事件（添加防抖）
    const allUserSearchInput = document.getElementById('allUserSearchInput');
    const debouncedAllUserSearch = debounce((value) => {
        searchAllUsers(value);
    }, 300);

    allUserSearchInput.addEventListener('input', (e) => {
        debouncedAllUserSearch(e.target.value);
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

    // 页面可见性变化时调整轮询频率
    document.addEventListener('visibilitychange', () => {
        adjustPrivateFetchInterval();
    });

    // 页面获得/失去焦点时调整轮询频率
    window.addEventListener('focus', () => {
        adjustPrivateFetchInterval();
    });

    window.addEventListener('blur', () => {
        adjustPrivateFetchInterval();
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

    // 初始化所有用户列表
    await initAllUserList();

    // 初始加载所有消息
    await loadAllUserMessages();

    console.log('页面初始化完成');

    // 启动高性能轮询
    privateIsPollingActive = true;
    fetchPrivateMessages();
}

// 页面加载完成后初始化
document.addEventListener('DOMContentLoaded', initPage);

// 页面卸载前清理
window.addEventListener('beforeunload', () => {
    if (privatePollingTimer) {
        clearTimeout(privatePollingTimer);
    }
    if (checkUnreadInterval) {
        clearInterval(checkUnreadInterval);
    }
    privateIsPollingActive = false;
});

// 优化：搜索用户，添加性能优化
function searchUsers(query) {
    if (!query) {
        renderUserList(userList);
        return;
    }

    query = query.toLowerCase();
    const filteredUsers = userList.filter(user =>
        user.username.toLowerCase().includes(query)
    );

    // 重置渲染状态，因为这是一个新的搜索结果
    const originalFirstRender = isFirstUserListRender;
    isFirstUserListRender = true;
    renderUserList(filteredUsers);
    isFirstUserListRender = originalFirstRender;
}

// 优化：定时检查未读消息状态
function startCheckingUnread() {
    // 清除现有的定时器
    if (checkUnreadInterval) {
        clearInterval(checkUnreadInterval);
    }

    // 设置新的定时器，间隔更长以减少性能影响
    checkUnreadInterval = setInterval(() => {
        updateUnreadIndicators();
    }, 5000); // 5秒检查一次
}

// 【修复】检查特定用户的未读状态 - 只检查对方发送给我的未读消息
function checkUnreadWithUser(username) {
    if (!allMessages[username]) return;

    // 【关键修复】只检查对方发送给我的未读消息
    const hasUnreadMessages = allMessages[username].some(msg => {
        // 检查消息的接收者是否是当前用户
        let messageReceiver = "";

        // 从metadata中提取接收者
        if (msg.metadata) {
            if (typeof msg.metadata === 'string') {
                try {
                    const metadata = JSON.parse(msg.metadata);
                    messageReceiver = metadata.to || "";
                } catch (e) {
                    console.error('解析metadata失败:', e);
                }
            } else if (typeof msg.metadata === 'object') {
                messageReceiver = msg.metadata.to || "";
            }
        }

        // 只检查对方发送给我的未读消息
        return msg.user === username && // 消息发送者是对方
            messageReceiver === currentUsername && // 消息接收者是我
            Number(msg.is_read) === 0; // 且未读
    });

    // 更新UI中的未读标记
    const userItem = document.querySelector(`[data-username="${username}"]`);
    if (userItem) {
        const badge = userItem.querySelector('.user-badge');
        if (badge) {
            badge.style.display = hasUnreadMessages ? 'flex' : 'none';
        }
    }
}

// 优化：清理已读状态缓存的函数
function clearReadStatusCache() {
    // 定期清理缓存，防止内存泄漏
    if (readStatusCache.size > 1000) {
        readStatusCache.clear();
    }
}

// 定期清理缓存
setInterval(clearReadStatusCache, 300000); // 5分钟清理一次