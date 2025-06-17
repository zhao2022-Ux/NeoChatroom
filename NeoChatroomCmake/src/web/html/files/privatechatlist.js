// 【新增】所有用户列表状态管理
let allUsersState = {
    users: [],           // 所有用户数据
    filteredUsers: [],   // 搜索过滤后的用户
    currentPage: 0,      // 当前页码（从0开始）
    pageSize: 10,        // 每页显示数量
    totalUsers: 0,       // 总用户数
    searchQuery: '',     // 当前搜索词
    isLoading: false     // 加载状态
};

// 【新增】获取所有用户列表（用于分页显示）
async function fetchAllUsers() {
    if (allUsersState.isLoading) return;

    allUsersState.isLoading = true;
    updateAllUsersLoadingState(true);

    try {
        // 获取更多用户数据用于分页显示
        let allUsers = [];
        let startUid = 1;
        let hasMore = true;
        const batchSize = 100;

        while (hasMore && allUsers.length < 1000) { // 最多获取1000个用户
            const endUid = startUid + batchSize - 1;
            const response = await fetch(`/api/users?start=${startUid}&end=${endUid}&size=${batchSize}`, {
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

            if (users.length < batchSize) {
                hasMore = false;
            } else {
                startUid += batchSize;
            }
        }

        // 过滤掉自己和被封禁的用户
        allUsersState.users = allUsers.filter(user =>
            user.username !== currentUsername &&
            user.labei !== 'BAN'
        );

        allUsersState.totalUsers = allUsersState.users.length;
        allUsersState.filteredUsers = [...allUsersState.users];

        // 渲染第一页
        renderAllUsersPage();
        updateAllUsersPagination();

    } catch (error) {
        console.error('获取所有用户失败:', error);
        document.getElementById('private-chat-user-list').innerHTML =
            '<div class="empty-message">获取用户列表失败</div>';
    } finally {
        allUsersState.isLoading = false;
        updateAllUsersLoadingState(false);
    }
}

// 【新增】更新所有用户列表加载状态
function updateAllUsersLoadingState(isLoading) {
    const container = document.getElementById('private-chat-user-list');
    if (!container) return;

    if (isLoading) {
        if (!document.getElementById('all-users-loader')) {
            const loader = document.createElement('div');
            loader.id = 'all-users-loader';
            loader.className = 'empty-message';
            loader.textContent = '加载中...';
            container.appendChild(loader);
        }
    } else {
        const loader = document.getElementById('all-users-loader');
        if (loader) {
            loader.remove();
        }
    }
}

// 【新增】渲染所有用户列表的当前页
function renderAllUsersPage() {
    const container = document.getElementById('private-chat-user-list');
    if (!container) return;

    // 清空容器
    container.innerHTML = '';

    if (allUsersState.filteredUsers.length === 0) {
        container.innerHTML = '<div class="empty-message">没有找到用户</div>';
        return;
    }

    // 计算当前页的用户
    const startIndex = allUsersState.currentPage * allUsersState.pageSize;
    const endIndex = Math.min(startIndex + allUsersState.pageSize, allUsersState.filteredUsers.length);
    const pageUsers = allUsersState.filteredUsers.slice(startIndex, endIndex);

    // 创建文档片段提高性能
    const fragment = document.createDocumentFragment();

    pageUsers.forEach((user, index) => {
        const userItem = createAllUserItem(user, index);
        fragment.appendChild(userItem);
    });

    container.appendChild(fragment);
}

// 【新增】创建所有用户列表中的用户项
function createAllUserItem(user, index) {
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

    // 【关键】添加点击事件处理 - 与最近聊天列表相同的效果
    userItem.addEventListener('click', () => {
        selectUser(user);
    });

    return userItem;
}

// 【新增】更新所有用户列表分页信息
function updateAllUsersPagination() {
    const paginationInfo = document.getElementById('pagination-info');
    const prevButton = document.getElementById('prev-page');
    const nextButton = document.getElementById('next-page');

    if (!paginationInfo || !prevButton || !nextButton) return;

    const startIndex = allUsersState.currentPage * allUsersState.pageSize + 1;
    const endIndex = Math.min((allUsersState.currentPage + 1) * allUsersState.pageSize, allUsersState.filteredUsers.length);
    const totalPages = Math.ceil(allUsersState.filteredUsers.length / allUsersState.pageSize);

    paginationInfo.textContent = `显示 ${startIndex}-${endIndex} 共 ${allUsersState.filteredUsers.length} 个用户`;

    // 更新按钮状态
    prevButton.disabled = allUsersState.currentPage <= 0;
    nextButton.disabled = allUsersState.currentPage >= totalPages - 1;
}

// 【新增】上一页功能
function goToPreviousPage() {
    if (allUsersState.currentPage > 0) {
        allUsersState.currentPage--;
        renderAllUsersPage();
        updateAllUsersPagination();
    }
}

// 【新增】下一页功能
function goToNextPage() {
    const totalPages = Math.ceil(allUsersState.filteredUsers.length / allUsersState.pageSize);
    if (allUsersState.currentPage < totalPages - 1) {
        allUsersState.currentPage++;
        renderAllUsersPage();
        updateAllUsersPagination();
    }
}

// 【新增】搜索所有用户
function searchAllUsers(query) {
    allUsersState.searchQuery = query.toLowerCase();

    if (!query) {
        // 如果没有搜索词，显示所有用户
        allUsersState.filteredUsers = [...allUsersState.users];
    } else {
        // 根据用户名过滤
        allUsersState.filteredUsers = allUsersState.users.filter(user =>
            user.username.toLowerCase().includes(allUsersState.searchQuery)
        );
    }

    // 重置到第一页
    allUsersState.currentPage = 0;

    // 重新渲染
    renderAllUsersPage();
    updateAllUsersPagination();
}

// 【新增】更新所有用户列表中的未读状态
function updateAllUsersListUnreadStatus() {
    const container = document.getElementById('private-chat-user-list');
    if (!container) return;

    // 更新当前页所有用户项的未读状态
    const userItems = container.querySelectorAll('.user-item');
    userItems.forEach(userItem => {
        const username = userItem.dataset.username;
        if (username) {
            const user = allUsersState.filteredUsers.find(u => u.username === username);
            if (user) {
                const badge = userItem.querySelector('.user-badge');
                if (badge) {
                    const userHasUnread = hasUnread(user);
                    badge.style.display = userHasUnread ? 'flex' : 'none';
                }
            }
        }
    });
}

// 【修改】更新现有的setupEventListeners函数，添加所有用户相关的事件监听
function setupEventListeners() {
    // 原有的搜索框事件（最近聊天）
    const searchInput = document.getElementById('userSearchInput');
    const debouncedSearch = debounce((value) => {
        searchUsers(value);
    }, 300);

    searchInput.addEventListener('input', (e) => {
        debouncedSearch(e.target.value);
    });

    // 【新增】所有用户搜索框事件
    const allUserSearchInput = document.getElementById('allUserSearchInput');
    const debouncedAllUserSearch = debounce((value) => {
        searchAllUsers(value);
    }, 300);

    allUserSearchInput.addEventListener('input', (e) => {
        debouncedAllUserSearch(e.target.value);
    });

    // 【新增】分页按钮事件
    const prevButton = document.getElementById('prev-page');
    const nextButton = document.getElementById('next-page');

    if (prevButton) {
        prevButton.addEventListener('click', goToPreviousPage);
    }

    if (nextButton) {
        nextButton.addEventListener('click', goToNextPage);
    }

    // 原有的发送按钮事件
    const sendButton = document.getElementById('sendButton');
    sendButton.addEventListener('click', sendMessage);

    // 原有的输入框按Enter发送
    const messageInput = document.getElementById('messageInput');
    messageInput.addEventListener('keydown', (e) => {
        if (e.key === 'Enter' && !e.shiftKey) {
            e.preventDefault();
            sendMessage();
        }
    });

    // 原有的输入框自动调整高度
    messageInput.addEventListener('input', function() {
        this.style.height = 'auto';
        this.style.height = (this.scrollHeight) + 'px';
    });

    // 原有的页面可见性变化事件
    document.addEventListener('visibilitychange', () => {
        adjustPrivateFetchInterval();
    });

    window.addEventListener('focus', () => {
        adjustPrivateFetchInterval();
    });

    window.addEventListener('blur', () => {
        adjustPrivateFetchInterval();
    });
}

// 【修改】修改现有的initPage函数，添加所有用户列表初始化
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

    // 获取用户列表（最近聊天）
    await fetchUserList();

    // 【新增】获取所有用户列表（分页显示）
    await fetchAllUsers();

    // 初始加载所有与当前用户相关的消息
    await loadAllUserMessages();

    console.log('页面初始化完成');
    isFirstMessageLoad = false;

    // 首次渲染用户列表
    renderUserList(userList);
    updateAllUsersUnreadStatus();

    // 启动高性能轮询
    privateIsPollingActive = true;
    fetchPrivateMessages();
    startUserListUnreadCheck();
}

// 【修改】修改现有的updateAllUsersUnreadStatus函数，同时更新两个用户列表
function updateAllUsersUnreadStatus() {
    if (!userList || userList.length === 0) return;

    let hasChanges = false;

    // 更新最近聊天列表
    userList.forEach(user => {
        if (user.username !== currentUsername) {
            const currentUnreadStatus = hasUnread(user);
            const userItem = document.querySelector(`#userList [data-username="${user.username}"]`);

            if (userItem) {
                const badge = userItem.querySelector('.user-badge');
                if (badge) {
                    const isCurrentlyVisible = badge.style.display === 'flex';

                    if (currentUnreadStatus !== isCurrentlyVisible) {
                        badge.style.display = currentUnreadStatus ? 'flex' : 'none';
                        hasChanges = true;
                    }
                }
            }
        }
    });

    // 【新增】同时更新所有用户列表的未读状态
    updateAllUsersListUnreadStatus();

    if (hasChanges) {
        // console.log('检测到未读状态变化');
    }
}