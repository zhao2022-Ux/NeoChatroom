// 全局变量
let currentUsername = "";
const serverUrl = window.location.origin;

// 工具函数：获取 cookie
function getCookie(name) {
    const value = "; " + document.cookie;
    const parts = value.split("; " + name + "=");
    if (parts.length === 2) return parts.pop().split(";").shift();
    return null;
}

// 工具函数：创建过渡效果
function createTransitionOverlay(color = 'rgba(0, 123, 255, 0.2)') {
    const transitionOverlay = document.createElement('div');
    transitionOverlay.className = 'transition-overlay';
    transitionOverlay.style.position = 'fixed';
    transitionOverlay.style.top = '0';
    transitionOverlay.style.left = '0';
    transitionOverlay.style.width = '100%';
    transitionOverlay.style.height = '100%';
    transitionOverlay.style.backgroundColor = color;
    transitionOverlay.style.opacity = '0';
    transitionOverlay.style.transition = 'opacity 0.3s ease';
    transitionOverlay.style.zIndex = '1000';
    document.body.appendChild(transitionOverlay);
    
    // 淡入效果
    setTimeout(() => {
        transitionOverlay.style.opacity = '1';
    }, 10);
    
    return transitionOverlay;
}

// 更新登录状态
async function updateLoginStatus() {
    const uid = getCookie("uid");
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
        console.error('检查登录状态错误:', error);
        document.getElementById('loginButton').style.display = 'inline-block';
        currentUsername = '';
        document.getElementById('usernameDisplay').textContent = '';
        return false;
    }
}

// 检查登录状态
async function checkLoginStatus() {
    const uid = getCookie("uid");
    try {
        const response = await fetch(`${serverUrl}/user/username?uid=${uid}`);
        
        // 未授权时重定向到登录页
        if (response.status === 401 || response.status === 403) {
            window.location.href = '/login';
            return false;
        }
        
        // 其他错误返回false但不重定向
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

// 标签页切换
function setupTabs() {
    const tabs = document.querySelectorAll('.tab');
    tabs.forEach(tab => {
        tab.addEventListener('click', () => {
            // 移除所有活动标签
            tabs.forEach(t => t.classList.remove('active'));
            // 添加当前活动标签
            tab.classList.add('active');
            // 隐藏所有内容区域
            const contentAreas = document.querySelectorAll('.content-area');
            contentAreas.forEach(area => {
                area.style.display = 'none';
            });
            // 显示当前内容区域
            const tabId = tab.getAttribute('data-tab');
            document.getElementById(`${tabId}-tab`).style.display = 'block';
            
            // 如果切换到"我的剪贴板"，加载用户剪贴板
            if (tabId === 'my-pastes') {
                loadUserPastes();
            }
            // 如果切换到"公开剪贴板"，加载公开剪贴板
            else if (tabId === 'public-pastes') {
                loadPublicPastes();
            }
        });
    });
}

// 私有剪贴板设置显示/隐藏密码字段
function setupPrivateToggle() {
    const privateToggle = document.getElementById('paste-private');
    const passwordGroup = document.getElementById('password-group');
    
    privateToggle.addEventListener('change', () => {
        passwordGroup.style.display = privateToggle.checked ? 'block' : 'none';
    });
    
    const editPrivateToggle = document.getElementById('edit-paste-private');
    const editPasswordGroup = document.getElementById('edit-password-group');
    
    editPrivateToggle.addEventListener('change', () => {
        editPasswordGroup.style.display = editPrivateToggle.checked ? 'block' : 'none';
    });
}

// 创建剪贴板
async function createPaste(event) {
    event.preventDefault();
    
    if (!await checkLoginStatus()) return;
    
    const title = document.getElementById('paste-title').value.trim();
    const content = document.getElementById('paste-content').value.trim();
    const language = document.getElementById('paste-language').value;
    const expiryDays = parseInt(document.getElementById('paste-expiry').value);
    const isPrivate = document.getElementById('paste-private').checked;
    const password = document.getElementById('paste-password').value;
    
    if (!title || !content) {
        alert('标题和内容不能为空');
        return;
    }
    
    try {
        // 显示加载指示器
        const loadingOverlay = createTransitionOverlay('rgba(0, 123, 255, 0.1)');
        
        const response = await fetch(`${serverUrl}/paste/update`, {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify({
                title: title,
                content: content,
                language: language,
                isPrivate: isPrivate,
                password: password,
                expiryDays: expiryDays
            }),
            credentials: 'include'
        });
        
        // 移除加载指示器
        document.body.removeChild(loadingOverlay);
        
        if (!response.ok) {
            const errorData = await response.json();
            throw new Error(errorData.message || '创建剪贴板失败');
        }
        
        const result = await response.json();
        
        if (result.success) {
            alert('剪贴板创建成功！');
            document.getElementById('create-paste-form').reset();
            document.getElementById('password-group').style.display = 'none';
            
            // 切换到"我的剪贴板"标签
            document.querySelector('.tab[data-tab="my-pastes"]').click();
        } else {
            throw new Error(result.message || '创建剪贴板失败');
        }
    } catch (error) {
        console.error('创建剪贴板时发生错误:', error);
        alert('创建剪贴板时发生错误: ' + error.message);
    }
}

// 加载用户的剪贴板列表
async function loadUserPastes() {
    if (!await checkLoginStatus() || !currentUsername) return;
    
    const container = document.getElementById('my-pastes-list');
    container.innerHTML = `
        <div class="loading">
            <div class="loading-spinner"></div>
            <div>加载中...</div>
        </div>
    `;
    
    try {
        // 确保使用encodeURIComponent处理用户名，防止特殊字符问题
        const encodedUsername = encodeURIComponent(currentUsername);
        const response = await fetch(`${serverUrl}/paste/query?username=${encodedUsername}`, {
            method: 'GET',
            credentials: 'include'
        });
        
        if (!response.ok) {
            const errorData = await response.json();
            throw new Error(errorData.message || '加载剪贴板失败');
        }
        
        const result = await response.json();
        
        if (result.success) {
            renderPasteList(container, result.data, true);
        } else {
            throw new Error(result.message || '加载剪贴板失败');
        }
    } catch (error) {
        console.error('加载用户剪贴板时发生错误:', error);
        container.innerHTML = `
            <div class="empty-state">
                <div class="empty-state-icon">📋</div>
                <div class="empty-state-message">加载剪贴板时发生错误</div>
                <div>${error.message}</div>
            </div>
        `;
    }
}

// 加载公开的剪贴板列表
async function loadPublicPastes() {
    const container = document.getElementById('public-pastes-list');
    container.innerHTML = `
        <div class="loading">
            <div class="loading-spinner"></div>
            <div>加载中...</div>
        </div>
    `;
    
    try {
        const response = await fetch(`${serverUrl}/paste/query`, {
            method: 'GET',
            credentials: 'include'
        });
        
        if (!response.ok) {
            const errorData = await response.json();
            throw new Error(errorData.message || '加载剪贴板失败');
        }
        
        const result = await response.json();
        
        if (result.success) {
            renderPasteList(container, result.data, false);
        } else {
            throw new Error(result.message || '加载剪贴板失败');
        }
    } catch (error) {
        console.error('加载公开剪贴板时发生错误:', error);
        container.innerHTML = `
            <div class="empty-state">
                <div class="empty-state-icon">📋</div>
                <div class="empty-state-message">加载剪贴板时发生错误</div>
                <div>${error.message}</div>
            </div>
        `;
    }
}

// 渲染剪贴板列表
function renderPasteList(container, pastes, isUserList) {
    if (!pastes || pastes.length === 0) {
        container.innerHTML = `
            <div class="empty-state">
                <div class="empty-state-icon">📋</div>
                <div class="empty-state-message">${isUserList ? '您还没有创建任何剪贴板' : '没有可用的公开剪贴板'}</div>
                ${isUserList ? '<div>点击"创建剪贴板"标签来创建您的第一个剪贴板</div>' : ''}
            </div>
        `;
        return;
    }
    
    container.innerHTML = '';
    
    pastes.forEach(paste => {
        const card = document.createElement('div');
        card.className = 'paste-card';
        
        // 安全处理内容，防止XSS攻击
        const safeTitle = document.createTextNode(paste.title).textContent;
        const safeAuthor = document.createTextNode(paste.author).textContent;
        const safeLanguage = document.createTextNode(paste.language || '纯文本').textContent;
        
        // 确保内容预览不超过100个字符，并处理可能的HTML内容
        let contentPreview = '';
        if (paste.content) {
            contentPreview = paste.content.length > 100 
                ? paste.content.substring(0, 100) + '...' 
                : paste.content;
        } else {
            contentPreview = '暂无内容预览';
        }
        
        // 安全处理内容预览
        const safeContentPreview = document.createTextNode(contentPreview).textContent;
        
        card.innerHTML = `
            <div class="paste-card-title">${safeTitle}</div>
            <div class="paste-card-meta">
                <span>作者: ${safeAuthor}</span>
                <span>${paste.formattedTime || '未知时间'}</span>
            </div>
            <div class="paste-card-meta">
                <span>语言: ${safeLanguage}</span>
                <span>${paste.isPrivate ? '私有' : '公开'}</span>
                <span>${paste.expiryDays ? `${paste.expiryDays}天后过期` : '永不过期'}</span>
            </div>
            <div class="paste-card-preview">${safeContentPreview}</div>
            <div class="paste-card-actions">
                <button class="view-btn" data-id="${paste.id}">查看</button>
                ${isUserList ? `
                    <button class="edit-btn" data-id="${paste.id}">编辑</button>
                    <button class="delete-btn" data-id="${paste.id}">删除</button>
                ` : ''}
            </div>
        `;
        
        container.appendChild(card);
    });
    
    // 添加查看按钮事件处理
    const viewButtons = container.querySelectorAll('.view-btn');
    viewButtons.forEach(btn => {
        btn.addEventListener('click', () => {
            const pasteId = btn.getAttribute('data-id');
            viewPaste(pasteId);
        });
    });
    
    // 添加编辑按钮事件处理（仅用户列表）
    if (isUserList) {
        const editButtons = container.querySelectorAll('.edit-btn');
        editButtons.forEach(btn => {
            btn.addEventListener('click', () => {
                const pasteId = btn.getAttribute('data-id');
                editPaste(pasteId);
            });
        });
        
        // 添加删除按钮事件处理
        const deleteButtons = container.querySelectorAll('.delete-btn');
        deleteButtons.forEach(btn => {
            btn.addEventListener('click', () => {
                const pasteId = btn.getAttribute('data-id');
                deletePaste(pasteId);
            });
        });
    }
}

// 查看剪贴板详情
async function viewPaste(pasteId) {
    try {
        // 显示加载指示器
        const loadingOverlay = createTransitionOverlay('rgba(0, 123, 255, 0.1)');
        
        // 直接跳转到详情页面，让详情页面自己处理加载
        setTimeout(() => {
            window.location.href = `/paste/view?id=${pasteId}`;
            document.body.removeChild(loadingOverlay);
        }, 300);
    } catch (error) {
        console.error('查看剪贴板时发生错误:', error);
        alert('查看剪贴板时发生错误: ' + error.message);
    }
}

// 使用密码查看私有剪贴板
async function viewPasteWithPassword(pasteId, password) {
    try {
        // 显示加载指示器
        const loadingOverlay = createTransitionOverlay('rgba(0, 123, 255, 0.1)');
        
        // 使用encodeURIComponent确保密码中的特殊字符正确编码
        const encodedPassword = encodeURIComponent(password);
        const response = await fetch(`${serverUrl}/paste/query?id=${pasteId}&password=${encodedPassword}`, {
            method: 'GET',
            credentials: 'include'
        });
        
        // 移除加载指示器
        document.body.removeChild(loadingOverlay);
        
        if (!response.ok) {
            const errorData = await response.json();
            throw new Error(errorData.message || '密码错误或加载剪贴板失败');
        }
        
        const result = await response.json();
        
        if (result.success) {
            showPasteDetailModal(result.data);
        } else {
            throw new Error(result.message || '密码错误或加载剪贴板失败');
        }
    } catch (error) {
        console.error('使用密码查看剪贴板时发生错误:', error);
        alert('使用密码查看剪贴板时发生错误: ' + error.message);
    }
}

// 显示剪贴板详情模态框
function showPasteDetailModal(paste) {
    // 安全处理内容，防止XSS攻击
    const safeTitle = document.createTextNode(paste.title).textContent;
    const safeAuthor = document.createTextNode(paste.author).textContent;
    const safeLanguage = document.createTextNode(paste.language || '纯文本').textContent;
    
    // 设置模态框内容
    document.getElementById('modal-paste-title').textContent = safeTitle;
    document.getElementById('modal-paste-author').textContent = `作者: ${safeAuthor}`;
    document.getElementById('modal-paste-time').textContent = `创建时间: ${paste.formattedTime || '未知时间'}`;
    document.getElementById('modal-paste-language').textContent = `语言: ${safeLanguage}`;
    document.getElementById('modal-paste-expiry').textContent = paste.expiryDays ? 
        `过期时间: ${paste.expiryDays}天后` : '过期时间: 永不过期';
    
    const contentElement = document.getElementById('modal-paste-content');
    contentElement.textContent = paste.content || '';
    contentElement.className = paste.language ? `language-${paste.language}` : '';
    
    // 应用代码高亮
    if (paste.language && hljs) {
        try {
            hljs.highlightElement(contentElement);
        } catch (e) {
            console.error('代码高亮出错:', e);
        }
    }
    
    // 控制编辑和删除按钮的显示
    const deleteBtn = document.getElementById('modal-delete-btn');
    const editBtn = document.getElementById('modal-edit-btn');
    
    if (paste.author === currentUsername) {
        deleteBtn.style.display = 'inline-block';
        editBtn.style.display = 'inline-block';
        
        // 添加编辑和删除功能
        deleteBtn.onclick = () => {
            closeDetailModal();
            deletePaste(paste.id);
        };
        
        editBtn.onclick = () => {
            closeDetailModal();
            editPaste(paste.id);
        };
    } else {
        deleteBtn.style.display = 'none';
        editBtn.style.display = 'none';
    }
    
    // 显示模态框
    document.getElementById('paste-detail-modal').classList.add('active');
    
    // 添加关闭事件
    document.getElementById('modal-close').onclick = closeDetailModal;
    document.getElementById('modal-close-btn').onclick = closeDetailModal;
}

// 关闭详情模态框
function closeDetailModal() {
    document.getElementById('paste-detail-modal').classList.remove('active');
}

// 删除剪贴板
async function deletePaste(pasteId) {
    if (!await checkLoginStatus()) return;
    
    if (!confirm('确定要删除此剪贴板吗？此操作不可撤销。')) {
        return;
    }
    
    try {
        // 显示加载指示器
        const loadingOverlay = createTransitionOverlay('rgba(220, 53, 69, 0.1)');
        
        const response = await fetch(`${serverUrl}/paste/delete`, {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify({
                id: parseInt(pasteId)  // 确保ID是数字类型
            }),
            credentials: 'include'
        });
        
        // 移除加载指示器
        document.body.removeChild(loadingOverlay);
        
        if (!response.ok) {
            const errorData = await response.json();
            throw new Error(errorData.message || '删除剪贴板失败');
        }
        
        const result = await response.json();
        
        if (result.success) {
            alert('剪贴板删除成功！');
            // 重新加载用户剪贴板列表
            loadUserPastes();
        } else {
            throw new Error(result.message || '删除剪贴板失败');
        }
    } catch (error) {
        console.error('删除剪贴板时发生错误:', error);
        alert('删除剪贴板时发生错误: ' + error.message);
    }
}

// 编辑剪贴板
async function editPaste(pasteId) {
    if (!await checkLoginStatus()) return;
    
    try {
        // 显示加载指示器
        const loadingOverlay = createTransitionOverlay('rgba(0, 123, 255, 0.1)');
        
        const response = await fetch(`${serverUrl}/paste/query?id=${pasteId}`, {
            method: 'GET',
            credentials: 'include'
        });
        
        // 移除加载指示器
        document.body.removeChild(loadingOverlay);
        
        if (!response.ok) {
            const errorData = await response.json();
            throw new Error(errorData.message || '加载剪贴板失败');
        }
        
        const result = await response.json();
        
        if (result.success) {
            const paste = result.data;
            
            // 设置编辑表单的值
            document.getElementById('edit-paste-id').value = paste.id;
            document.getElementById('edit-paste-title').value = paste.title || '';
            document.getElementById('edit-paste-content').value = paste.content || '';
            document.getElementById('edit-paste-language').value = paste.language || '';
            document.getElementById('edit-paste-expiry').value = paste.expiryDays || '0';
            document.getElementById('edit-paste-private').checked = paste.isPrivate;
            // 不设置密码，让用户重新输入
            document.getElementById('edit-paste-password').value = '';
            
            // 显示/隐藏密码字段
            document.getElementById('edit-password-group').style.display = paste.isPrivate ? 'block' : 'none';
            
            // 显示编辑模态框
            document.getElementById('paste-edit-modal').classList.add('active');
            
            // 添加关闭和保���事件
            document.getElementById('edit-modal-close').onclick = closeEditModal;
            document.getElementById('edit-modal-cancel').onclick = closeEditModal;
            document.getElementById('edit-modal-save').onclick = savePasteEdit;
        } else {
            throw new Error(result.message || '加载剪贴板失败');
        }
    } catch (error) {
        console.error('编辑剪贴板时发生错误:', error);
        alert('编辑剪贴板时发生错误: ' + error.message);
    }
}

// 关闭编辑模态框
function closeEditModal() {
    document.getElementById('paste-edit-modal').classList.remove('active');
}

// 保存剪贴板编辑
async function savePasteEdit() {
    if (!await checkLoginStatus()) return;
    
    const pasteId = document.getElementById('edit-paste-id').value;
    const title = document.getElementById('edit-paste-title').value.trim();
    const content = document.getElementById('edit-paste-content').value.trim();
    const language = document.getElementById('edit-paste-language').value;
    const expiryDays = parseInt(document.getElementById('edit-paste-expiry').value);
    const isPrivate = document.getElementById('edit-paste-private').checked;
    const password = document.getElementById('edit-paste-password').value;
    
    if (!title || !content) {
        alert('标题和内容不能为空');
        return;
    }
    
    try {
        // 显示加载指示器
        const loadingOverlay = createTransitionOverlay('rgba(0, 123, 255, 0.1)');
        
        const response = await fetch(`${serverUrl}/paste/update`, {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify({
                id: parseInt(pasteId),  // 确保ID是数字类型
                title: title,
                content: content,
                language: language,
                isPrivate: isPrivate,
                password: password,
                expiryDays: expiryDays
            }),
            credentials: 'include'
        });
        
        // 移除加载指示器
        document.body.removeChild(loadingOverlay);
        
        if (!response.ok) {
            const errorData = await response.json();
            throw new Error(errorData.message || '更新剪贴板失败');
        }
        
        const result = await response.json();
        
        if (result.success) {
            alert('剪贴板更新成功！');
            closeEditModal();
            // 重新加载用户剪贴板列表
            loadUserPastes();
        } else {
            throw new Error(result.message || '更新剪贴板失败');
        }
    } catch (error) {
        console.error('更新剪贴板时发生错误:', error);
        alert('更新剪贴板时发生错误: ' + error.message);
    }
}

// 设置暗黑模式切换
function setupDarkMode() {
    const themeToggle = document.getElementById('themeToggle');
    themeToggle.addEventListener('click', () => {
        document.body.classList.toggle('dark-mode');
        // 保存主题偏好
        const isDarkMode = document.body.classList.contains('dark-mode');
        localStorage.setItem('darkMode', isDarkMode);
        
        // 更新代码高亮样式
        if (hljs) {
            const codeElements = document.querySelectorAll('pre code');
            codeElements.forEach(code => {
                try {
                    hljs.highlightElement(code);
                } catch (e) {
                    console.error('代码高亮更新出错:', e);
                }
            });
        }
    });

    // 检查保存的主题偏好
    if (localStorage.getItem('darkMode') === 'true') {
        document.body.classList.add('dark-mode');
    }
}

// 页面加载后初始化
document.addEventListener('DOMContentLoaded', async () => {
    // 检查登录状态
    await updateLoginStatus();
    
    if (!currentUsername) {
        // 不立即重定向，给用户一个提示
        setTimeout(() => {
            if (confirm('需要登录后才能使用云剪贴板功能。是否前往登录页面？')) {
                window.location.href = '/login';
            }
        }, 500);
    }
    
    // 设置标签页切换
    setupTabs();
    
    // 设置私有剪贴板切换
    setupPrivateToggle();
    
    // 设置表单提交事件
    document.getElementById('create-paste-form').addEventListener('submit', createPaste);
    
    // 设置暗黑模式
    setupDarkMode();
    
    // 如果用户已登录，加载初始内容
    if (currentUsername) {
        loadUserPastes();
    }
    
    // 页面加载动画
    document.body.style.opacity = '0';
    document.body.style.transition = 'opacity 0.5s ease';
    setTimeout(() => {
        document.body.style.opacity = '1';
    }, 50);
});