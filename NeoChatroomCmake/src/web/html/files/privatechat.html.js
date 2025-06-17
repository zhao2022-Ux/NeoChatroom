// 全局变量
let currentUsername = "";
let selectedUser = null;
let lastMessageTimestamp = 0; // 用于轮询时获取增量消息
let isLoadingAllMessages = false; // 防止并发调用 loadAllUserMessages
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

// 【新增】用户列表未读状态检查定时器
let userListUnreadCheckTimer = null;

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

        // 【修改】过滤用户：只显示有聊天消息的用户（排除自己和被封禁的用户）
        userList = allUsers.filter(user =>
            user.username !== currentUsername &&
            user.labei !== 'BAN' &&
            hasMessagesWith(user.username) // 新增条件：必须有聊天消息
        );

        renderUserList(userList);
    } catch (error) {
        console.error('获取用户列表失败:', error);
        document.getElementById('userList').innerHTML = '<div class="empty-message">获取用户列表失败</div>';
    }
}

// 【新增】检查是否与某个用户有聊天消息的函数
function hasMessagesWith(username) {
    // 如果 allMessages 中没有该用户的消息记录，返回 false
    if (!allMessages[username] || allMessages[username].length === 0) {
        return false;
    }

    // 检查是否存在与该用户的有效聊天消息
    return allMessages[username].some(msg => {
        // 检查消息的发送者和接收者
        let messageReceiver = "";

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

        // 返回 true 如果：
        // 1. 当前用户发送给该用户的消息，或
        // 2. 该用户发送给当前用户的消息
        return (msg.user === currentUsername && messageReceiver === username) ||
            (msg.user === username && messageReceiver === currentUsername);
    });
}

// 【核心修复】选择用户开始聊天 - 立即标记对方发来的消息为已读
async function selectUser(user) {
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

    // 直接从 allMessages 加载与该用户的对话
    await loadDirectMessages(user.username);

    // 【修复】在加载消息后，立即将对方发给我的所有未读消息标记为已读
    await markAllMessagesFromUserAsRead(user.username);

    // 聚焦输入框
    document.getElementById('messageInput').focus();

    // 【新增】开始检查其他用户的未读状态
    startUserListUnreadCheck();
}

// 【新增】将特定用户发给当前用户的所有未读消息标记为已读
async function markAllMessagesFromUserAsRead(fromUsername) {
    if (!fromUsername || !currentUsername) return;

    console.log(`标记来自 ${fromUsername} 的所有消息为已读`);

    // 1. 先在前端更新已读状态
    updateMessagesReadStatusInFrontend(fromUsername);

    // 2. 发送到后端标记已读
    try {
        const response = await fetch('/private/mark-read', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify({
                from: fromUsername,
                to: currentUsername
            }),
            credentials: 'include'
        });

        if (response.ok) {
            console.log(`成功标记来自 ${fromUsername} 的消息为已读`);

            // 3. 更新已读状态缓存
            const readKey = `${fromUsername}-${currentUsername}`;
            readStatusCache.add(readKey);

            // 4. 更新用户列表中的未读指示器
            checkUnreadWithUser(fromUsername);

            // 5. 更新聊天界面
            if (selectedUser && selectedUser.username === fromUsername) {
                updateChatView(fromUsername);
            }
        } else {
            console.error("后端标记已读失败，状态码:", response.status);
        }
    } catch (error) {
        console.error('标记消息为已读失败:', error);
    }
}

// 【新增】在前端更新特定用户发来的消息的已读状态
function updateMessagesReadStatusInFrontend(fromUsername) {
    if (!allMessages[fromUsername]) return;

    let hasChanges = false;

    // 遍历该用户的所有消息，将对方发给我的未读消息标记为已读
    allMessages[fromUsername].forEach(msg => {
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
        if (msg.user === fromUsername && // 消息发送者是对方
            messageReceiver === currentUsername && // 消息接收者是我
            Number(msg.is_read) === 0) { // 且未读
            msg.is_read = 1;
            hasChanges = true;
            console.log(`前端标记消息为已读: ${msg.message.substring(0, 20)}...`);
        }
    });

    if (hasChanges) {
        console.log(`前端已更新来自 ${fromUsername} 的消息已读状态`);
    }
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

// 【修复】检查是否有来自特定用户的未读消息 - 只检查对方发送给我的未读消息
function hasUnreadFromUser(username) {
    if (!allMessages[username] || allMessages[username].length === 0) return false;
    return allMessages[username].some(msg =>
        msg.user === username && // 消息是对方发送的
        Number(msg.is_read) === 0 && // 且未读
        isMessageToCurrentUser(msg) // 且是发送给我的
    );
}

// 【新增】检查消息是否是发送给当前用户的
function isMessageToCurrentUser(msg) {
    if (!msg.metadata) return false;

    let messageReceiver = "";

    // 从metadata中提取接收者
    if (typeof msg.metadata === 'string') {
        try {
            const metadata = JSON.parse(msg.metadata);
            messageReceiver = metadata.to || "";
        } catch (e) {
            console.error('解析metadata失败:', e);
            return false;
        }
    } else if (typeof msg.metadata === 'object') {
        messageReceiver = msg.metadata.to || "";
    }

    return messageReceiver === currentUsername;
}

// 【修改】优化：渲染用户列表，避免不必要的重新渲染和动画，并在消息加载后重新过滤
function renderUserList(users) {
    // 【新增】重新过滤用户列表：只显示有聊天消息的用户
    const usersWithMessages = users.filter(user => hasMessagesWith(user.username));

    // 对用户列表排序：有未读消息的用户移到前面
    const sortedUsers = [...usersWithMessages].sort((a, b) => {
        if (hasUnread(a) && !hasUnread(b)) return -1;
        if (!hasUnread(a) && hasUnread(b)) return 1;
        return 0;
    });

    const userListElement = document.getElementById('userList');

    if (!sortedUsers || sortedUsers.length === 0) {
        userListElement.innerHTML = '<div class="empty-message">暂无聊天用户</div>';
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

// 修改：使用高性能轮询获取私聊消息，减少不必要的渲染
async function fetchPrivateMessages() {
    if (!privateIsPollingActive || isLoadingAllMessages) return; // 防止并发执行
    isLoadingAllMessages = true;
    try {
        // 更新所有用户消息
        await loadAllUserMessages(); // 这将获取所有与当前用户相关的消息

        // 如果已选中对话，更新聊天界面
        if (selectedUser) {
            // updateChatView 会从 allMessages 获取数据
            updateChatView(selectedUser.username);
        }

        // 【修改】更新用户列表的未读状态，并重新过滤显示有聊天消息的用户
        renderUserList(userList); // renderUserList 会使用 allMessages 判断未读
        updateAllUsersUnreadStatus(); // 确保未读标记实时更新

    } catch (error) {
        console.error("Private polling error:", error);
    } finally {
        isLoadingAllMessages = false;
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

// 【新增】用户列表未读状态检查功能
function startUserListUnreadCheck() {
    // 清除现有的定时器
    if (userListUnreadCheckTimer) {
        clearInterval(userListUnreadCheckTimer);
    }

    // 设置定时器，每5秒检查一次用户列表的未读状态
    userListUnreadCheckTimer = setInterval(() => {
        updateAllUsersUnreadStatus();
    }, 5000); // 5秒检查一次
}

// 【新增】更新所有用户的未读状态显示
function updateAllUsersUnreadStatus() {
    if (!userList || userList.length === 0) return;

    // console.log('检查所有用户未读状态...');

    let hasChanges = false;

    userList.forEach(user => {
        if (user.username !== currentUsername) {
            const currentUnreadStatus = hasUnread(user);
            const userItem = document.querySelector(`[data-username="${user.username}"]`);

            if (userItem) {
                const badge = userItem.querySelector('.user-badge');
                if (badge) {
                    const isCurrentlyVisible = badge.style.display === 'flex';

                    // 如果状态发生变化，更新显示
                    if (currentUnreadStatus !== isCurrentlyVisible) {
                        badge.style.display = currentUnreadStatus ? 'flex' : 'none';
                        hasChanges = true;
                        // console.log(`用户 ${user.username} 未读状态变化: ${currentUnreadStatus}`);
                    }
                }
            }
        }
    });

    // 如果有变化，renderUserList 会在下一次轮询时处理排序和重绘
    // 因此移除 reorderUserListByUnreadStatus() 的直接调用
    if (hasChanges) {
        // console.log('检测到未读状态变化，将由 renderUserList 处理用户列表排序');
    }
}

// 【新增】根据未读状态重新排序用户列表（不重新渲染）
// 此函数不再从 updateAllUsersUnreadStatus 调用，renderUserList 会处理排序
function reorderUserListByUnreadStatus() {
    const userListElement = document.getElementById('userList');
    const userItems = Array.from(userListElement.querySelectorAll('.user-item'));

    // 按未读状态排序
    userItems.sort((a, b) => {
        const usernameA = a.dataset.username;
        const usernameB = b.dataset.username;

        const userA = userList.find(u => u.username === usernameA);
        const userB = userList.find(u => u.username === usernameB);

        if (!userA || !userB) return 0;

        const hasUnreadA = hasUnread(userA);
        const hasUnreadB = hasUnread(userB);

        if (hasUnreadA && !hasUnreadB) return -1;
        if (!hasUnreadA && hasUnreadB) return 1;
        return 0;
    });

    // 重新排序DOM元素
    userItems.forEach(item => {
        userListElement.appendChild(item);
    });
}

// 聊天功能相关
// 加载当前用户的所有私聊消息（支持分页和增量加载）
async function loadAllUserMessages() {
    if (!currentUsername) return;

    try {
        // 对于轮询，我们通常获取从 lastMessageTimestamp 开始的所有新消息
        // 后端 getUserMessages 内部的 getUserRelatedMessages 有 LIMIT 100，
        // 所以如果一次轮询有很多新消息，可能需要多次获取，但目前简化处理，一次获取一批
        const pageSizeForPolling = 1000; // 获取足够多的新消息
        const response = await fetch(`/private/messages?lastTimestamp=${lastMessageTimestamp}&page=0&pageSize=${pageSizeForPolling}`, {
            method: 'GET',
            credentials: 'include'
        });

        if (!response.ok) {
            throw new Error(`获取所有相关消息失败: ${response.status}`);
        }

        const messages = await response.json();

        if (Array.isArray(messages) && messages.length > 0) {
            // 处理新消息，将其分类存储到 allMessages
            processNewMessages(messages);

            // 更新 lastMessageTimestamp 为最新收到的消息的时间戳
            // 注意：messages 是按时间升序排列的
            const latestMessageInBatch = messages[messages.length - 1];
            if (latestMessageInBatch && latestMessageInBatch.timestamp > lastMessageTimestamp) {
                lastMessageTimestamp = latestMessageInBatch.timestamp;
            }

            console.log(`Processed ${messages.length} new messages. New lastMessageTimestamp: ${lastMessageTimestamp}`);

            // 如果有选中的用户，并且新消息中有与当前选中用户的对话，则更新聊天界面
            if (selectedUser) {
                // updateChatView 将从 allMessages 中获取数据
                updateChatView(selectedUser.username);
            }

            // 【新增】立即更新所有用户的未读状态，因为可能有新未读消息
            updateAllUsersUnreadStatus();
        }

        // 首次加载完成后，标记不再是第一次
        if (isFirstMessageLoad) {
            isFirstMessageLoad = false;
            // 首次加载后立即渲染用户列表以显示未读状态
            renderUserList(userList);
        }
    } catch (error) {
        console.error('加载所有相关消息失败:', error);
    }
}

// 处理新接收到的消息
function processNewMessages(newMessages) {
    let unreadStatusChangedOverall = false;
    const affectedChatPartners = new Set();

    newMessages.forEach(serverMessage => {
        const sender = serverMessage.user;
        let receiver = "";

        if (serverMessage.metadata) {
            if (typeof serverMessage.metadata === 'string') {
                try {
                    const metadata = JSON.parse(serverMessage.metadata);
                    receiver = metadata.to || "";
                } catch (e) {
                    const match = serverMessage.metadata.match(/"to"\s*:\s*"([^"]*)"/);
                    if (match && match[1]) receiver = match[1];
                }
            } else if (typeof serverMessage.metadata === 'object') {
                receiver = serverMessage.metadata.to || "";
            }
        }

        let chatPartner = "";
        if (sender === currentUsername) {
            chatPartner = receiver;
        } else if (receiver === currentUsername) {
            chatPartner = sender;
        } else {
            return;
        }

        if (chatPartner) {
            affectedChatPartners.add(chatPartner);
            if (!allMessages[chatPartner]) {
                allMessages[chatPartner] = [];
            }

            let replacedOptimistic = false;
            // 如果是当前用户发送的消息，尝试替换乐观更新的消息
            if (serverMessage.user === currentUsername) {
                const optimisticMatchIndex = allMessages[chatPartner].findIndex(
                    optMsg => optMsg.isOptimistic &&
                        optMsg.user === serverMessage.user &&
                        optMsg.message === serverMessage.message && // 比较Base64编码后的内容
                        Math.abs(optMsg.timestamp - serverMessage.timestamp) <= 3 // 时间戳在3秒内认为是同一个
                );

                if (optimisticMatchIndex !== -1) {
                    // console.log("Replacing optimistic message with server version:", serverMessage);
                    allMessages[chatPartner][optimisticMatchIndex] = serverMessage;
                    replacedOptimistic = true;
                }
            }

            if (!replacedOptimistic) {
                // 标准去重逻辑：检查是否已存在完全相同的消息
                const isDuplicate = allMessages[chatPartner].some(
                    m => m.timestamp === serverMessage.timestamp &&
                        m.user === serverMessage.user &&
                        m.message === serverMessage.message &&
                        JSON.stringify(m.metadata) === JSON.stringify(serverMessage.metadata) // 更严格的元数据比较
                );

                if (!isDuplicate) {
                    if (serverMessage.is_read === undefined || serverMessage.is_read === null) {
                        serverMessage.is_read = 0; // 默认为未读
                    }
                    allMessages[chatPartner].push(serverMessage);
                    if (Number(serverMessage.is_read) === 0 && serverMessage.user !== currentUsername && receiver === currentUsername) {
                        unreadStatusChangedOverall = true;
                    }
                }
            }
        }
    });

    // 对所有受影响的聊天伙伴的消息列表进行排序
    affectedChatPartners.forEach(partner => {
        if (allMessages[partner]) {
            allMessages[partner].sort((a, b) => a.timestamp - b.timestamp);
        }
    });

    if (unreadStatusChangedOverall) {
        // console.log("Unread status changed, re-rendering user list and indicators.");
        renderUserList(userList); // 重新渲染用户列表以反映未读状态和排序
        updateAllUsersUnreadStatus(); // 更新徽章
    }
}

// 直接加载两个用户之间消息的函数 - 改为从 allMessages 获取
async function loadDirectMessages(otherUsername) {
    if (!currentUsername || !otherUsername) return;

    console.log(`从 allMessages 加载与 ${otherUsername} 的对话...`);

    // 确保 allMessages 中有该用户的条目
    if (!allMessages[otherUsername]) {
        allMessages[otherUsername] = [];
    }

    // 如果当前选中的用户是我们加载消息的用户，更新聊天界面
    if (selectedUser && selectedUser.username === otherUsername) {
        updateChatView(otherUsername); // updateChatView 会从 allMessages[otherUsername] 读取
    } else if (!selectedUser && isFirstMessageLoad) {
        // 如果是首次加载且没有选中用户，但 allMessages 有数据，也尝试更新视图
        // (这种情况较少，通常 selectUser 会先被调用)
    }

    // 如果没有消息，显示提示
    if (selectedUser && selectedUser.username === otherUsername && (!allMessages[otherUsername] || allMessages[otherUsername].length === 0)) {
        document.getElementById('chatMessages').innerHTML = '<div class="empty-message">暂无消息</div>';
        lastChatMessagesCount = 0; // 重置计数
    }
}

// 【删除旧的已读更新函数，替换为新的前端已读状态更新函数】
// updateMessagesReadStatusForReceiver 函数已被 updateMessagesReadStatusInFrontend 替代

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

    // console.log(`准备发送消息给: ${selectedUser.username}, 内容: ${message}`);

    try {
        // 立即在UI中显示消息（乐观更新）
        const optimisticMessage = {
            user: currentUsername,
            message: Base64.encode(message), // 存储编码后的消息
            timestamp: Math.floor(Date.now() / 1000),
            is_read: 0,
            metadata: {
                to: selectedUser.username
            },
            isOptimistic: true // 标记为乐观消息
        };

        // 添加到当前会话的消息中
        if (!allMessages[selectedUser.username]) {
            allMessages[selectedUser.username] = [];
        }
        allMessages[selectedUser.username].push(optimisticMessage);
        allMessages[selectedUser.username].sort((a, b) => a.timestamp - b.timestamp); // 排序

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
                message: Base64.encode(message) // 发送编码后的消息
            }),
            credentials: 'include'
        });

        if (!response.ok) {
            // 如果发送失败，可以考虑从UI中移除乐观消息或标记为失败
            allMessages[selectedUser.username] = allMessages[selectedUser.username].filter(
                msg => !msg.isOptimistic || msg.message !== optimisticMessage.message // 简易移除
            );
            updateChatView(selectedUser.username); // 刷新视图
            throw new Error(`发送消息失败: ${response.status}`);
        }

    } catch (error) {
        console.error('发送消息失败:', error);
        // alert('发送消息失败，请重试'); // 考虑更友好的错误提示
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

// 【保留但简化】标记已读函数 - 保持向后兼容性
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
            updateMessagesReadStatusInFrontend(fromUser);
        } else {
            console.error("标记已读失败，状态码:", response.status);
        }
    }).catch(error => {
        console.error('标记消息为已读失败:', error);
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

// 【新增】获取全部用户列表的函数（保持原有的 fetchUserList 功能）
async function fetchAllUsers() {
    if (!await checkLoginStatus()) return;

    try {
        // 分页加载更多用户数据
        let allUsers = [];
        let startUid = 1;
        let hasMore = true;
        const pageSize = 100;  // 一次加载更多用户

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

        // 过滤掉自己和被封禁的用户，但不过滤是否有聊天消息
        const allValidUsers = allUsers.filter(user =>
            user.username !== currentUsername &&
            user.labei !== 'BAN'
        );

        console.log(`获取到 ${allValidUsers.length} 个有效用户`);

        // 将完整的用户列表存储在全局变量中，以便后续使用
        // 注意：这里不直接修改 userList，因为 userList 应该只包含有聊天消息的用户
        // 如果需要显示所有用户的功能，可以创建一个新的全局变量

    } catch (error) {
        console.error('获取全部用户列表失败:', error);
    }
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

    // 【修改】先获取全部用户列表，然后获取消息，最后过滤显示
    await fetchAllUsers(); // 获取全部用户列表

    // 初始加载所有与当前用户相关的消息
    // 这会填充 allMessages 对象
    await loadAllUserMessages();

    // 获取用户列表并过滤显示有聊天消息的用户
    await fetchUserList(); // 获取用户列表后，userList 会被填充并过滤

    console.log('页面初始化完成');
    isFirstMessageLoad = false; // 标记初始加载完成

    // 首次渲染用户列表，此时 allMessages 可能已有数据，可以正确显示未读
    renderUserList(userList);
    updateAllUsersUnreadStatus();

    // 启动高性能轮询
    privateIsPollingActive = true;
    fetchPrivateMessages(); // 开始轮询
    startUserListUnreadCheck(); // 开始用户列表未读状态的定期检查
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
    if (userListUnreadCheckTimer) {
        clearInterval(userListUnreadCheckTimer);
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