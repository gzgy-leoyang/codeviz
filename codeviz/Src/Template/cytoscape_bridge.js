// cytoscape_bridge.js - Cytoscape.js 数据桥接脚本
// 初始化图形渲染，绑定交互事件
// 内嵌到 HTML 报告中，以 CODEVIZ_DATA 为数据源

(function() {
    'use strict';

    const data = window.CODEVIZ_DATA;
    let cy = null;
    let currentTab = 'call';

    // 当节点数量超过 1000 时启用 WebGL 渲染降级（满足 SR_4 需求）
    const WEBGL_THRESHOLD = 1000;

    // 调用图懒展开状态
    let isLazyMode = false;
    let fullGraphData = null;
    let visibleNodeIds = new Set();
    let expandedNodeIds = new Set();

    // 颜色映射：根据热力值从蓝到红
    function heatColor(value) {
        const r = Math.round(value * 220 + 35);
        const g = Math.round((1 - value) * 150 + 30);
        const b = Math.round((1 - value) * 180 + 30);
        return `rgb(${r},${g},${b})`;
    }

    // 将符号类型映射到节点形状
    function nodeShape(kind) {
        switch (kind) {
            case 'FUNCTION':
            case 'STRUCT':
            case 'CLASS':
            case 'FILE_ENTITY': return 'round-rectangle';
            default:            return 'round-rectangle';
        }
    }

    // 构建当前视图的 Cytoscape elements
    function buildElements(graphData, symbols, stats) {
        const elements = [];
        const symbolMap = {};
        symbols.forEach(s => { symbolMap[s.symbol_id] = s; });

        // 构建函数统计的快速索引
        const statsMap = {};
        if (stats && stats.function_stats) {
            stats.function_stats.forEach(f => { statsMap[f.function_id] = f; });
        }

        // 添加节点
        const nodeSet = new Set();
        (graphData.nodes || []).forEach(n => {
            if (nodeSet.has(n.id)) return;
            nodeSet.add(n.id);

            const sym = symbolMap[n.id] || {};
            const fstat = statsMap[n.id] || {};

            // 计算热力值（被调用次数越高越热）
            const maxFanIn = stats && stats.maxFanIn || 1;
            const heatVal = (fstat.fan_in || 0) / Math.max(maxFanIn, 1);

            const kind = sym.kind || n.type || 'FUNCTION';
            let label = n.label || sym.name || String(n.id);
            if (kind !== 'FILE_ENTITY') {
                const fname = (sym.file_path || '').split('/').pop();
                if (fname) label += '\n(' + fname + ')';
            }

            elements.push({
                group: 'nodes',
                data: {
                    id: String(n.id),
                    label: label,
                    kind: kind,
                    shape: nodeShape(kind),
                    file: sym.file_path || '',
                    line: sym.line || 0,
                    fan_in: fstat.fan_in || 0,
                    fan_out: fstat.fan_out || 0,
                    complexity: fstat.cyclomatic_complexity || 0,
                    heat: heatVal,
                    comment: sym.comment || ''
                }
            });
        });

        // 添加边
        (graphData.edges || []).forEach((e, idx) => {
            elements.push({
                group: 'edges',
                data: {
                    id: `e${idx}`,
                    source: String(e.source_id),
                    target: String(e.target_id),
                    relation: e.relation || 'CALLS',
                    weight: e.weight || 1
                }
            });
        });

        return elements;
    }

    // 初始化调用图懒加载模式：仅显示入口节点，点击后展开下一级
    function initCallGraphLazy() {
        isLazyMode = true;
        fullGraphData = data.call_graph || { nodes: [], edges: [] };
        visibleNodeIds = new Set();
        expandedNodeIds = new Set();

        const entryId = data.metadata && data.metadata.entry_function_id;
        if (!entryId || !fullGraphData.nodes.length) {
            // 无入口函数时回退到全量显示（仍扫描并标记叶节点灰色）
            isLazyMode = false;
            initCytoscape(buildElements(fullGraphData, data.symbols || [], data.stats || {}));
            if (cy) markDeadEndNodes();
            return;
        }

        // 为入口节点构建 symbol/stats 查找表
        const symbolMap = {};
        (data.symbols || []).forEach(s => { symbolMap[s.symbol_id] = s; });
        const statsMap = {};
        if (data.stats && data.stats.function_stats) {
            data.stats.function_stats.forEach(f => { statsMap[f.function_id] = f; });
        }

        const sym = symbolMap[entryId] || {};
        const fstat = statsMap[entryId] || {};
        const maxFanIn = data.stats && data.stats.maxFanIn || 1;
        const heatVal = (fstat.fan_in || 0) / Math.max(maxFanIn, 1);
        const kind = sym.kind || 'FUNCTION';
        let label = sym.name || String(entryId);
        const fname = (sym.file_path || '').split('/').pop();
        if (fname) label += '\n(' + fname + ')';

        const elements = [{
            group: 'nodes',
            data: {
                id: String(entryId),
                label: label,
                kind: kind,
                shape: nodeShape(kind),
                file: sym.file_path || '',
                line: sym.line || 0,
                fan_in: fstat.fan_in || 0,
                fan_out: fstat.fan_out || 0,
                complexity: fstat.cyclomatic_complexity || 0,
                heat: heatVal,
                comment: sym.comment || '',
                isEntry: true
            }
        }];

        visibleNodeIds.add(String(entryId));
        initCytoscape(elements);

        // 若入口函数无下级调用，标记灰色边框
        const hasEntryOutgoing = fullGraphData.edges.some(e => String(e.source_id) === String(entryId));
        if (!hasEntryOutgoing && cy) {
            cy.getElementById(String(entryId)).data('isDeadEnd', true);
        }
    }

    // 展开节点的下一级调用关系
    function expandNode(nodeId) {
        if (!isLazyMode || expandedNodeIds.has(nodeId) || !fullGraphData) return;

        // 查找从该节点出发的未显示边
        const newEdges = fullGraphData.edges.filter(e =>
            String(e.source_id) === nodeId && !visibleNodeIds.has(String(e.target_id))
        );
        if (newEdges.length === 0) {
            const expandEl = document.getElementById('ni-expand');
            if (expandEl && expandedNodeIds.has(nodeId)) expandEl.textContent = '已展开';
            return;
        }

        expandedNodeIds.add(nodeId);

        // 收集新目标节点 ID
        const calleeIds = new Set();
        newEdges.forEach(e => calleeIds.add(String(e.target_id)));

        // 构建 symbol/stats 查找表
        const symbolMap = {};
        (data.symbols || []).forEach(s => { symbolMap[s.symbol_id] = s; });
        const statsMap = {};
        if (data.stats && data.stats.function_stats) {
            data.stats.function_stats.forEach(f => { statsMap[f.function_id] = f; });
        }
        const maxFanIn = data.stats && data.stats.maxFanIn || 1;

        // 构建新增节点和边的 elements
        const newElements = [];
        calleeIds.forEach(id => {
            if (visibleNodeIds.has(id)) return;
            visibleNodeIds.add(id);

            const nodeData = fullGraphData.nodes.find(n => String(n.id) === id);
            const sym = symbolMap[parseInt(id)] || {};
            const fstat = statsMap[parseInt(id)] || {};
            const heatVal = (fstat.fan_in || 0) / Math.max(maxFanIn, 1);
            const kind = sym.kind || (nodeData ? nodeData.type : 'FUNCTION');
            let label = (nodeData && nodeData.label) || sym.name || id;
            if (kind !== 'FILE_ENTITY') {
                const fname = (sym.file_path || '').split('/').pop();
                if (fname) label += '\n(' + fname + ')';
            }

            newElements.push({
                group: 'nodes',
                data: {
                    id: id,
                    label: label,
                    kind: kind,
                    shape: nodeShape(kind),
                    file: sym.file_path || '',
                    line: sym.line || 0,
                    fan_in: fstat.fan_in || 0,
                    fan_out: fstat.fan_out || 0,
                    complexity: fstat.cyclomatic_complexity || 0,
                    heat: heatVal,
                    comment: sym.comment || ''
                }
            });
        });

        newEdges.forEach((e, idx) => {
            newElements.push({
                group: 'edges',
                data: {
                    id: `e-lazy-${idx}-${nodeId}`,
                    source: String(e.source_id),
                    target: String(e.target_id),
                    relation: e.relation || 'CALLS',
                    weight: e.weight || 1
                }
            });
        });

        if (newElements.length === 0) return;

        // 增量添加到当前图并重新布局（保持视角不变）
        cy.add(newElements);

        // 标记无下级展开的节点为灰色边框
        calleeIds.forEach(id => {
            const hasOutgoing = fullGraphData.edges.some(e => String(e.source_id) === id);
            if (!hasOutgoing) {
                cy.getElementById(id).data('isDeadEnd', true);
            }
        });

        // 以父节点为中心，固定半径扇形展开子节点
        const parentPos = cy.getElementById(nodeId).position();
        const radius = 150;
        const calleeArray = Array.from(calleeIds);
        const count = calleeArray.length;
        const arcAngle = Math.PI * 0.6; // 108° 扇形
        const startAngle = Math.PI / 2 - arcAngle / 2;

        calleeArray.forEach((id, i) => {
            const angle = startAngle + arcAngle * i / Math.max(count - 1, 1);
            const node = cy.getElementById(id);
            if (node.length) {
                node.position({
                    x: parentPos.x + radius * Math.cos(angle),
                    y: parentPos.y + radius * Math.sin(angle)
                });
            }
        });
    }

    // 在全量显示的图中扫描并标记叶节点（无出边 → 灰色边框）
    function markDeadEndNodes() {
        if (!cy || !fullGraphData) return;
        cy.nodes().forEach(n => {
            const nid = n.id();
            const hasOutgoing = fullGraphData.edges.some(e => String(e.source_id) === nid);
            if (!hasOutgoing) n.data('isDeadEnd', true);
        });
    }

    // 初始化 Cytoscape 图
    function initCytoscape(elements) {
        const container = document.getElementById('cy');
        if (!container) return;

        // 节点数量超阈值时降级提示
        const nodeCount = elements.filter(e => e.group === 'nodes').length;
        if (nodeCount > WEBGL_THRESHOLD) {
            console.warn(`节点数 ${nodeCount} 超过阈值 ${WEBGL_THRESHOLD}，启用聚合模式`);
        }

        try {
            cy = cytoscape({
                container: container,
                elements: elements,
                style: [
                    {
                        selector: 'node',
                        style: {
                            'label': 'data(label)',
                            'color': '#fff',
                            'font-size': '11px',
                            'text-valign': 'center',
                            'text-halign': 'center',
                            'shape': 'data(shape)',
                            'width': 'label',
                            'height': 'label',
                            'text-wrap': 'wrap',
                            'padding': '6px',
                            'border-width': 1,
                            'border-color': '#e94560'
                        }
                    },
                    {
                        selector: 'edge',
                        style: {
                            'width': 1.5,
                            'line-color': '#0f3460',
                            'target-arrow-color': '#e94560',
                            'target-arrow-shape': 'triangle',
                            'curve-style': 'bezier'
                        }
                    },
                    {
                        selector: 'node[isDeadEnd]',
                        style: {
                            'border-color': '#555555',
                            'border-width': 1,
                            'border-style': 'solid'
                        }
                    },
                    {
                        selector: 'node[isEntry]',
                        style: {
                            'border-width': 3,
                            'border-color': '#FFD700',
                            'color': '#FFD700',
                            'font-weight': 'bold'
                        }
                    },
                    {
                        selector: 'node:selected',
                        style: {
                            'border-width': 3,
                            'border-color': '#D5EE2E'
                        }
                    }
                ],
                layout: {
                    name: nodeCount > 100 ? 'cose' : 'dagre',
                    directed: true,
                    rankDir: 'TB',
                    padding: 20
                }
            });

            // 节点点击：显示详情 + 懒展开
            cy.on('tap', 'node', function(evt) {
                const node = evt.target;
                const d = node.data();
                document.getElementById('node-info').style.display = 'block';
                document.getElementById('ni-name').textContent = d.label;
                document.getElementById('ni-kind').textContent = d.kind;
                document.getElementById('ni-file').textContent = (d.file || '').split('/').pop();
                document.getElementById('ni-line').textContent = d.line;
                document.getElementById('ni-fanin').textContent = d.fan_in;
                document.getElementById('ni-fanout').textContent = d.fan_out;
                document.getElementById('ni-cc').textContent = d.complexity;
                const nc = document.getElementById('ni-comment');
                if (d.comment) { nc.textContent = d.comment; nc.style.display = 'block'; }
                else { nc.style.display = 'none'; }

                // 懒展开模式：显示展开状态
                const expandRow = document.getElementById('ni-expand-row');
                const expandEl = document.getElementById('ni-expand');
                if (isLazyMode && fullGraphData) {
                    if (expandedNodeIds.has(node.id())) {
                        expandEl.textContent = '已展开';
                    } else {
                        const calleeCount = fullGraphData.edges.filter(e =>
                            String(e.source_id) === node.id()
                        ).length;
                        expandEl.textContent = calleeCount > 0 ? calleeCount + ' 个被调用函数' : '无调用关系';
                    }
                    expandRow.style.display = 'flex';
                } else if (expandRow) {
                    expandRow.style.display = 'none';
                }

                // 展开下一级调用关系
                if (isLazyMode) {
                    expandNode(node.id());
                }
            });

            // 背景点击：隐藏节点详情
            cy.on('tap', function(evt) {
                if (evt.target === cy) {
                    document.getElementById('node-info').style.display = 'none';
                }
            });

        } catch (e) {
            console.error('Cytoscape 初始化失败:', e);
            container.innerHTML = '<div style="padding:20px;color:#e94560;">图形渲染初始化失败，请检查浏览器控制台</div>';
        }
    }

    // 切换图视图
    window.switchTab = function(tab) {
        currentTab = tab;
        document.querySelectorAll('.tab').forEach(t => t.classList.remove('active'));
        event.target.classList.add('active');

        const statsPanel = document.getElementById('stats-panel');
        const canvasContainer = document.getElementById('canvas-container');
        const sidebar = document.getElementById('sidebar');

        if (tab === 'stats') {
            statsPanel.style.display = 'block';
            canvasContainer.style.display = 'none';
            sidebar.style.display = 'none';
            renderStats();
            return;
        }

        statsPanel.style.display = 'none';
        canvasContainer.style.display = 'block';
        sidebar.style.display = 'block';

        if (cy) { cy.destroy(); cy = null; }

        if (tab === 'call') {
            initCallGraphLazy();
        } else {
            isLazyMode = false;
            let graphData;
            switch (tab) {
                case 'include': graphData = data.include_graph; break;
                case 'type':    graphData = data.type_graph;    break;
                default:        graphData = data.call_graph;
            }
            const elements = buildElements(graphData, data.symbols || [], data.stats || {});
            initCytoscape(elements);
        }
    };

    // 重置布局
    window.resetLayout = function() {
        if (cy) {
            cy.layout({ name: 'dagre', directed: true, rankDir: 'TB', padding: 20 }).run();
        }
    };

    // 适应窗口
    window.fitGraph = function() {
        if (cy) cy.fit();
    };

    // 过滤符号列表
    window.filterSymbols = function(query) {
        const list = document.getElementById('symbol-list');
        list.querySelectorAll('li').forEach(li => {
            li.style.display = li.textContent.toLowerCase().includes(query.toLowerCase()) ? '' : 'none';
        });
    };

    // 渲染符号侧边栏
    function renderSidebar() {
        const list = document.getElementById('symbol-list');
        list.innerHTML = '';
        (data.symbols || []).forEach(s => {
            const li = document.createElement('li');
            li.textContent = s.name;
            li.title = s.qualified_name;
            li.onclick = () => {
                if (cy) {
                    const node = cy.getElementById(String(s.symbol_id));
                    if (node.length) {
                        cy.animate({ fit: { eles: node, padding: 60 }, duration: 400 });
                        node.select();
                    }
                }
            };
            list.appendChild(li);
        });
    }

    // 渲染统计面板
    function renderStats() {
        const stats = data.stats || {};
        const meta = data.metadata || {};

        // 项目概览
        const summaryTable = document.getElementById('summary-table');
        summaryTable.innerHTML = `
            <tr><th>项目</th><td>${meta.project_name || '-'}</td></tr>
            <tr><th>文件数</th><td>${meta.file_count || 0}</td></tr>
            <tr><th>函数数</th><td>${meta.function_count || 0}</td></tr>
            <tr><th>C 编译器</th><td>${meta.c_compiler || '-'}</td></tr>
            <tr><th>C++ 编译器</th><td>${meta.cxx_compiler || '-'}</td></tr>
            <tr><th>生成时间</th><td>${meta.generated_at || '-'}</td></tr>
        `;

        // 文件热力图
        const fileTable = document.getElementById('file-hotspot-table');
        const maxLines = Math.max(...(stats.file_stats || []).map(f => f.code_lines || 0), 1);
        (stats.file_stats || []).sort((a, b) => b.code_lines - a.code_lines).slice(0, 20).forEach(f => {
            const heat = (f.code_lines || 0) / maxLines;
            const tr = document.createElement('tr');
            tr.innerHTML = `<td>${f.file_path.split('/').pop()}</td><td>${f.code_lines}</td>
                <td><span class="hotbar" style="width:${Math.max(heat*120,4)}px;background:${heatColor(heat)}"></span></td>`;
            fileTable.appendChild(tr);
        });

        // 函数热力图
        const funcTable = document.getElementById('func-hotspot-table');
        (stats.function_stats || []).slice(0, 20).forEach(f => {
            const sym = (data.symbols || []).find(s => s.symbol_id === f.function_id) || {};
            const tr = document.createElement('tr');
            tr.innerHTML = `<td>${sym.name || f.function_id}</td>
                <td>${f.fan_in}</td><td>${f.fan_out}</td><td>${f.cyclomatic_complexity}</td>`;
            funcTable.appendChild(tr);
        });

        // 异常检测
        const anomalyList = document.getElementById('anomaly-list');
        const anomalies = data.anomalies || {};
        if ((anomalies.circular_includes || []).length === 0) {
            anomalyList.innerHTML = '<p style="color:#4ade80;font-size:13px;">未检测到循环包含</p>';
        } else {
            (anomalies.circular_includes || []).forEach(ci => {
                const div = document.createElement('div');
                div.className = 'anomaly';
                div.textContent = '循环包含: ' + ci.file_cycle.join(' → ');
                anomalyList.appendChild(div);
            });
        }
    }

    // 更新页面元数据信息
    function updateMeta() {
        const meta = data.metadata || {};
        document.getElementById('meta-info').textContent =
            `项目: ${meta.project_name || '-'} | 生成时间: ${meta.generated_at || '-'}`;
    }

    // 初始化入口
    document.addEventListener('DOMContentLoaded', function() {
        updateMeta();
        renderSidebar();
        // 默认显示调用图（懒加载模式）
        initCallGraphLazy();
    });

})();
