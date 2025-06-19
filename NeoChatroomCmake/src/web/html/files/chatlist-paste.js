/**
 * 云剪贴板功能 - 聊天室列表页面集成
 * 提供最近剪贴板内容预览和跳转功能
 */

// 全局配置
const PASTE_CONFIG = {
    maxItems: 3,         // 最多显示的剪贴板数量
    refreshInterval: 30, // 自动刷新间隔（秒）
    maxTitleLength: 30,  // 标题最大长度
};

/**
 * 工具函数：格式化时间戳
 */
function formatTimestamp(timestamp) {
    const date = new Date(timestamp * 1000);
    const now = new Date();
    const diff = Math.floor((now - date) / 1000);
    
    // 如果是今天，显示"今天 HH:MM"
    if (date.getDate() === now.getDate() && 
        date.getMonth() === now.getMonth() && 
        date.getFullYear() === now.getFullYear()) {
        return `今天 ${date.getHours().toString().padStart(2, '0')}:${date.getMinutes().toString().padStart(2, '0')}`;
    }
    
    // 如果是昨天，显示"昨天 HH:MM"
    const yesterday = new Date(now);
    yesterday.setDate(yesterday.getDate() - 1);
    if (date.getDate() === yesterday.getDate() && 
        date.getMonth() === yesterday.getMonth() && 
        date.getFullYear() === yesterday.getFullYear()) {
        return `昨天 ${date.getHours().toString().padStart(2, '0')}:${date.getMinutes().toString().padStart(2, '0')}`;
    }
    
    // 如��是最近7天，显示"周几 HH:MM"
    if (diff < 7 * 24 * 60 * 60) {
        const weekdays = ['日', '一', '二', '三', '四', '五', '六'];
        return `周${weekdays[date.getDay()]} ${date.getHours().toString().padStart(2, '0')}:${date.getMinutes().toString().padStart(2, '0')}`;
    }
    
    // 其他情况显示完整日期
    return `${date.getFullYear()}-${(date.getMonth() + 1).toString().padStart(2, '0')}-${date.getDate().toString().padStart(2, '0')} ${date.getHours().toString().padStart(2, '0')}:${date.getMinutes().toString().padStart(2, '0')}`;
}

/**
 * 工具函数：截断文本并添加省略号
 */
function truncateText(text, maxLength) {
    if (text.length <= maxLength) return text;
    return text.substring(0, maxLength) + "...";
}

/**
 * 工具函数：安全文本，防止XSS
 */
function safeText(text) {
    const div = document.createElement('div');
    div.textContent = text;
    return div.innerHTML;
}

/**
 * 创建过渡动画
 */
function createPasteTransition() {
    const overlay = document.createElement('div');
    overlay.className = 'transition-overlay';
    overlay.style.backgroundColor = 'rgba(0, 123, 255, 0.2)';
    document.body.appendChild(overlay);
    
    setTimeout(() => {
        overlay.style.opacity = '1';
    }, 10);
    
    return overlay;
}

/**
 * 加载用户剪贴板
 */
async function loadUserPastes() {
    const pasteListElement = document.getElementById('cloud-paste-list');
    if (!pasteListElement) return;
    
    // 检查用户是否登录
    if (!currentUsername) {
        pasteListElement.innerHTML = `
            <div class="cloud-paste-empty">
                请先登录以查看您的剪贴板内容
            </div>
        `;
        return;
    }
    
    try {
        // 显示加载状态
        pasteListElement.innerHTML = `
            <div class="paste-loading">
                <div class="paste-loading-spinner"></div>
                <div>加载中...</div>
            </div>
        `;
        
        // 获取用户的剪贴板列表
        const encodedUsername = encodeURIComponent(currentUsername);
        const response = await fetch(`${serverUrl}/paste/query?username=${encodedUsername}&limit=${PASTE_CONFIG.maxItems}`, {
            method: 'GET',
            credentials: 'include'
        });
        
        if (!response.ok) {
            throw new Error('获取剪贴板失败');
        }
        
        const result = await response.json();
        
        if (!result.success) {
            throw new Error(result.message || '获取剪贴板失败');
        }
        
        // 渲染剪贴板列表
        renderPasteList(result.data);
        
    } catch (error) {
        console.error('加载云剪贴板失败:', error);
        pasteListElement.innerHTML = `
            <div class="cloud-paste-empty">
                加载剪贴板数据时出错
            </div>
        `;
    }
}

/**
 * 渲染剪贴板列表
 */
function renderPasteList(pastes) {
    const pasteListElement = document.getElementById('cloud-paste-list');
    if (!pasteListElement) return;
    
    // 如果没有剪贴板内容
    if (!pastes || pastes.length === 0) {
        pasteListElement.innerHTML = `
            <div class="cloud-paste-empty">
                您还没有创建任何剪贴板
            </div>
        `;
        return;
    }
    
    // 清空容器
    pasteListElement.innerHTML = '';
    
    // 创建并添加剪贴板项目
    pastes.forEach((paste, index) => {
        const pasteItem = document.createElement('div');
        pasteItem.className = 'paste-item';
        pasteItem.style.animationDelay = `${index * 0.1}s`;
        
        // 安全处理文本
        const safeTitle = safeText(paste.title);
        const safeLanguage = safeText(paste.language || '纯文本');
        
        // 格式化时间
        let timeDisplay = paste.formattedTime;
        if (paste.timestamp) {
            timeDisplay = formatTimestamp(paste.timestamp);
        }
        
        pasteItem.innerHTML = `
            <div class="paste-item-title">${truncateText(safeTitle, PASTE_CONFIG.maxTitleLength)}</div>
            <div class="paste-item-meta">
                <span class="paste-item-time">${timeDisplay}</span>
                <span class="paste-item-language">${safeLanguage}</span>
            </div>
        `;
        
        // 添加点击事件
        pasteItem.addEventListener('click', () => {
            viewPaste(paste.id);
        });
        
        pasteListElement.appendChild(pasteItem);
    });
}

/**
 * 查看剪贴板详情
 */
function viewPaste(pasteId) {
    // 创建过渡效果
    const overlay = createPasteTransition();
    
    // 跳转到剪贴板详情页
    setTimeout(() => {
        window.location.href = `/paste/view?id=${pasteId}`;
    }, 300);
}

/**
 * 初始化云剪贴板功能
 */
function initCloudPaste() {
    // 加载用户剪贴板
    loadUserPastes();
    
    // 设置定时刷新
    setInterval(loadUserPastes, PASTE_CONFIG.refreshInterval * 1000);
    
    // 进入剪贴板按钮点击事件
    const enterButton = document.getElementById('cloud-paste-button');
    if (enterButton) {
        enterButton.addEventListener('click', () => {
            // 创建过渡效果
            const overlay = createPasteTransition();
            
            // 跳转到剪贴板页面
            setTimeout(() => {
                window.location.href = '/paste';
            }, 300);
        });
    }
}

// 页面加载完成后初始化云剪贴板功能
document.addEventListener('DOMContentLoaded', () => {
    // 等待主脚本完成登录状态检查
    setTimeout(initCloudPaste, 500);
});