// Reporter/Reporter.cpp - HTML 报告生成器实现
// 将分析数据序列化为 JSON，注入 HTML 模板，生成自包含 HTML 报告
// 对应设计文档 4.3.8 节

#include "Reporter/Reporter.h"
#include <nlohmann/json.hpp>
#include <inja/inja.hpp>
#include <spdlog/spdlog.h>
#include <stdexcept>
#include <sstream>
#include <ctime>
#include <algorithm>
#include <limits>

// 内嵌 Cytoscape.js 库（由 3rdparty/cytoscape_min_js.h 提供）
#include "cytoscape_min_js.h"

using json = nlohmann::json;

// 内嵌 HTML 模板（从 Template/template.html 内嵌为字符串常量）
static const char* HTML_TEMPLATE = R"HTML(
<!DOCTYPE html>
<html lang="zh-CN">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>codeviz - 源码可视化分析报告</title>
    <style>
        * { box-sizing: border-box; margin: 0; padding: 0; }
        body { font-family: 'Segoe UI', Arial, sans-serif; background: #1a1a2e; color: #eee; }
        #header { background: #16213e; padding: 16px 24px; display: flex; align-items: center; justify-content: space-between; border-bottom: 1px solid #0f3460; }
        #header h1 { font-size: 20px; color: #e94560; }
        #tabs { background: #16213e; padding: 0 24px; display: flex; border-bottom: 1px solid #0f3460; }
        .tab { padding: 12px 20px; cursor: pointer; color: #aaa; border-bottom: 2px solid transparent; font-size: 14px; }
        .tab.active { color: #e94560; border-bottom-color: #e94560; }
        #main { display: flex; height: calc(100vh - 102px); }
        #sidebar { width: 280px; background: #16213e; border-right: 1px solid #0f3460; overflow-y: auto; padding: 12px; flex-shrink: 0; }
        #sidebar h3 { font-size: 12px; color: #888; text-transform: uppercase; margin-bottom: 8px; }
        #search { width: 100%; background: #0f3460; border: 1px solid #e94560; color: #eee; padding: 6px 10px; border-radius: 4px; margin-bottom: 12px; font-size: 13px; }
        #symbol-list { list-style: none; }
        #symbol-list li { padding: 6px 8px; cursor: pointer; border-radius: 4px; font-size: 13px; color: #ccc; }
        #symbol-list li:hover { background: #0f3460; color: #e94560; }
        #canvas-container { flex: 1; position: relative; }
        #cy { width: 100%; height: 100%; }
        #stats-panel { display: none; padding: 20px; overflow-y: auto; flex: 1; }
        .stat-card { background: #16213e; border: 1px solid #0f3460; border-radius: 8px; padding: 16px; margin-bottom: 16px; }
        .stat-card h3 { font-size: 14px; color: #e94560; margin-bottom: 12px; }
        table { width: 100%; border-collapse: collapse; font-size: 13px; }
        th { text-align: left; padding: 8px; background: #0f3460; color: #aaa; }
        td { padding: 8px; border-bottom: 1px solid #0f3460; }
        .hotbar { height: 8px; border-radius: 4px; display: inline-block; min-width: 4px; }
        .anomaly { background: #e94560; color: #fff; padding: 4px 8px; border-radius: 4px; font-size: 12px; margin: 4px 0; }
        #node-info { position: absolute; bottom: 16px; right: 16px; background: #16213e; border: 1px solid #0f3460; border-radius: 8px; padding: 12px; max-width: 300px; display: none; font-size: 13px; z-index: 10; }
        #node-info h4 { color: #e94560; margin-bottom: 8px; }
        #node-info .kv { display: flex; justify-content: space-between; margin-bottom: 4px; }
        #node-info .kv .k { color: #aaa; }
        #controls { position: absolute; top: 16px; right: 16px; display: flex; gap: 8px; z-index: 10; }
        .btn { background: #0f3460; color: #eee; border: 1px solid #e94560; padding: 6px 14px; border-radius: 4px; cursor: pointer; font-size: 12px; }
        .btn:hover { background: #e94560; }
    </style>
</head>
<body>
    <div id="header">
        <h1>codeviz &nbsp;<span style="color:#aaa;font-size:14px;">源码可视化分析报告</span></h1>
        <div class="meta" id="meta-info">加载中...</div>
    </div>
    <div id="tabs">
        <div class="tab active" onclick="switchTab('call')">调用图</div>
        <div class="tab" onclick="switchTab('include')">包含图</div>
        <div class="tab" onclick="switchTab('type')">类型图</div>
        <div class="tab" onclick="switchTab('stats')">统计分析</div>
    </div>
    <div id="main">
        <div id="sidebar">
            <h3>符号搜索</h3>
            <input id="search" type="text" placeholder="搜索函数/类/文件..." oninput="filterSymbols(this.value)">
            <ul id="symbol-list"></ul>
        </div>
        <div id="canvas-container">
            <div id="controls">
                <button class="btn" onclick="resetLayout()">重置布局</button>
                <button class="btn" onclick="fitGraph()">适应窗口</button>
            </div>
            <div id="degrade-notice" style="display:none;position:absolute;top:56px;right:16px;background:#e94560;color:#fff;padding:6px 14px;border-radius:4px;font-size:12px;z-index:10;"></div>
            <div id="cy"></div>
            <div id="node-info">
                <h4 id="ni-name">-</h4>
                <div class="kv"><span class="k">类型</span><span id="ni-kind">-</span></div>
                <div class="kv"><span class="k">文件</span><span id="ni-file">-</span></div>
                <div class="kv"><span class="k">行号</span><span id="ni-line">-</span></div>
                <div class="kv"><span class="k">扇入</span><span id="ni-fanin">-</span></div>
                <div class="kv"><span class="k">扇出</span><span id="ni-fanout">-</span></div>
                <div class="kv"><span class="k">圈复杂度</span><span id="ni-cc">-</span></div>
            </div>
        </div>
        <div id="stats-panel"></div>
    </div>
    <script>
    // Cytoscape.js 内嵌（由 Reporter 填充真实库内容）
    {{ cytoscape_js }}
    </script>
    <script>
    window.CODEVIZ_DATA = {{ data_json }};
    </script>
    <script>
    {{ bridge_js }}
    </script>
</body>
</html>
)HTML";

// 内嵌桥接 JS 脚本（从 Template/cytoscape_bridge.js 内嵌）
static const char* BRIDGE_JS = R"BRIDGE(
(function(){
'use strict';
var data=window.CODEVIZ_DATA;
var cy=null;
var WEBGL_THRESHOLD=1000;
function heatColor(v){var r=Math.round(v*220+35),g=Math.round((1-v)*150+30),b=Math.round((1-v)*180+30);return'rgb('+r+','+g+','+b+')';}
function nodeShape(k){switch(k){case'FUNCTION':return'ellipse';case'STRUCT':case'CLASS':return'rectangle';case'FILE_ENTITY':return'diamond';default:return'ellipse';}}
function buildElements(graphData,symbols,stats){
var elements=[];var symbolMap={};symbols.forEach(function(s){symbolMap[s.symbol_id]=s;});
var statsMap={};if(stats&&stats.function_stats){stats.function_stats.forEach(function(f){statsMap[f.function_id]=f;});}
var nodeSet=new Set();var maxFanIn=1;
if(stats&&stats.function_stats){stats.function_stats.forEach(function(f){if(f.fan_in>maxFanIn)maxFanIn=f.fan_in;});}
(graphData.nodes||[]).forEach(function(n){
if(nodeSet.has(n.id))return;nodeSet.add(n.id);
var sym=symbolMap[n.id]||{};var fstat=statsMap[n.id]||{};
var heatVal=(fstat.fan_in||0)/Math.max(maxFanIn,1);
elements.push({group:'nodes',data:{id:String(n.id),label:n.label||sym.name||String(n.id),kind:sym.kind||n.type||'FUNCTION',file:sym.file_path||'',line:sym.line||0,fan_in:fstat.fan_in||0,fan_out:fstat.fan_out||0,complexity:fstat.cyclomatic_complexity||0,heat:heatVal}});
});
(graphData.edges||[]).forEach(function(e,idx){elements.push({group:'edges',data:{id:'e'+idx,source:String(e.source_id),target:String(e.target_id),relation:e.relation||'CALLS',weight:e.weight||1}});});
return elements;}
function initCytoscape(elements){
var container=document.getElementById('cy');if(!container)return;
if(typeof cytoscape==='undefined'){container.innerHTML='<div style="padding:40px;color:#e94560;text-align:center;"><h3>Cytoscape.js 未加载</h3><p style="margin-top:8px;color:#aaa;font-size:13px;">本报告需要网络连接以加载 Cytoscape.js，或替换为离线版本</p></div>';return;}
var nodeCount=elements.filter(function(e){return e.group==='nodes';}).length;
var isLarge=nodeCount>1000;var notice=document.getElementById('degrade-notice');
if(isLarge&&notice){notice.style.display='block';notice.textContent='大图模式: '+nodeCount+' 个节点，已启用性能优化';}
try{var opts={container:container,elements:elements,
style:[{selector:'node',style:{label:'data(label)',color:'#fff','font-size':isLarge?'9px':'11px','text-valign':'center','text-halign':'center',width:isLarge?'40px':'60px',height:isLarge?'20px':'30px','border-width':1,'border-color':'#e94560','background-color':'#16213e'}},
{selector:'edge',style:{width:isLarge?1:1.5,'line-color':'#0f3460','target-arrow-color':'#e94560','target-arrow-shape':'triangle','curve-style':'bezier'}},
{selector:'node:selected',style:{'border-width':3,'border-color':'#e94560'}}],
layout:{name:'cose',padding:20}};
if(isLarge){opts.hideEdgesOnViewport=true;opts.motionBlur=true;opts.textEvents='no';opts.wheelSensitivity=0.5;}
cy=cytoscape(opts);
cy.on('tap','node',function(evt){var d=evt.target.data();document.getElementById('node-info').style.display='block';document.getElementById('ni-name').textContent=d.label;document.getElementById('ni-kind').textContent=d.kind;document.getElementById('ni-file').textContent=(d.file||'').split('/').pop();document.getElementById('ni-line').textContent=d.line;document.getElementById('ni-fanin').textContent=d.fan_in;document.getElementById('ni-fanout').textContent=d.fan_out;document.getElementById('ni-cc').textContent=d.complexity;});
cy.on('tap',function(evt){if(evt.target===cy)document.getElementById('node-info').style.display='none';});
}catch(e){console.error('Cytoscape init failed:',e);}}
window.switchTab=function(tab){
document.querySelectorAll('.tab').forEach(function(t){t.classList.remove('active');});event.target.classList.add('active');
var sp=document.getElementById('stats-panel');var cc=document.getElementById('canvas-container');var sb=document.getElementById('sidebar');
if(tab==='stats'){sp.style.display='block';cc.style.display='none';sb.style.display='none';renderStats();return;}
sp.style.display='none';cc.style.display='block';sb.style.display='block';
var graphData=tab==='call'?data.call_graph:tab==='include'?data.include_graph:data.type_graph;
if(cy){cy.destroy();cy=null;}
initCytoscape(buildElements(graphData||{nodes:[],edges:[]},data.symbols||[],data.stats||{}));};
window.resetLayout=function(){if(cy)cy.layout({name:'cose',padding:20}).run();};
window.fitGraph=function(){if(cy)cy.fit();};
window.filterSymbols=function(q){document.getElementById('symbol-list').querySelectorAll('li').forEach(function(li){li.style.display=li.textContent.toLowerCase().includes(q.toLowerCase())?'':'none';});};
function renderSidebar(){var list=document.getElementById('symbol-list');list.innerHTML='';(data.symbols||[]).forEach(function(s){var li=document.createElement('li');li.textContent=s.name;li.title=s.qualified_name;li.onclick=function(){if(cy){var node=cy.getElementById(String(s.symbol_id));if(node.length){cy.animate({fit:{eles:node,padding:60},duration:400});node.select();}}};list.appendChild(li);});}
function renderStats(){
var stats=data.stats||{};var meta=data.metadata||{};
var sp=document.getElementById('stats-panel');
sp.innerHTML='<div class="stat-card"><h3>项目概览</h3><table id="sum-tbl"></table></div><div class="stat-card"><h3>文件热力图</h3><table id="file-tbl"><tr><th>文件</th><th>行数</th><th>热力</th></tr></table></div><div class="stat-card"><h3>函数热力图</h3><table id="func-tbl"><tr><th>函数</th><th>扇入</th><th>扇出</th><th>圈复杂度</th></tr></table></div><div class="stat-card"><h3>异常检测</h3><div id="ano-list"></div></div>';
var st=document.getElementById('sum-tbl');
st.innerHTML='<tr><th>项目</th><td>'+( meta.project_name||'-')+'</td></tr><tr><th>文件数</th><td>'+(meta.file_count||0)+'</td></tr><tr><th>函数数</th><td>'+(meta.function_count||0)+'</td></tr><tr><th>C编译器</th><td>'+(meta.c_compiler||'-')+'</td></tr><tr><th>C++编译器</th><td>'+(meta.cxx_compiler||'-')+'</td></tr>';
var ml=Math.max.apply(null,(stats.file_stats||[]).map(function(f){return f.code_lines||0;}).concat([1]));
var ft=document.getElementById('file-tbl');
(stats.file_stats||[]).sort(function(a,b){return b.code_lines-a.code_lines;}).slice(0,20).forEach(function(f){var h=(f.code_lines||0)/ml;var tr=document.createElement('tr');tr.innerHTML='<td>'+f.file_path.split('/').pop()+'</td><td>'+f.code_lines+'</td><td><span class="hotbar" style="width:'+Math.max(h*120,4)+'px;background:'+heatColor(h)+'"></span></td>';ft.appendChild(tr);});
var symMap={};(data.symbols||[]).forEach(function(s){symMap[s.symbol_id]=s;});
var fnT=document.getElementById('func-tbl');
(stats.function_stats||[]).slice(0,20).forEach(function(f){var sym=symMap[f.function_id]||{};var tr=document.createElement('tr');tr.innerHTML='<td>'+(sym.name||f.function_id)+'</td><td>'+f.fan_in+'</td><td>'+f.fan_out+'</td><td>'+f.cyclomatic_complexity+'</td>';fnT.appendChild(tr);});
var anoList=document.getElementById('ano-list');var ano=data.anomalies||{};
if(!(ano.circular_includes||[]).length){anoList.innerHTML='<p style="color:#4ade80;font-size:13px;">未检测到循环包含</p>';}
else{(ano.circular_includes||[]).forEach(function(ci){var d=document.createElement('div');d.className='anomaly';d.textContent='循环包含: '+ci.file_cycle.join(' → ');anoList.appendChild(d);});}
}
function updateMeta(){var meta=data.metadata||{};document.getElementById('meta-info').textContent='项目: '+(meta.project_name||'-')+' | 生成时间: '+(meta.generated_at||'-');}
document.addEventListener('DOMContentLoaded',function(){updateMeta();renderSidebar();initCytoscape(buildElements(data.call_graph||{nodes:[],edges:[]},data.symbols||[],data.stats||{}));});
})();
)BRIDGE";

HTMLReport Reporter::generate(const std::vector<SymbolMetadata>& symbols,
                               const AnalysisStats& stats,
                               const AnalysisContext& ctx) {
    spdlog::info("生成 HTML 报告: {} 个符号", symbols.size());

    // 1. 加载模板
    std::string tmpl = load_template();
    if (tmpl.empty()) {
        throw std::runtime_error("HTML 模板字符串为空");
    }

    // 2. 构建 JSON 数据
    json data = build_json(symbols, stats, ctx);
    std::string json_str = data.dump(2);

    // 3. 准备 Cytoscape.js 代码
    std::string cytoscape_js_code(
        reinterpret_cast<const char*>(cytoscape_min_js),
        cytoscape_min_js_len);

    // 4. 使用 Inja 模板引擎渲染
    inja::Environment env;
    std::string result = env.render(tmpl, {
        {"cytoscape_js", cytoscape_js_code},
        {"data_json", json_str},
        {"bridge_js", BRIDGE_JS}
    });

    spdlog::info("HTML 报告生成完成，大小: {} 字节", result.size());

    HTMLReport report;
    report.content = std::move(result);
    report.output_path = "report.html";
    return report;
}

std::string Reporter::load_template() {
    return std::string(HTML_TEMPLATE);
}

json Reporter::build_json(const std::vector<SymbolMetadata>& symbols,
                           const AnalysisStats& stats,
                           const AnalysisContext& ctx) {
    json root;

    // 生成时间
    std::time_t now = std::time(nullptr);
    char time_buf[64];
    std::strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", std::localtime(&now));

    // 统计函数和文件数量
    size_t func_count = std::count_if(symbols.begin(), symbols.end(),
                                       [](const SymbolMetadata& s) {
                                           return s.kind == SymbolKind::FUNCTION;
                                       });

    // 元数据
    root["metadata"] = {
        {"project_name", ctx.project_root.empty() ? "unknown" : ctx.project_root},
        {"file_count", ctx.files.size()},
        {"function_count", func_count},
        {"c_compiler", ctx.c_compiler},
        {"cxx_compiler", ctx.cxx_compiler},
        {"generated_at", time_buf}
    };

    // 符号列表
    json sym_array = json::array();
    for (const auto& s : symbols) {
        json obj;
        obj["symbol_id"] = s.symbol_id;
        obj["name"] = s.name;
        obj["qualified_name"] = s.qualified_name;
        obj["file_path"] = s.file_path;
        obj["line"] = s.line;
        obj["kind"] = [&s]() -> std::string {
            switch (s.kind) {
                case SymbolKind::FUNCTION: return "FUNCTION";
                case SymbolKind::STRUCT:   return "STRUCT";
                case SymbolKind::CLASS:    return "CLASS";
                case SymbolKind::ENUM:     return "ENUM";
                case SymbolKind::MACRO:    return "MACRO";
                case SymbolKind::FILE_ENTITY: return "FILE_ENTITY";
                default: return "UNKNOWN";
            }
        }();
        obj["fan_in"] = s.fan_in;
        obj["fan_out"] = s.fan_out;
        obj["complexity"] = s.complexity;

        // 补充函数签名信息（从 ctx.functions 查找）
        auto fit = std::find_if(ctx.functions.begin(), ctx.functions.end(),
                                [id = s.symbol_id](const FunctionSymbol& f) {
                                    return f.symbol_id == id;
                                });
        if (fit != ctx.functions.end()) {
            obj["return_type"] = fit->return_type;
            obj["is_virtual"] = fit->is_virtual;
            obj["is_static"] = fit->is_static;
            obj["is_inline"] = fit->is_inline;
            json params = json::array();
            for (const auto& p : fit->parameters) params.push_back(p);
            obj["parameters"] = params;
        }
        sym_array.push_back(obj);
    }
    root["symbols"] = sym_array;

    // 复合类型数据（含字段信息）
    json comp_array = json::array();
    for (const auto& csym : ctx.composites) {
        json cobj;
        cobj["symbol_id"] = csym.symbol_id;
        json fields = json::array();
        for (const auto& f : csym.fields) {
            fields.push_back({
                {"name", f.name},
                {"type", f.type},
                {"access", static_cast<int>(f.access)}
            });
        }
        cobj["fields"] = fields;
        comp_array.push_back(cobj);
    }
    root["composites"] = comp_array;

    // 调用图
    root["call_graph"] = convert_call_graph(ctx.call_edges, ctx.symbols);

    // 包含图
    root["include_graph"] = convert_include_graph(ctx.include_edges, ctx.files, ctx.symbols);

    // 类型图
    root["type_graph"] = convert_type_graph(ctx.type_edges, ctx.composites, ctx.symbols);

    // 热力图
    root["hotspots"] = build_hotspots(stats);

    // 异常检测
    root["anomalies"] = build_anomalies(stats);

    // 外部符号引用
    json ext_array = json::array();
    for (const auto& ext : ctx.external_refs) {
        ext_array.push_back({
            {"caller_name", ext.caller_name},
            {"callee_name", ext.callee_name},
            {"library", ext.library}
        });
    }
    root["external_refs"] = ext_array;

    // 统计数据（直接给前端使用）
    json stats_json;
    json file_stats = json::array();
    for (const auto& fs : stats.file_stats) {
        file_stats.push_back({
            {"file_path", fs.file_path},
            {"total_lines", fs.total_lines},
            {"code_lines", fs.code_lines},
            {"complexity_sum", fs.complexity_sum}
        });
    }
    stats_json["file_stats"] = file_stats;

    json func_stats = json::array();
    for (const auto& fs : stats.function_stats) {
        func_stats.push_back({
            {"function_id", fs.function_id},
            {"fan_in", fs.fan_in},
            {"fan_out", fs.fan_out},
            {"cyclomatic_complexity", fs.cyclomatic_complexity}
        });
    }
    stats_json["function_stats"] = func_stats;

    root["stats"] = stats_json;

    return root;
}

json Reporter::convert_call_graph(const std::vector<CallEdge>& edges,
                                   const std::vector<Symbol>& symbols) {
    json graph;
    json nodes = json::array();
    json edge_arr = json::array();

    // 收集出现在边中的节点 ID
    std::unordered_map<uint32_t, bool> node_set;
    for (const auto& e : edges) {
        if (e.callee_id != std::numeric_limits<uint32_t>::max()) {
            node_set[e.caller_id] = true;
            node_set[e.callee_id] = true;
        }
    }

    // 生成节点
    for (const auto& [id, _] : node_set) {
        json node;
        node["id"] = id;
        node["label"] = find_symbol_name(id, symbols);
        node["type"] = "FUNCTION";
        nodes.push_back(node);
    }

    // 生成边
    for (const auto& e : edges) {
        if (e.callee_id == std::numeric_limits<uint32_t>::max()) continue;
        json edge;
        edge["source_id"] = e.caller_id;
        edge["target_id"] = e.callee_id;
        edge["relation"] = "CALLS";
        edge["weight"] = e.call_count;
        edge_arr.push_back(edge);
    }

    graph["nodes"] = nodes;
    graph["edges"] = edge_arr;
    return graph;
}

json Reporter::convert_include_graph(const std::vector<IncludeEdge>& edges,
                                      const std::vector<FileSymbol>& files,
                                      const std::vector<Symbol>& symbols) {
    json graph;
    json nodes = json::array();
    json edge_arr = json::array();

    std::unordered_map<uint32_t, bool> node_set;
    for (const auto& e : edges) {
        node_set[e.includer_id] = true;
        node_set[e.includee_id] = true;
    }

    for (const auto& [id, _] : node_set) {
        json node;
        node["id"] = id;
        // 包含图节点只显示文件名，不显示完整路径
        std::string full_name = find_symbol_name(id, symbols);
        size_t pos = full_name.rfind('/');
        node["label"] = (pos != std::string::npos) ? full_name.substr(pos + 1) : full_name;
        node["type"] = "FILE_ENTITY";
        nodes.push_back(node);
    }

    for (const auto& e : edges) {
        json edge;
        edge["source_id"] = e.includer_id;
        edge["target_id"] = e.includee_id;
        edge["relation"] = "INCLUDES";
        edge["weight"] = 1;
        edge_arr.push_back(edge);
    }

    graph["nodes"] = nodes;
    graph["edges"] = edge_arr;
    return graph;
}

json Reporter::convert_type_graph(const std::vector<TypeDependencyEdge>& edges,
                                   const std::vector<CompositeSymbol>& composites,
                                   const std::vector<Symbol>& symbols) {
    json graph;
    json nodes = json::array();
    json edge_arr = json::array();

    std::unordered_map<uint32_t, bool> node_set;
    for (const auto& e : edges) {
        node_set[e.source_id] = true;
        node_set[e.target_id] = true;
    }

    for (const auto& [id, _] : node_set) {
        json node;
        node["id"] = id;
        node["label"] = find_symbol_name(id, symbols);
        node["type"] = "STRUCT";
        nodes.push_back(node);
    }

    for (const auto& e : edges) {
        json edge;
        edge["source_id"] = e.source_id;
        edge["target_id"] = e.target_id;
        edge["relation"] = (e.relation == TypeDependencyEdge::INHERITS) ? "INHERITS" : "CONTAINS";
        edge["weight"] = 1;
        edge_arr.push_back(edge);
    }

    graph["nodes"] = nodes;
    graph["edges"] = edge_arr;
    return graph;
}

json Reporter::build_hotspots(const AnalysisStats& stats) {
    json hotspots;

    // 文件热力
    json file_hot = json::array();
    double max_lines = 1.0;
    for (const auto& f : stats.file_stats) {
        max_lines = std::max(max_lines, static_cast<double>(f.code_lines));
    }
    for (const auto& f : stats.file_stats) {
        file_hot.push_back({
            {"file_path", f.file_path},
            {"code_lines", f.code_lines},
            {"heat", f.code_lines / max_lines}
        });
    }

    // 函数热力
    json func_hot = json::array();
    int max_fan_in = 1;
    for (const auto& f : stats.function_stats) {
        max_fan_in = std::max(max_fan_in, f.fan_in);
    }
    for (const auto& f : stats.function_stats) {
        func_hot.push_back({
            {"function_id", f.function_id},
            {"fan_in", f.fan_in},
            {"heat", static_cast<double>(f.fan_in) / max_fan_in}
        });
    }

    hotspots["files"] = file_hot;
    hotspots["functions"] = func_hot;
    return hotspots;
}

json Reporter::build_anomalies(const AnalysisStats& stats) {
    json anomalies;

    json circular = json::array();
    for (const auto& ci : stats.circular_includes) {
        json entry;
        entry["file_cycle"] = ci.file_cycle;
        circular.push_back(entry);
    }
    anomalies["circular_includes"] = circular;

    return anomalies;
}

std::string Reporter::find_symbol_name(uint32_t id, const std::vector<Symbol>& symbols) {
    for (const auto& s : symbols) {
        if (s.id == id) return s.name;
    }
    return "sym_" + std::to_string(id);
}
