/**
 * chatroom-paste.js - 聊天室剪贴板集成模块
 * 提供检测大块文本粘贴、创建云剪贴板及渲染剪贴板内容的功能
 */

const ChatroomPaste = (function() {
    // 配置项
    const config = {
        // 判断为大段文本的最小字符数
        minPasteLength: 200,
        // 判断为大段文本的最小行数
        minPasteLines: 5,
        // 服务器URL
        serverUrl: window.location.origin,
        // 超时时间（毫秒）
        timeout: 5000
    };

    // 缓存已渲染的剪贴板内容
    const renderedPastes = new Map();
    
    // 确保highlight.js已加载
    let hljs = null;
    
    /**
     * 加载highlight.js库
     */
    function loadHighlightJs() {
        // 如果页面已经有hljs，直接使用
        if (window.hljs) {
            hljs = window.hljs;
            return Promise.resolve();
        }
        
        // 动态加载highlight.js和CSS
        return new Promise((resolve, reject) => {
            // 加载CSS
            const cssLink = document.createElement('link');
            cssLink.rel = 'stylesheet';
            cssLink.href = '/files/github.min.css';
            document.head.appendChild(cssLink);
            
            // 加载JS
            const script = document.createElement('script');
            script.src = '/files/highlight.min.js';
            script.onload = () => {
                hljs = window.hljs;
                resolve();
            };
            script.onerror = reject;
            document.head.appendChild(script);
        });
    }

    /**
     * 检查文本是否应该作为代码块处理
     * @param {string} text 粘贴的文本
     * @returns {boolean} 是否为大段文本
     */
    function isLargeTextBlock(text) {
        if (!text) return false;

        // 检查字符数
        if (text.length >= config.minPasteLength) {
            return true;
        }

        // 检查行数
        const lines = text.split('\n');
        if (lines.length >= config.minPasteLines) {
            return true;
        }

        // 检查是否包含代码特征（如括号、缩进等）
        const codePatterns = [
            /\{[\s\S]*\}/,         // 花括号块
            /\([\s\S]*\)/,         // 圆括号块
            /^\s*function\s/m,     // 函数声明
            /^\s*class\s/m,        // 类声明
            /^\s*import\s/m,       // import语句
            /^\s*export\s/m,       // export语句
            /^\s*const\s|^\s*let\s|^\s*var\s/m, // 变量声明
            /^\s*if\s*\(/m,        // if语句
            /^\s*for\s*\(/m,       // for循环
            /^\s*while\s*\(/m,     // while循环
            /^\s*switch\s*\(/m,    // switch语句
            /^\s*return\s/m,       // return语句
            /^\s*<[a-zA-Z][^>]*>/m // HTML标签
        ];

        // 检查是否有连续的缩进行
        let indentedLines = 0;
        for (const line of lines) {
            if (/^\s{2,}/.test(line)) {
                indentedLines++;
                if (indentedLines >= 3) return true;
            } else {
                indentedLines = 0;
            }
        }

        // 检查代码特征
        return codePatterns.some(pattern => pattern.test(text));
    }

    /**
     * 检测文本的编程语言
     * @param {string} text 文本内容
     * @returns {string} 检测到的语言或空字符串
     */
    function detectLanguage(text) {
        if (!text) return '';

        // 预处理：移除多余空白，但保留结构
        const normalizedText = text.trim();

        // 优化的语言检测规则，按权重排序
        const languagePatterns = [
            {
                lang: 'json',
                weight: 3,
                patterns: [
                    { regex: /^\s*[\{\[][\s\S]*[\}\]]\s*$/, weight: 3 }, // JSON结构
                    { regex: /"[\w\s-]+"\s*:\s*["{\[\d]/, weight: 2 }, // JSON键值对
                    { regex: /:\s*[\[\{]/, weight: 1 }
                ]
            },
            {
                lang: 'html',
                weight: 3,
                patterns: [
                    { regex: /<!DOCTYPE\s+html>/i, weight: 3 },
                    { regex: /<html[\s>]/, weight: 3 },
                    { regex: /<\/?(div|span|p|h[1-6]|body|head|meta|link)\b[^>]*>/i, weight: 2 },
                    { regex: /<[a-zA-Z][^>]*>[\s\S]*<\/[a-zA-Z][^>]*>/, weight: 1 }
                ]
            },
            {
                lang: 'xml',
                weight: 2,
                patterns: [
                    { regex: /<\?xml\s+version\s*=/, weight: 3 },
                    { regex: /<\w+\s+xmlns\s*=/, weight: 2 },
                    { regex: /<\/\w+>\s*$/, weight: 1 }
                ]
            },
            {
                lang: 'css',
                weight: 2,
                patterns: [
                    { regex: /[.#][\w-]+\s*\{[^}]*\}/, weight: 3 }, // CSS选择器
                    { regex: /@(media|import|keyframes|font-face)/, weight: 2 },
                    { regex: /:\s*(#[\da-fA-F]{3,6}|rgba?\(|hsla?\(|\d+px|\d+em|\d+%)/, weight: 2 }, // CSS属性值
                    { regex: /(margin|padding|border|background|color|font-size)\s*:/, weight: 1 }
                ]
            },
            {
                lang: 'sql',
                weight: 2,
                patterns: [
                    { regex: /\b(SELECT|INSERT|UPDATE|DELETE|CREATE|ALTER|DROP)\b.*\b(FROM|INTO|SET|WHERE|TABLE)\b/i, weight: 3 },
                    { regex: /\b(JOIN|INNER|LEFT|RIGHT|OUTER)\s+JOIN\b/i, weight: 2 },
                    { regex: /\b(GROUP\s+BY|ORDER\s+BY|HAVING)\b/i, weight: 2 }
                ]
            },
            {
                lang: 'typescript',
                weight: 2,
                patterns: [
                    { regex: /:\s*(string|number|boolean|any|void|never|unknown)\b/, weight: 3 }, // TS类型注解
                    { regex: /\binterface\s+\w+/, weight: 3 },
                    { regex: /\btype\s+\w+\s*=/, weight: 2 },
                    { regex: /\bas\s+\w+/, weight: 2 }, // 类型断言
                    { regex: /<[A-Z]\w*>/, weight: 1 } // 泛型
                ]
            },
            {
                lang: 'javascript',
                weight: 2,
                patterns: [
                    { regex: /\b(const|let|var)\s+\w+\s*=/, weight: 2 },
                    { regex: /function\s*\w*\s*\([^)]*\)\s*\{/, weight: 2 },
                    { regex: /=>\s*[\{\(]/, weight: 2 }, // 箭头函数
                    { regex: /\b(console\.(log|error|warn)|document\.|window\.)/, weight: 2 },
                    { regex: /\.(forEach|map|filter|reduce|find)\s*\(/, weight: 1 }, // 数组方法
                    { regex: /\b(import|export|require)\b/, weight: 1 }
                ]
            },
            {
                lang: 'python',
                weight: 2,
                patterns: [
                    { regex: /^def\s+\w+\s*\([^)]*\)\s*:/, weight: 3 },
                    { regex: /^class\s+\w+(\([^)]*\))?\s*:/, weight: 3 },
                    { regex: /^import\s+\w+|^from\s+\w+\s+import/, weight: 2 },
                    { regex: /if\s+__name__\s*==\s*['""]__main__['""]\s*:/, weight: 3 },
                    { regex: /\b(print|len|range|enumerate|zip)\s*\(/, weight: 1 }
                ]
            },
            {
                lang: 'java',
                weight: 2,
                patterns: [
                    { regex: /public\s+class\s+\w+/, weight: 3 },
                    { regex: /public\s+static\s+void\s+main\s*\(\s*String\[\]\s*\w+\s*\)/, weight: 3 },
                    { regex: /System\.(out|err)\.(println|print)/, weight: 2 },
                    { regex: /(private|protected|public)\s+(static\s+)?(final\s+)?\w+\s+\w+/, weight: 2 }
                ]
            },
            {
                lang: 'csharp',
                weight: 2,
                patterns: [
                    { regex: /namespace\s+[\w.]+/, weight: 3 },
                    { regex: /using\s+System;/, weight: 2 },
                    { regex: /Console\.(WriteLine|Write)/, weight: 2 },
                    { regex: /(public|private|protected)\s+(static\s+)?(class|interface)\s+\w+/, weight: 2 }
                ]
            },
            {
                lang: 'cpp',
                weight: 2,
                patterns: [
                    { regex: /#include\s*<\w+>/, weight: 2 },
                    { regex: /using\s+namespace\s+std;/, weight: 3 },
                    { regex: /std::(cout|cin|endl|vector|string)/, weight: 2 },
                    { regex: /int\s+main\s*\(\s*(void|int\s+argc,\s*char\s*\*\s*argv\[\])\s*\)/, weight: 2 }
                ]
            },
            {
                lang: 'c',
                weight: 2,
                patterns: [
                    { regex: /#include\s*<[\w.]+\.h>/, weight: 2 },
                    { regex: /int\s+main\s*\(\s*(void|int\s+argc,\s*char\s*\*\s*argv\[\])\s*\)/, weight: 2 },
                    { regex: /printf\s*\(/, weight: 2 },
                    { regex: /scanf\s*\(/, weight: 1 }
                ]
            },
            {
                lang: 'php',
                weight: 2,
                patterns: [
                    { regex: /<\?php/, weight: 3 },
                    { regex: /\$\w+\s*=/, weight: 2 },
                    { regex: /echo\s+/, weight: 1 },
                    { regex: /function\s+\w+\s*\(/, weight: 1 }
                ]
            },
            {
                lang: 'ruby',
                weight: 2,
                patterns: [
                    { regex: /^def\s+\w+/, weight: 2 },
                    { regex: /puts\s+/, weight: 1 },
                    { regex: /require\s+['""]\w+['""]/m, weight: 2 },
                    { regex: /^end$/m, weight: 1 }
                ]
            },
            {
                lang: 'markdown',
                weight: 1,
                patterns: [
                    { regex: /^#{1,6}\s+.+$/m, weight: 2 }, // 标题
                    { regex: /^\*{1,2}.+\*{1,2}$/m, weight: 1 }, // 粗体/斜体
                    { regex: /^[-*+]\s+.+$/m, weight: 1 }, // 列表
                    { regex: /^\d+\.\s+.+$/m, weight: 1 }, // 有序列表
                    { regex: /\[.+\]\(.+\)/, weight: 1 } // 链接
                ]
            },
            {
                lang: 'bash',
                weight: 1,
                patterns: [
                    { regex: /^#!/, weight: 3 }, // shebang
                    { regex: /export\s+\w+=/, weight: 2 },
                    { regex: /if\s+\[\s+.+\s+\];\s*then/, weight: 2 },
                    { regex: /echo\s+/, weight: 1 }
                ]
            }
        ];

        // 计算每种语言的得分
        const scores = {};

        languagePatterns.forEach(({ lang, weight: langWeight, patterns }) => {
            let totalScore = 0;

            patterns.forEach(({ regex, weight: patternWeight }) => {
                if (regex.test(normalizedText)) {
                    totalScore += patternWeight * langWeight;
                }
            });

            if (totalScore > 0) {
                scores[lang] = totalScore;
            }
        });

        // 找出得分最高的语言
        let maxScore = 0;
        let detectedLang = '';

        for (const [lang, score] of Object.entries(scores)) {
            if (score > maxScore) {
                maxScore = score;
                detectedLang = lang;
            }
        }

        // 设置最低阈值，避免误判
        const minimumScore = 3;
        if (maxScore < minimumScore) {
            return '';
        }

        // 特殊处理：如果检测���多种语言且分数接近，进行二次检查
        const topLanguages = Object.entries(scores)
            .filter(([, score]) => score >= maxScore * 0.8)
            .sort((a, b) => b[1] - a[1]);

        if (topLanguages.length > 1) {
            // 使用更严格的规则进行二次检测
            const secondaryCheck = performSecondaryLanguageCheck(normalizedText, topLanguages.map(([lang]) => lang));
            if (secondaryCheck) {
                return secondaryCheck;
            }
        }

        return detectedLang;
    }

    /**
     * 二次语言检测，用于区分相似的语言
     * @param {string} text 文本内容
     * @param {string[]} candidates 候选语言列表
     * @returns {string} 最终检测结果
     */
    function performSecondaryLanguageCheck(text, candidates) {
        // JavaScript vs TypeScript
        if (candidates.includes('javascript') && candidates.includes('typescript')) {
            // 如果有明确的TS特征，返回TypeScript
            if (/:\s*(string|number|boolean|any|void|never|unknown)\b/.test(text) ||
                /\binterface\s+\w+/.test(text) ||
                /\btype\s+\w+\s*=/.test(text)) {
                return 'typescript';
            }
            // 否则返回JavaScript
            return 'javascript';
        }

        // C vs C++
        if (candidates.includes('c') && candidates.includes('cpp')) {
            // 如果有C++特征，返回C++
            if (/std::|using\s+namespace|#include\s*<iostream>/.test(text)) {
                return 'cpp';
            }
            // 否则返回C
            return 'c';
        }

        // HTML vs XML
        if (candidates.includes('html') && candidates.includes('xml')) {
            // 如果有HTML特征，返回HTML
            if (/<\/?(div|span|p|h[1-6]|body|head|meta|link)\b/i.test(text)) {
                return 'html';
            }
            // 如果有XML声明，返回XML
            if (/<\?xml/.test(text)) {
                return 'xml';
            }
        }

        // 如果无法区分，返回第一个候选
        return candidates[0] || '';
    }

    /**
     * 创建云剪贴板
     * @param {string} content 剪贴板内容
     * @returns {Promise<Object>} 创建结果
     */
    async function createPaste(content) {
        try {
            // 检测语言
            const detectedLang = detectLanguage(content);

            // 准备请求数据
            const data = {
                title: `聊天中的代码片段 - ${new Date().toLocaleString()}`,
                content: content,
                language: detectedLang,
                isPrivate: false,
                expiryDays: 30 // 30天后过期
            };

            // 发送创建请求
            const response = await fetch(`${config.serverUrl}/paste/update`, {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json'
                },
                body: JSON.stringify(data),
                credentials: 'include'
            });

            if (!response.ok) {
                throw new Error(`服务器响应错误: ${response.status}`);
            }

            const result = await response.json();

            if (!result.success) {
                throw new Error(result.message || '创建剪贴板失败');
            }

            return result.data;
        } catch (error) {
            console.error('创建剪贴板失败:', error);
            throw error;
        }
    }

    /**
     * 获取剪贴板内容
     * @param {number} pasteId 剪贴板ID
     * @returns {Promise<Object>} 剪贴板数据
     */
    async function getPaste(pasteId) {
        try {
            // 如果已经缓存了这个剪贴板内容，直接返回
            if (renderedPastes.has(pasteId)) {
                return renderedPastes.get(pasteId);
            }

            // 否则从服务器获取
            const controller = new AbortController();
            const timeoutId = setTimeout(() => controller.abort(), config.timeout);

            const response = await fetch(`${config.serverUrl}/paste/query?id=${pasteId}`, {
                method: 'GET',
                credentials: 'include',
                signal: controller.signal
            });

            clearTimeout(timeoutId);

            if (!response.ok) {
                throw new Error(`服务器响应错误: ${response.status}`);
            }

            const result = await response.json();

            if (!result.success) {
                throw new Error(result.message || '获取剪贴板失败');
            }

            // 缓存结果
            renderedPastes.set(pasteId, result.data);

            return result.data;
        } catch (error) {
            console.error('获取剪贴板失败:', error);
            throw error;
        }
    }

    /**
     * 从消息文本中提取所有剪贴板ID
     * @param {string} messageText 消息文本
     * @returns {number[]} 剪贴板ID数组
     */
    function extractAllPasteIds(messageText) {
        if (!messageText) return [];

        const pasteIds = [];
        // 支持两种格式：旧格式 __PASTE__(\d+)__ 和新格式 %%PASTE(\d+)%%
        const pasteRegex = /(?:__PASTE__(\d+)__|%%PASTE(\d+)%%)/g;
        let match;

        while ((match = pasteRegex.exec(messageText)) !== null) {
            // match[1]是旧格式的ID，match[2]是新格式的ID，取非空的那个
            const id = parseInt(match[1] || match[2]);
            pasteIds.push(id);
        }

        return pasteIds;
    }

    /**
     * 从消息文本中提取剪贴板ID (保留原函数用于兼容性)
     * @param {string} messageText 消息文本
     * @returns {number|null} 剪贴板ID或null
     */
    function extractPasteId(messageText) {
        if (!messageText) return null;

        // 同时支持旧格式和新格式
        const pasteMatch = messageText.match(/__PASTE__(\d+)__/) || messageText.match(/%%PASTE(\d+)%%/);
        return pasteMatch ? parseInt(pasteMatch[1]) : null;
    }

    /**
     * 将文本转换为剪贴板链接格式
     * @param {number} pasteId 剪贴板ID
     * @returns {string} 格式化的剪贴板链接
     */
    function formatPasteLink(pasteId) {
        // 使用新的格式
        return `%%PASTE${pasteId}%%`;
    }

    /**
     * 渲染剪贴板内容为HTML
     * @param {Object} paste 剪贴板数据
     * @returns {string} HTML字符串
     */
    function renderPasteContent(paste) {
        if (!paste) return '<div class="paste-error">无法加载剪贴板内容</div>';

        const safeTitle = escapeHtml(paste.title || '无标题');
        const safeLanguage = escapeHtml(paste.language || '纯文本');

        // 安全处理代码内容
        let codeContent = escapeHtml(paste.content || '');

        // 确定是否显示行号
        const showLineNumbers = paste.content && paste.content.split('\n').length > 3;

        // 生成HTML
        return `
            <div class="inline-paste">
                <div class="inline-paste-header">
                    <div class="inline-paste-title">${safeTitle}</div>
                    <div class="inline-paste-meta">
                        <span class="inline-paste-language">${safeLanguage}</span>
                        <a href="/paste/view?id=${paste.id}" target="_blank" class="inline-paste-link">查看完整</a>
                    </div>
                </div>
                <div class="inline-paste-content">
                    <pre class="code-block ${showLineNumbers ? 'with-line-numbers' : ''}"><code class="language-${paste.language || 'plaintext'}">${codeContent}</code></pre>
                </div>
            </div>
        `;
    }

    /**
     * ���HTML特殊字符进行转义
     * @param {string} text 原始文本
     * @returns {string} 转义后的文本
     */
    function escapeHtml(text) {
        if (!text) return '';
        const div = document.createElement('div');
        div.textContent = text;
        return div.innerHTML;
    }

    /**
     * 高亮代码块
     * @param {Element} element 包含代码的元素
     */
    function highlightCodeBlock(element) {
        if (!element) return;

        // 确保hljs已加载
        if (!hljs && window.hljs) {
            hljs = window.hljs;
        }

        // 如果有hljs可用
        if (hljs) {
            try {
                const codeElements = element.querySelectorAll('pre code');
                codeElements.forEach(block => {
                    // 获取代码块语言
                    const classes = block.className.split(' ');
                    const languageClass = classes.find(cls => cls.startsWith('language-'));
                    const language = languageClass ? languageClass.replace('language-', '') : '';
                    
                    // 如果是有效语言，应用高亮
                    if (language && language !== 'plaintext') {
                        hljs.highlightElement(block);
                    } else {
                        // 尝试自动检测语言
                        hljs.highlightAuto(block);
                    }
                });
            } catch (e) {
                console.error('代码高亮失败:', e);
            }
        } else {
            // 尝试加载hljs
            loadHighlightJs().then(() => {
                highlightCodeBlock(element);
            }).catch(err => {
                console.error('加载高亮库失败:', err);
            });
        }
    }

    /**
     * 处理粘贴事件
     * @param {Element} inputElement 输入框元素
     * @param {ClipboardEvent} event 粘贴事件
     * @returns {Promise<boolean>} 是否已处理为剪贴板
     */
    async function handlePaste(inputElement, event) {
        // 获取粘贴的文本
        const clipboardData = event.clipboardData || window.clipboardData;
        const pastedText = clipboardData.getData('text');

        // 如果没有文本或文本不够长，直接返回
        if (!pastedText || !isLargeTextBlock(pastedText)) {
            return false;
        }

        // 询问用户是否要创建剪贴板
        const createPasteConfirm = confirm(
            "检测到粘贴了大段文本，是否创建剪贴板？\n" +
            "这将使消息更加整洁，并提供代码高亮功能。"
        );

        if (!createPasteConfirm) {
            return false;
        }

        try {
            // 阻止默认粘贴行为
            event.preventDefault();

            // 显示加载提示
            const originalValue = inputElement.value;
            const cursorPosition = inputElement.selectionStart;
            inputElement.value = originalValue.substring(0, cursorPosition) +
                "【创建剪贴板中...】" +
                originalValue.substring(inputElement.selectionEnd);

            // 创建剪贴板
            const paste = await createPaste(pastedText);

            // 更新输入框内容，插入剪贴板链接
            const pasteLink = formatPasteLink(paste.id);
            inputElement.value = originalValue.substring(0, cursorPosition) +
                pasteLink +
                originalValue.substring(inputElement.selectionEnd);

            // 更新光标位置
            const newCursorPosition = cursorPosition + pasteLink.length;
            inputElement.setSelectionRange(newCursorPosition, newCursorPosition);

            return true;
        } catch (error) {
            console.error('处理粘贴时出错:', error);
            alert('创建剪贴板失败: ' + error.message);
            return false;
        }
    }

    /**
     * 处理消息渲染
     * @param {Element} messageElement 消息元素
     * @param {string} messageText 消息文本
     */
    async function handleMessageRender(messageElement, messageText) {
        if (!messageElement || !messageText) return;

        console.log("处理剪贴板渲染:", messageText.includes('__PASTE__') || messageText.includes('%%PASTE'));

        // 提取所有剪贴板ID
        const pasteIds = extractAllPasteIds(messageText);
        if (pasteIds.length === 0) return;

        console.log("找到剪贴板ID列表:", pasteIds);

        try {
            // 确保高亮库已加载
            await loadHighlightJs();
            
            // 找到消息体元素，增加更多兼容性查找
            let messageBody = messageElement.querySelector('.message-body');

            // 如果没找到标准message-body，尝试查找其他可能包含消息内容的元素
            if (!messageBody) {
                messageBody = messageElement.querySelector('[class*="message"]') ||
                    messageElement.querySelector('div > div:last-child');

                if (!messageBody) {
                    console.error('无法找到消息内容元素');
                    return;
                }
            }

            // 检查内容是否已经包含渲染过的剪贴���
            if (messageBody.querySelector('.inline-paste')) {
                console.log('剪贴板已渲染，跳过');
                return;
            }

            // 存储原始HTML内容
            let currentHTML = messageBody.innerHTML;
            let hasChanges = false;

            // 逐个处理每个剪贴板引用
            for (const pasteId of pasteIds) {
                console.log("处理剪贴板ID:", pasteId);

                // 确认此ID的引用还存在于当前HTML中 (检查两种格式)
                const oldFormat = `__PASTE__${pasteId}__`;
                const newFormat = `%%PASTE${pasteId}%%`;

                if (!currentHTML.includes(oldFormat) && !currentHTML.includes(newFormat)) {
                    console.log(`剪贴板 ${pasteId} 引用不存在或已处理，跳过`);
                    continue;
                }

                try {
                    // 获取剪贴板内容
                    const paste = await getPaste(pasteId);

                    // 替换掉对应的剪贴板引用 (同时支持两种格式)
                    const oldFormatPattern = new RegExp(`__PASTE__${pasteId}__`, 'g');
                    const newFormatPattern = new RegExp(`%%PASTE${pasteId}%%`, 'g');
                    const renderedContent = renderPasteContent(paste);

                    // 更新当前HTML
                    currentHTML = currentHTML.replace(oldFormatPattern, renderedContent)
                        .replace(newFormatPattern, renderedContent);
                    hasChanges = true;
                } catch (error) {
                    console.error(`处理剪贴板 ${pasteId} 时出错:`, error);

                    // 替换为错误提示 (同时支持两种格式)
                    const oldFormatPattern = new RegExp(`__PASTE__${pasteId}__`, 'g');
                    const newFormatPattern = new RegExp(`%%PASTE${pasteId}%%`, 'g');
                    const errorHTML = `<div class="paste-error">无法加载剪贴板内容 (ID: ${pasteId}): ${error.message || '未知错误'}</div>`;

                    // 更新当前HTML
                    currentHTML = currentHTML.replace(oldFormatPattern, errorHTML)
                        .replace(newFormatPattern, errorHTML);
                    hasChanges = true;
                }
            }

            // 如果有更改，更新DOM
            if (hasChanges) {
                messageBody.innerHTML = currentHTML;

                // 应用代码高亮
                highlightCodeBlock(messageElement);
                console.log("剪贴板渲染完成，应用高亮");
            }
        } catch (error) {
            console.error('渲染剪贴板内容时出错:', error);

            try {
                // 更强健的错误处理
                const messageBody = messageElement.querySelector('.message-body') ||
                    messageElement.querySelector('[class*="message"]') ||
                    messageElement;

                if (messageBody) {
                    // 为所有未处理的剪贴板引用添加错误提示
                    let currentHTML = messageBody.innerHTML;
                    // 同时匹配两种格式
                    const pasteRegex = /(?:__PASTE__(\d+)__|%%PASTE(\d+)%%)/g;
                    let match;

                    while ((match = pasteRegex.exec(messageBody.innerHTML)) !== null) {
                        const pasteId = match[1] || match[2]; // 获取匹配的ID
                        const matchedFormat = match[0]; // 获取完整的匹配字符串
                        const errorHTML = `<div class="paste-error">无法加载剪贴板内容 (ID: ${pasteId}): ${error.message || '未知错误'}</div>`;

                        // 使用精确的替换
                        currentHTML = currentHTML.replace(matchedFormat, errorHTML);
                    }

                    messageBody.innerHTML = currentHTML;
                }
            } catch (e) {
                // 即使错误处理失败也不中断聊天室功能
                console.error('处理剪贴板错误信息时失败:', e);
            }
        }
    }

    // 注入CSS样式
    function injectStyles() {
        const styleElement = document.createElement('style');
        styleElement.textContent = `
            /* 内联剪贴板样式 */
            .inline-paste {
                margin: 10px 0;
                border-radius: 8px;
                overflow: hidden;
                border: 1px solid #ddd;
                background: #f8f9fa;
                max-width: 100%;  /* 限制最大宽度 */
                box-sizing: border-box;
            }
            
            body.dark-mode .inline-paste {
                border-color: #444;
                background: #2a2a2a;
            }
            
            .inline-paste-header {
                display: flex;
                justify-content: space-between;
                align-items: center;
                padding: 8px 12px;
                background: #e9ecef;
                border-bottom: 1px solid #ddd;
            }
            
            body.dark-mode .inline-paste-header {
                background: #343a40;
                border-color: #444;
            }
            
            .inline-paste-title {
                font-weight: 500;
                font-size: 14px;
                color: #495057;
                flex: 1;
                white-space: nowrap;
                overflow: hidden;
                text-overflow: ellipsis;
            }
            
            body.dark-mode .inline-paste-title {
                color: #e9ecef;
            }
            
            .inline-paste-meta {
                display: flex;
                align-items: center;
                gap: 10px;
                font-size: 12px;
                flex-shrink: 0;
            }
            
            .inline-paste-language {
                padding: 2px 6px;
                background: #dee2e6;
                border-radius: 4px;
                color: #495057;
            }
            
            body.dark-mode .inline-paste-language {
                background: #495057;
                color: #dee2e6;
            }
            
            .inline-paste-link {
                color: #007bff;
                text-decoration: none;
            }
            
            body.dark-mode .inline-paste-link {
                color: #6ea8fe;
            }
            
            .inline-paste-link:hover {
                text-decoration: underline;
            }
            
            .inline-paste-content {
                max-height: 300px;
                overflow: auto;
            }
            
            .inline-paste pre.code-block {
                margin: 0;
                padding: 12px;
                font-family: "SFMono-Regular", Consolas, "Liberation Mono", Menlo, monospace;
                font-size: 13px;
                line-height: 1.5;
                white-space: pre;
                overflow-x: auto;
                tab-size: 4;
                max-width: 100%;  /* 确保代码不超出容器 */
                background-color: #f8f9fa;
            }
            
            body.dark-mode .inline-paste pre.code-block {
                background-color: #282c34;
                color: #abb2bf;
            }
            
            .inline-paste code {
                background: transparent;
                padding: 0;
                color: inherit;
                max-width: 100%;
                display: block;
                overflow-x: auto;
                word-wrap: normal;
            }
            
            .paste-error {
                padding: 10px;
                color: #842029;
                background-color: #f8d7da;
                border: 1px solid #f5c2c7;
                border-radius: 6px;
                margin: 5px 0;
                font-size: 14px;
                max-width: 100%;
                word-wrap: break-word;
            }
            
            body.dark-mode .paste-error {
                color: #ea868f;
                background-color: #2c0b0e;
                border-color: #842029;
            }
            
            /* 确保消息内容不会被溢出的内容破坏 */
            .message-body {
                max-width: 100%;
                overflow-wrap: break-word;
                word-break: break-word;
            }

            /* 适配hljs样式 */
            .hljs {
                display: block;
                overflow-x: auto;
                padding: 0.5em;
                color: #383a42;
                background: #f8f9fa;
            }
            
            body.dark-mode .hljs {
                background: #282c34;
                color: #abb2bf;
            }
            
            /* 语法高亮样式 */
            .hljs-comment,
            .hljs-quote {
                color: #a0a1a7;
                font-style: italic;
            }
            
            .hljs-doctag,
            .hljs-keyword,
            .hljs-formula {
                color: #a626a4;
            }
            
            .hljs-section,
            .hljs-name,
            .hljs-selector-tag,
            .hljs-deletion,
            .hljs-subst {
                color: #e45649;
            }
            
            .hljs-literal {
                color: #0184bb;
            }
            
            .hljs-string,
            .hljs-regexp,
            .hljs-addition,
            .hljs-attribute,
            .hljs-meta-string {
                color: #50a14f;
            }
            
            .hljs-built_in,
            .hljs-class .hljs-title {
                color: #c18401;
            }
            
            .hljs-attr,
            .hljs-variable,
            .hljs-template-variable,
            .hljs-type,
            .hljs-selector-class,
            .hljs-selector-attr,
            .hljs-selector-pseudo,
            .hljs-number {
                color: #986801;
            }
            
            .hljs-symbol,
            .hljs-bullet,
            .hljs-link,
            .hljs-meta,
            .hljs-selector-id,
            .hljs-title {
                color: #4078f2;
            }
            
            .hljs-emphasis {
                font-style: italic;
            }
            
            .hljs-strong {
                font-weight: bold;
            }
            
            .hljs-link {
                text-decoration: underline;
            }
            
            /* 深色模式下的高亮样式 */
            body.dark-mode .hljs-comment,
            body.dark-mode .hljs-quote {
                color: #5c6370;
            }
            
            body.dark-mode .hljs-doctag,
            body.dark-mode .hljs-keyword,
            body.dark-mode .hljs-formula {
                color: #c678dd;
            }
            
            body.dark-mode .hljs-section,
            body.dark-mode .hljs-name,
            body.dark-mode .hljs-selector-tag,
            body.dark-mode .hljs-deletion,
            body.dark-mode .hljs-subst {
                color: #e06c75;
            }
            
            body.dark-mode .hljs-literal {
                color: #56b6c2;
            }
            
            body.dark-mode .hljs-string,
            body.dark-mode .hljs-regexp,
            body.dark-mode .hljs-addition,
            body.dark-mode .hljs-attribute,
            body.dark-mode .hljs-meta-string {
                color: #98c379;
            }
            
            body.dark-mode .hljs-built_in,
            body.dark-mode .hljs-class .hljs-title {
                color: #e6c07b;
            }
            
            body.dark-mode .hljs-attr,
            body.dark-mode .hljs-variable,
            body.dark-mode .hljs-template-variable,
            body.dark-mode .hljs-type,
            body.dark-mode .hljs-selector-class,
            body.dark-mode .hljs-selector-attr,
            body.dark-mode .hljs-selector-pseudo,
            body.dark-mode .hljs-number {
                color: #d19a66;
            }
            
            body.dark-mode .hljs-symbol,
            body.dark-mode .hljs-bullet,
            body.dark-mode .hljs-link,
            body.dark-mode .hljs-meta,
            body.dark-mode .hljs-selector-id,
            body.dark-mode .hljs-title {
                color: #61afef;
            }
        `;
        document.head.appendChild(styleElement);
    }

    /**
     * 初始化函数 - 在模块加载时调用
     */
    function init() {
        console.log("初始化ChatroomPaste模块");
        injectStyles();
        
        // 尝试预加载高亮库
        loadHighlightJs().then(() => {
            console.log("代码高亮库加载成功");
        }).catch(err => {
            console.error("代码高亮库加载失败，将在需要时重试", err);
        });

        // 检查是否有未处理的剪贴板引用
        setTimeout(() => {
            document.querySelectorAll('.message').forEach(msgEl => {
                const msgBody = msgEl.querySelector('.message-body');
                if (msgBody && (msgBody.innerHTML.includes('__PASTE__') || msgBody.innerHTML.includes('%%PASTE'))) {
                    handleMessageRender(msgEl, msgBody.innerHTML);
                }
            });
        }, 500);
    }

    // 公开API
    return {
        init,
        handlePaste,
        handleMessageRender,
        createPaste,
        getPaste,
        extractPasteId,
        extractAllPasteIds, // 导出新函���
        formatPasteLink
    };
})();

// 在DOM加载完成后初始化
if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', function() {
        ChatroomPaste.init();
    });
} else {
    // DOM已经加载完成，直接初始化
    ChatroomPaste.init();
}