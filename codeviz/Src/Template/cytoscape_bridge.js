// cytoscape_bridge.js - Cytoscape.js 数据桥接脚本
// 初始化图形渲染，绑定交互事件
// 内嵌到 HTML 报告中，以 CODEVIZ_DATA 为数据源

(function() {
    'use strict';

    const data = window.CODEVIZ_DATA;
    let cy = null;          // 左侧调用图 Cytoscape 实例
    let cySide = null;      // 右侧面板 Cytoscape 实例
    let currentCard = 'include';

    const WEBGL_THRESHOLD = 1000;

    // 调用图懒展开状态
    let isLazyMode = false;
    let fullGraphData = null;
    let visibleNodeIds = new Set();
    let expandedNodeIds = new Set();
    const expansionChildren = new Map();

    function heatColor(value) {
        const r = Math.round(value * 220 + 35);
        const g = Math.round((1 - value) * 150 + 30);
        const b = Math.round((1 - value) * 180 + 30);
        return `rgb(${r},${g},${b})`;
    }

    function nodeShape(kind) { return 'round-rectangle'; }

    function buildElements(graphData, symbols, stats) {
        const elements = [];
        const symbolMap = {};
        symbols.forEach(s => { symbolMap[s.symbol_id] = s; });
        const statsMap = {};
        if (stats && stats.function_stats) {
            stats.function_stats.forEach(f => { statsMap[f.function_id] = f; });
        }
        const nodeSet = new Set();
        (graphData.nodes || []).forEach(n => {
            if (nodeSet.has(n.id)) return;
            nodeSet.add(n.id);
            const sym = symbolMap[n.id] || {};
            const fstat = statsMap[n.id] || {};
            const maxFanIn = stats && stats.maxFanIn || 1;
            const heatVal = (fstat.fan_in || 0) / Math.max(maxFanIn, 1);
            const kind = sym.kind || n.type || 'FUNCTION';
            let label = n.label || sym.name || String(n.id);
            let fileType = '';
            let fname = '';
            if (kind === 'FILE_ENTITY') {
                const fp = (sym.file_path || n.label || '').toLowerCase();
                fileType = (fp.endsWith('.h') || fp.endsWith('.hpp') || fp.endsWith('.hxx')) ? 'header' : 'source';
            } else {
                fname = (sym.file_path || '').split('/').pop() || '';
            }
            elements.push({
                group: 'nodes',
                data: {
                    id: String(n.id), label, kind, shape: nodeShape(kind),
                    file: sym.file_path || '', line: sym.line || 0,
                    fan_in: fstat.fan_in || 0, fan_out: fstat.fan_out || 0,
                    complexity: fstat.cyclomatic_complexity || 0,
                    heat: heatVal, comment: sym.comment || '',
                    fileType: fileType,
                    fname: fname
                }
            });
        });
        (graphData.edges || []).forEach((e, idx) => {
            elements.push({
                group: 'edges',
                data: {
                    id: `e${idx}`, source: String(e.source_id),
                    target: String(e.target_id), relation: e.relation || 'CALLS',
                    weight: e.weight || 1
                }
            });
        });
        return elements;
    }

    // === 左侧调用图（懒展开）===
    function initCallGraphLazy() {
        isLazyMode = true;
        fullGraphData = data.call_graph || { nodes: [], edges: [] };
        visibleNodeIds = new Set();
        expandedNodeIds = new Set();
        expansionChildren.clear();

        const entryId = data.metadata && data.metadata.entry_function_id;
        if (!entryId || !fullGraphData.nodes.length) {
            isLazyMode = false;
            initCytoscape(buildElements(fullGraphData, data.symbols || [], data.stats || {}), 'cy-call');
            if (cy) markDeadEndNodes();
            return;
        }

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
        const fname = (sym.file_path || '').split('/').pop() || '';

        visibleNodeIds.add(String(entryId));
        initCytoscape([{
            group: 'nodes', data: {
                id: String(entryId), label, kind, shape: 'ellipse',
                file: sym.file_path || '', line: sym.line || 0,
                fan_in: fstat.fan_in || 0, fan_out: fstat.fan_out || 0,
                complexity: fstat.cyclomatic_complexity || 0,
                heat: heatVal, comment: sym.comment || '', isEntry: true,
                fname: fname
            }
        }], 'cy-call');

        const hasEntryOutgoing = fullGraphData.edges.some(e => String(e.source_id) === String(entryId));
        if (!hasEntryOutgoing && cy) {
            cy.getElementById(String(entryId)).data('isDeadEnd', true);
        }
    }

    // Build name->id map from symbols once
    const symNameToId = {};
    (data.symbols || []).forEach(function(s) { symNameToId[s.name] = s.symbol_id; });

    function expandNode(nodeId) {
        if (!isLazyMode || expandedNodeIds.has(nodeId) || !fullGraphData) return;
        const newEdges = fullGraphData.edges.filter(e =>
            String(e.source_id) === nodeId && !visibleNodeIds.has(String(e.target_id))
        );
        if (newEdges.length === 0) {
            // Check if node has external (system) calls
            const nodeName = (data.symbols || []).find(function(s) { return s.symbol_id === parseInt(nodeId); });
            const extCalls = (data.external_refs || []).filter(function(r) {
                return r.caller_name === (nodeName ? nodeName.name : '');
            });
            const expandEl = document.getElementById('ni-expand');
            if (extCalls.length > 0) {
                expandedNodeIds.add(nodeId);
                if (expandEl) expandEl.textContent = '已展开';
                const nc = document.getElementById('ni-comment');
                nc.textContent = '系统函数调用 (' + extCalls.length + '): ' + extCalls.map(function(r) { return r.callee_name; }).join(', ');
                nc.style.display = 'block';
                return;
            }
            if (expandedNodeIds.has(nodeId) && expandEl) expandEl.textContent = '已展开';
            return;
        }
        expandedNodeIds.add(nodeId);
        const calleeIds = new Set();
        newEdges.forEach(e => calleeIds.add(String(e.target_id)));

        const symbolMap = {};
        (data.symbols || []).forEach(s => { symbolMap[s.symbol_id] = s; });
        const statsMap = {};
        if (data.stats && data.stats.function_stats) {
            data.stats.function_stats.forEach(f => { statsMap[f.function_id] = f; });
        }
        const maxFanIn = data.stats && data.stats.maxFanIn || 1;

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
            let efname = '';
            if (kind !== 'FILE_ENTITY') {
                efname = (sym.file_path || '').split('/').pop() || '';
            }
            newElements.push({
                group: 'nodes', data: {
                    id, label, kind, shape: 'round-rectangle',
                    file: sym.file_path || '', line: sym.line || 0,
                    fan_in: fstat.fan_in || 0, fan_out: fstat.fan_out || 0,
                    complexity: fstat.cyclomatic_complexity || 0,
                    heat: heatVal, comment: sym.comment || '',
                    fname: efname
                }
            });
        });
        newEdges.forEach((e, idx) => {
            newElements.push({
                group: 'edges', data: {
                    id: `e-lazy-${idx}-${nodeId}`,
                    source: String(e.source_id), target: String(e.target_id),
                    relation: e.relation || 'CALLS', weight: e.weight || 1
                }
            });
        });
        if (newElements.length === 0) return;

        cy.add(newElements);

        calleeIds.forEach(id => {
            const hasOutgoing = fullGraphData.edges.some(e => String(e.source_id) === id);
            if (!hasOutgoing) cy.getElementById(id).data('isDeadEnd', true);
        });

        const parentPos = cy.getElementById(nodeId).position();
        const radius = 150;
        const calleeArray = Array.from(calleeIds);
        const count = calleeArray.length;
        const arcAngle = Math.PI * 0.6;
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

        const addedEdgeIds = new Set();
        newElements.forEach(el => { if (el.group === 'edges') addedEdgeIds.add(el.data.id); });
        expansionChildren.set(nodeId, { edgeIds: addedEdgeIds, nodeIds: calleeIds });

        // Add file-label companions for expanded nodes
        calleeIds.forEach(id => {
            const n = cy.getElementById(id);
            const fn = n.data('fname');
            if (!fn) return;
            const fid = '_fl_' + id;
            if (cy.getElementById(fid).length) return;
            cy.add({
                group: 'nodes',
                data: { id: fid, label: fn, isFileLabel: true },
                classes: 'file-label',
                position: { x: n.position().x, y: n.position().y + n.height() / 2 + 10 }
            });
        });
    }

    function collapseNode(parentId) {
        const children = expansionChildren.get(parentId);
        if (!children) return;
        children.edgeIds.forEach(eid => {
            const el = cy.getElementById(eid);
            if (el.length) el.remove();
        });
        children.nodeIds.forEach(cid => collapseNode(cid));
        children.nodeIds.forEach(cid => {
            const el = cy.getElementById(cid);
            // Remove companion file-label too
            const fl = cy.getElementById('_fl_' + cid);
            if (fl.length) fl.remove();
            if (el.length && el.connectedEdges().length === 0) {
                el.remove();
                visibleNodeIds.delete(cid);
            }
        });
        expansionChildren.delete(parentId);
        expandedNodeIds.delete(parentId);
    }

    function markDeadEndNodes() {
        if (!cy || !fullGraphData) return;
        cy.nodes().forEach(n => {
            const nid = n.id();
            const hasOutgoing = fullGraphData.edges.some(e => String(e.source_id) === nid);
            if (!hasOutgoing) n.data('isDeadEnd', true);
        });
    }

    // === 通用 Cytoscape 初始化 ===
    function initCytoscape(elements, containerId) {
        const container = document.getElementById(containerId);
        if (!container) return;

        if (typeof cytoscape === 'undefined') {
            container.innerHTML = '<div style="padding:40px;color:#d65d0e;text-align:center;"><h3>Cytoscape.js 未加载</h3><p style="margin-top:8px;color:#bdae93;font-size:13px;">本报告需要网络连接以加载 Cytoscape.js</p></div>';
            return;
        }

        const nodeCount = elements.filter(e => e.group === 'nodes').length;
        const isLarge = nodeCount > 1000;
        const notice = document.getElementById('degrade-notice');
        if (isLarge && notice) {
            notice.style.display = 'block';
            notice.textContent = '大图模式: ' + nodeCount + ' 个节点，已启用性能优化';
        }

        // Determine if this is left panel (call graph) or right panel
        const isCallGraph = containerId === 'cy-call';

        try {
            const instance = cytoscape({
                container: container,
                elements: elements,
                style: [
                    {
                        selector: 'node', style: {
                            label: 'data(label)', color: '#bdae93',
                            'font-size': isLarge ? '9px' : '11px',
                            'text-valign': 'center', 'text-halign': 'center',
                            shape: 'data(shape)', width: 'label', height: 'label',
                            'text-wrap': 'wrap', padding: '6px',
                            'border-width': 1, 'border-color': '#d65d0e',
                            'background-color': '#3c3836'
                        }
                    },
                    {
                        selector: 'edge', style: {
                            width: isLarge ? 1 : 1.5, 'line-color': '#504945',
                            'target-arrow-color': '#d65d0e',
                            'target-arrow-shape': 'triangle', 'curve-style': 'bezier'
                        }
                    },
                    {
                        selector: 'node[isDeadEnd]', style: {
                            'border-color': '#928374', 'border-width': 1, 'border-style': 'solid'
                        }
                    },
                    {
                        selector: 'node[isEntry]', style: {
                            color: '#fabd2f', 'font-weight': 'bold'
                        }
                    },
                    {
                        selector: 'node:selected', style: {
                            'border-width': 3, 'border-color': '#fe8019'
                        }
                    },
                    {
                        selector: 'node[fileType="header"]', style: {
                            'border-color': '#83a598', 'border-width': 2
                        }
                    },
                    {
                        selector: 'node[fileType="source"]', style: {
                            'border-color': '#b8bb26', 'border-width': 2
                        }
                    },
                    {
                        selector: 'node.file-label', style: {
                            'width': 0, 'height': 0, 'background-opacity': 0,
                            'border-width': 0, 'label': 'data(label)',
                            'color': '#928374', 'font-size': '10px',
                            'text-valign': 'bottom', 'text-halign': 'center',
                            'overlay-opacity': 0, 'events': 'no',
                            'text-margin-y': 2, 'min-zoomed-font-size': 4
                        }
                    }
                ],
                layout: {
                    name: 'cose',
                    padding: 20
                },
                wheelSensitivity: 0.15
            });

            if (isLarge) {
                instance.hideEdgesOnViewport = true;
                instance.motionBlur = true;
                instance.textEvents = 'no';
            }

            // Add filename labels below function nodes
            instance.one('layoutstop', function() {
                instance.nodes().forEach(function(n) {
                    const fn = n.data('fname');
                    if (!fn) return;
                    const fid = '_fl_' + n.id();
                    if (instance.getElementById(fid).length) return;
                    instance.add({
                        group: 'nodes',
                        data: { id: fid, label: fn, isFileLabel: true },
                        classes: 'file-label',
                        position: { x: n.position().x, y: n.position().y + n.height() / 2 + 10 }
                    });
                });
            });

            // Add file-label companions immediately for all nodes with fname
            try {
                instance.nodes().forEach(function(n_) {
                    var fn_ = n_.data('fname');
                    if (!fn_) return;
                    var fid_ = '_fl_' + n_.id();
                    if (instance.getElementById(fid_).length) return;
                    instance.add({
                        group: 'nodes', data: { id: fid_, label: fn_, isFileLabel: true },
                        classes: 'file-label',
                        position: { x: n_.position().x, y: n_.position().y + n_.height() / 2 + 10 }
                    });
                });
            } catch(e) { console.warn('file-label err', e); }

            // Only left panel (call graph) gets the full interactive treatment
            if (isCallGraph) {
                cy = instance;

                // Helper: find symbol by ID
                function symById(id) {
                    return (data.symbols || []).find(function(s) { return s.symbol_id === id; }) || {};
                }

                cy.on('tap', 'node', function(evt) {
                    const node = evt.target;
                    const d = node.data();
                    const nodeId = node.id();

                    // Non-leaf + not expanded → expand only, no info box
                    const hasOutgoing = isLazyMode && fullGraphData &&
                        fullGraphData.edges.some(e => String(e.source_id) === nodeId);
                    if (hasOutgoing && !expandedNodeIds.has(nodeId)) {
                        expandNode(nodeId);
                        return;
                    }

                    // Leaf or already-expanded → show detail info box
                    const container = document.getElementById('call-graph-area');
                    const rect = container.getBoundingClientRect();
                    const bb = node.renderedBoundingBox();
                    const info = document.getElementById('node-info');
                    const offsetX = bb.x2 - rect.left + 6;
                    const offsetY = bb.y2 - rect.top + 6;
                    info.style.left = Math.min(offsetX, Math.max(rect.width - 380, 0)) + 'px';
                    info.style.top = Math.min(offsetY, Math.max(rect.height - 260, 0)) + 'px';
                    info.style.display = 'block';
                    document.getElementById('edge-info').style.display = 'none';
                    document.getElementById('ni-name').textContent = d.label;
                    document.getElementById('ni-kind').textContent = d.kind;
                    document.getElementById('ni-file').textContent = (d.file || '').split('/').pop();
                    document.getElementById('ni-line').textContent = d.line;
                    document.getElementById('ni-fanin').textContent = d.fan_in;
                    document.getElementById('ni-fanout').textContent = d.fan_out;
                    document.getElementById('ni-cc').textContent = d.complexity;
                    document.getElementById('ni-expand-row').style.display = 'none';
                    const nc = document.getElementById('ni-comment');
                    if (d.comment) { nc.textContent = d.comment; nc.style.display = 'block'; }
                    else { nc.style.display = 'none'; }

                    const expandRow = document.getElementById('ni-expand-row');
                    const expandEl = document.getElementById('ni-expand');
                    if (isLazyMode && fullGraphData) {
                        if (expandedNodeIds.has(nodeId)) {
                            expandEl.textContent = '已展开';
                        } else {
                            const calleeCount = fullGraphData.edges.filter(e =>
                                String(e.source_id) === nodeId
                            ).length;
                            expandEl.textContent = calleeCount > 0 ? calleeCount + ' 个被调用函数' : '无调用关系';
                        }
                        expandRow.style.display = 'flex';
                    } else if (expandRow) {
                        expandRow.style.display = 'none';
                    }

                    // Show external/system call info for this node
                    var extCalls = (data.external_refs || []).filter(function(r) {
                        return r.caller_name === d.label.split('\n')[0];
                    });
                    if (extCalls.length > 0) {
                        var nc2 = document.getElementById('ni-comment');
                        nc2.textContent = '系统函数调用 (' + extCalls.length + '): ' + extCalls.map(function(r) { return r.callee_name; }).join(', ');
                        nc2.style.display = 'block';
                    }
                });

                // Edge click: show call parameters in edge-info panel
                cy.on('tap', 'edge', function(evt) {
                    const edge = evt.target;
                    const ed = edge.data();
                    const srcId = parseInt(ed.source);
                    const tgtId = parseInt(ed.target);
                    const caller = symById(srcId);
                    const callee = symById(tgtId);
                    const cName = caller.name || ('ID_' + srcId);
                    const tName = callee.name || ('ID_' + tgtId);

                    const ei = document.getElementById('edge-info');
                    const mp = edge.midpoint();
                    const container = document.getElementById('call-graph-area');
                    const rect = container.getBoundingClientRect();
                    const cyRect = document.getElementById('cy-call').getBoundingClientRect();
                    const zoomLvl = cy.zoom();
                    const pan = cy.pan();
                    const rpX = mp.x * zoomLvl + pan.x + cyRect.left;
                    const rpY = mp.y * zoomLvl + pan.y + cyRect.top;
                    ei.style.left = Math.min(rpX - rect.left + 8, Math.max(rect.width - 400, 0)) + 'px';
                    ei.style.top = Math.min(rpY - rect.top + 8, Math.max(rect.height - 200, 0)) + 'px';

                    document.getElementById('ei-header').textContent = cName + ' → ' + tName;

                    var body = '';
                    var params = callee.parameters || [];
                    if (callee.return_type) body += '返回: ' + callee.return_type + '\n';
                    if (params.length > 0) {
                        body += '参数 (' + params.length + '):';
                        params.forEach(function(p, i) {
                            body += '\n  ' + (i+1) + '. ' + p;
                        });
                    } else {
                        body += '参数: 无';
                    }
                    document.getElementById('ei-body').textContent = body;
                    ei.style.display = 'block';
                    document.getElementById('node-info').style.display = 'none';
                });

                cy.on('tap', function(evt) {
                    if (evt.target === cy) {
                        document.getElementById('node-info').style.display = 'none';
                        document.getElementById('edge-info').style.display = 'none';
                    }
                });

                cy.on('cxttap', 'node', function(evt) {
                    const nid = evt.target.id();
                    if (isLazyMode && expansionChildren.has(nid)) collapseNode(nid);
                });
            } else {
                // Right panel: basic click to show name
                cySide = instance;
                const sideInfo = document.getElementById('cy-side');
                cySide.on('tap', 'node', function(evt) {
                    const d = evt.target.data();
                    const info = document.getElementById('node-info');
                    const callArea = document.getElementById('call-graph-area');
                    const rect = callArea.getBoundingClientRect();
                    const bb = evt.target.renderedBoundingBox();
                    info.style.left = Math.min(bb.x2 - rect.left + 6, Math.max(rect.width - 380, 0)) + 'px';
                    info.style.top = Math.min(bb.y2 - rect.top + 6, Math.max(rect.height - 260, 0)) + 'px';
                    info.style.display = 'block';
                    document.getElementById('ni-name').textContent = d.label;
                    document.getElementById('ni-kind').textContent = d.kind;
                    document.getElementById('ni-file').textContent = (d.file || '').split('/').pop();
                    document.getElementById('ni-line').textContent = d.line;
                    document.getElementById('ni-fanin').textContent = d.fan_in || '-';
                    document.getElementById('ni-fanout').textContent = d.fan_out || '-';
                    document.getElementById('ni-cc').textContent = d.complexity || '-';
                    const nc = document.getElementById('ni-comment');
                    if (d.comment) { nc.textContent = d.comment; nc.style.display = 'block'; }
                    else { nc.style.display = 'none'; }
                    document.getElementById('ni-expand-row').style.display = 'none';
                });
            }

        } catch (e) {
            console.error('Cytoscape init failed:', e);
            if (isCallGraph) {
                container.innerHTML = '<div style="padding:20px;color:#d65d0e;">图形渲染初始化失败</div>';
            }
        }
    }

    // === 右侧卡片切换 ===
    window.switchCard = function(card) {
        currentCard = card;
        document.querySelectorAll('.card-tab').forEach(t => t.classList.remove('active'));
        document.querySelector(`.card-tab[data-card="${card}"]`).classList.add('active');

        // Hide all card bodies
        document.querySelectorAll('#card-body > div').forEach(d => d.style.display = 'none');

        switch (card) {
            case 'include':
            case 'type': {
                document.getElementById('cy-side').style.display = 'block';
                if (cySide) { cySide.destroy(); cySide = null; }
                const graphData = card === 'include' ? data.include_graph : data.type_graph;
                const elements = buildElements(graphData, data.symbols || [], data.stats || {});
                initCytoscape(elements, 'cy-side');
                break;
            }
            case 'stats':
                document.getElementById('stats-panel').style.display = 'block';
                renderStats();
                break;
            case 'symbols':
                document.getElementById('symbol-panel').style.display = 'block';
                break;
        }
    };

    window.resetLayout = function() {
        if (cy) cy.layout({ name: 'cose', padding: 20 }).run();
    };

    window.fitGraph = function() {
        if (cy) cy.fit();
    };

    window.filterSymbols = function(query) {
        const list = document.getElementById('symbol-list');
        list.querySelectorAll('li').forEach(li => {
            li.style.display = li.textContent.toLowerCase().includes(query.toLowerCase()) ? '' : 'none';
        });
    };

    function renderSidebar() {
        const list = document.getElementById('symbol-list');
        list.innerHTML = '';
        (data.symbols || []).forEach(s => {
            const li = document.createElement('li');
            li.textContent = s.name;
            li.title = s.qualified_name;
            li.onclick = () => {
                // Try left panel first, then right panel
                const target = cy ? cy.getElementById(String(s.symbol_id)) : null;
                if (target && target.length) {
                    cy.animate({ fit: { eles: target, padding: 60 }, duration: 400 });
                    target.select();
                }
            };
            list.appendChild(li);
        });
    }

    function renderStats() {
        const stats = data.stats || {};
        const meta = data.metadata || {};

        const summaryTable = document.getElementById('summary-table');
        summaryTable.innerHTML = `
            <tr><th>项目</th><td>${meta.project_name || '-'}</td></tr>
            <tr><th>文件数</th><td>${meta.file_count || 0}</td></tr>
            <tr><th>函数数</th><td>${meta.function_count || 0}</td></tr>
            <tr><th>C 编译器</th><td>${meta.c_compiler || '-'}</td></tr>
            <tr><th>C++ 编译器</th><td>${meta.cxx_compiler || '-'}</td></tr>
            <tr><th>生成时间</th><td>${meta.generated_at || '-'}</td></tr>
        `;

        const fileTable = document.getElementById('file-hotspot-table');
        fileTable.innerHTML = '<tr><th>文件</th><th>行数</th><th>热力</th></tr>';
        const maxLines = Math.max(...(stats.file_stats || []).map(f => f.code_lines || 0), 1);
        (stats.file_stats || []).sort((a, b) => b.code_lines - a.code_lines).slice(0, 20).forEach(f => {
            const heat = (f.code_lines || 0) / maxLines;
            const tr = document.createElement('tr');
            tr.innerHTML = `<td>${f.file_path.split('/').pop()}</td><td>${f.code_lines}</td>
                <td><span class="hotbar" style="width:${Math.max(heat*120,4)}px;background:${heatColor(heat)}"></span></td>`;
            fileTable.appendChild(tr);
        });

        const funcTable = document.getElementById('func-hotspot-table');
        funcTable.innerHTML = '<tr><th>函数</th><th>被调用</th><th>调用</th><th>圈复杂度</th></tr>';
        (stats.function_stats || []).slice(0, 20).forEach(f => {
            const sym = (data.symbols || []).find(s => s.symbol_id === f.function_id) || {};
            const tr = document.createElement('tr');
            tr.innerHTML = `<td>${sym.name || f.function_id}</td>
                <td>${f.fan_in}</td><td>${f.fan_out}</td><td>${f.cyclomatic_complexity}</td>`;
            funcTable.appendChild(tr);
        });

        const anomalyList = document.getElementById('anomaly-list');
        const anomalies = data.anomalies || {};
        if ((anomalies.circular_includes || []).length === 0) {
            anomalyList.innerHTML = '<p style="color:#b8bb26;font-size:13px;">未检测到循环包含</p>';
        } else {
            (anomalies.circular_includes || []).forEach(ci => {
                const div = document.createElement('div');
                div.className = 'anomaly';
                div.textContent = '循环包含: ' + ci.file_cycle.join(' → ');
                anomalyList.appendChild(div);
            });
        }
    }

    function updateMeta() {
        const meta = data.metadata || {};
        const mi = document.getElementById('meta-info');
        if (mi) mi.textContent = '项目: ' + (meta.project_name || '-') + ' | 生成时间: ' + (meta.generated_at || '-');
        const cl = document.getElementById('cmd-line');
        if (cl && meta.command_line) {
            cl.textContent = '运行命令: ' + meta.command_line;
            cl.style.display = 'block';
        }
    }

    // 主题切换（深色 Gruvbox / 浅色 Solarized）
    let isLight = false;
    function applyCyTheme(light) {
        const cyInsts = [];
        if (cy) cyInsts.push(cy);
        if (cySide) cyInsts.push(cySide);
        const dark = {
            bg: '#3c3836', edge: '#504945', text: '#bdae93',
            nodeBorder: '#d65d0e', selBorder: '#fe8019',
            entryText: '#fabd2f',
            deadBorder: '#928374', headBorder: '#83a598', srcBorder: '#b8bb26'
        };
        const light = {
            bg: '#eee8d5', edge: '#93a1a1', text: '#657b83',
            nodeBorder: '#cb4b16', selBorder: '#268bd2',
            entryText: '#b58900',
            deadBorder: '#586e75', headBorder: '#268bd2', srcBorder: '#859900'
        };
        const c = light ? light : dark;
        cyInsts.forEach(inst => {
            inst.style().selector('node').style({ 'background-color': c.bg, color: c.text, 'border-color': c.nodeBorder }).update();
            inst.style().selector('edge').style({ 'line-color': c.edge, 'target-arrow-color': c.nodeBorder }).update();
            inst.style().selector('node[isEntry]').style({ color: c.entryText }).update();
            inst.style().selector('node[isDeadEnd]').style({ 'border-color': c.deadBorder }).update();
            inst.style().selector('node[fileType="header"]').style({ 'border-color': c.headBorder }).update();
            inst.style().selector('node[fileType="source"]').style({ 'border-color': c.srcBorder }).update();
            inst.style().selector('node.file-label').style({ color: c.deadBorder }).update();
        });
    }
    window.toggleTheme = function toggleTheme() {
        isLight = !isLight;
        document.body.classList.toggle('light', isLight);
        document.getElementById('theme-btn').textContent = isLight ? '☾' : '☀';
        applyCyTheme(isLight);
    };

    // 可拖拽分割线
    function initSplitter() {
        const divider = document.getElementById('divider');
        const left = document.getElementById('call-graph-area');
        const right = document.getElementById('card-panel');
        if (!divider || !left || !right) return;

        divider.addEventListener('mousedown', function(e) {
            e.preventDefault();
            divider.classList.add('active');
            const startX = e.clientX;
            const leftW = left.getBoundingClientRect().width;
            const totalW = left.parentElement.getBoundingClientRect().width;

            function onMove(ev) {
                const pct = ((leftW + ev.clientX - startX) / totalW) * 100;
                if (pct < 20 || pct > 80) return;
                left.style.width = pct + '%';
                right.style.width = (100 - pct) + '%';
                if (cy) cy.resize();
                if (cySide) cySide.resize();
            }
            function onUp() {
                divider.classList.remove('active');
                document.removeEventListener('mousemove', onMove);
                document.removeEventListener('mouseup', onUp);
            }
            document.addEventListener('mousemove', onMove);
            document.addEventListener('mouseup', onUp);
        });
    }

    document.addEventListener('DOMContentLoaded', function() {
        updateMeta();
        renderSidebar();
        initCallGraphLazy();
        initSplitter();
        switchCard('include');
    });

})();
