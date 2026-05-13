// Reporter/Reporter.cpp - HTML 报告生成器实现
// 将分析数据序列化为 JSON，注入 HTML 模板，生成自包含 HTML 报告
// 对应设计文档 4.3.8 节

#include "Reporter/Reporter.h"
#include <nlohmann/json.hpp>
#include <inja/inja.hpp>
#include <spdlog/spdlog.h>
#include <map>
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
    <title>codeviz</title>
    <style>
        * { box-sizing: border-box; margin: 0; padding: 0; }
        body { font-family: 'Segoe UI', Arial, sans-serif; background: #282828; color: #ebdbb2; overflow: hidden; height: 100vh; }
        #header { background: #3c3836; padding: 8px 16px; display: flex; align-items: center; border-bottom: 1px solid #504945; flex-shrink: 0; gap: 12px; }
        #header .left { display: flex; flex-direction: column; gap: 1px; min-width: 0; }
        #header .left h1 { font-size: 16px; color: #d65d0e; white-space: nowrap; }
        #header .left .meta { font-size: 11px; color: #bdae93; white-space: nowrap; overflow: hidden; text-overflow: ellipsis; }
        #header .right { margin-left: auto; display: flex; gap: 6px; flex-shrink: 0; }
        #main { display: flex; height: calc(100vh - 60px); }
        #call-graph-area { width: 70%; position: relative; min-width: 0; }
        #cy-call { width: 100%; height: 100%; }
        #node-info { position: absolute; background: #3c3836; border: 1px solid #fe8019; border-radius: 8px; padding: 12px; max-width: 360px; display: none; font-size: 13px; z-index: 10; }
        #node-info h4 { color: #d65d0e; margin-bottom: 8px; }
        #node-info .kv { display: flex; justify-content: space-between; margin-bottom: 4px; }
        #node-info .kv .k { color: #bdae93; }
        
        .btn { background: #504945; color: #ebdbb2; border: 1px solid #d65d0e; padding: 4px 10px; border-radius: 4px; cursor: pointer; font-size: 20px; white-space: nowrap; line-height: 1; }
        .btn:hover { background: #d65d0e; }
        #divider { width: 4px; cursor: col-resize; background: #504945; flex-shrink: 0; }
        #divider:hover, #divider.active { background: #d65d0e; }
        #card-panel { flex: 1; display: flex; flex-direction: column; min-width: 0; }
        #card-tabs { display: flex; background: #3c3836; border-bottom: 1px solid #504945; flex-shrink: 0; }
        .card-tab { padding: 10px 16px; cursor: pointer; color: #bdae93; border-bottom: 2px solid transparent; font-size: 13px; user-select: none; }
        .card-tab:hover { color: #d65d0e; }
        .card-tab.active { color: #d65d0e; border-bottom-color: #d65d0e; }
        #card-body { flex: 1; position: relative; overflow: hidden; }
        #card-body > div { width: 100%; height: 100%; display: none; }
        #cy-side { width: 100%; height: 100%; }
        #stats-panel { overflow-y: auto; padding: 16px; }
        #symbol-panel { overflow-y: auto; padding: 12px; }
        .stat-card { background: #3c3836; border: 1px solid #504945; border-radius: 8px; padding: 16px; margin-bottom: 16px; }
        .stat-card h3 { font-size: 14px; color: #d65d0e; margin-bottom: 12px; }
        table { width: 100%; border-collapse: collapse; font-size: 13px; }
        th { text-align: left; padding: 8px; background: #504945; color: #bdae93; }
        td { padding: 8px; border-bottom: 1px solid #504945; }
        .hotbar { height: 8px; border-radius: 4px; display: inline-block; min-width: 4px; }
        .anomaly { background: #d65d0e; color: #ebdbb2; padding: 4px 8px; border-radius: 4px; font-size: 12px; margin: 4px 0; }
        #search { width: 100%; background: #504945; border: 1px solid #d65d0e; color: #ebdbb2; padding: 6px 10px; border-radius: 4px; margin-bottom: 12px; font-size: 13px; }
        #symbol-list { list-style: none; max-height: calc(50vh); overflow-y: auto; }
        #symbol-list li { padding: 6px 8px; cursor: pointer; border-radius: 4px; font-size: 13px; color: #d5c4a1; }
        #symbol-list li:hover { background: #504945; color: #d65d0e; }

        /* Solarized Light */
        body.light { background: #fdf6e3; color: #657b83; }
        body.light #header { background: #eee8d5; border-color: #93a1a1; }
        body.light #header .left h1 { color: #586e75; }
        body.light #header .left .meta { color: #839496; }
        body.light #header .left h1 span { color: #839496 !important; }
        body.light .btn { background: #eee8d5; color: #586e75; border-color: #93a1a1; font-size: 20px; line-height: 1; }
        body.light .btn:hover { background: #cb4b16; color: #fdf6e3; border-color: #cb4b16; }
        body.light #divider { background: #93a1a1; }
        body.light #divider:hover, body.light #divider.active { background: #cb4b16; }
        body.light #node-info { background: #eee8d5; border-color: #268bd2; }
        body.light #node-info h4 { color: #586e75; }
        body.light #node-info .kv .k { color: #839496; }
        body.light #ni-comment { background: #93a1a1 !important; color: #fdf6e3 !important; }
        body.light #card-tabs { background: #eee8d5; border-color: #93a1a1; }
        body.light .card-tab { color: #839496; }
        body.light .card-tab:hover { color: #cb4b16; }
        body.light .card-tab.active { color: #cb4b16; border-color: #cb4b16; }
        body.light .stat-card { background: #eee8d5; border-color: #93a1a1; }
        body.light .stat-card h3 { color: #586e75; }
        body.light table th { background: #93a1a1; color: #fdf6e3; }
        body.light table td { border-color: #93a1a1; }
        body.light .anomaly { background: #cb4b16; color: #fdf6e3; }
        body.light #search { background: #eee8d5; border-color: #cb4b16; color: #586e75; }
        body.light #symbol-list li { color: #657b83; }
        body.light #symbol-list li:hover { background: #93a1a1; color: #fdf6e3; }
        body.light #degrade-notice { background: #cb4b16 !important; color: #fdf6e3 !important; }
        body.light #edge-info { background: #eee8d5; border-color: #268bd2; }
        body.light #edge-info #ei-header { color: #268bd2; }
        body.light #edge-info #ei-body { color: #586e75; }
    </style>
</head>
<body>
    <div id="header">
        <div class="left">
            <h1>codeviz</h1>
            <div class="meta" id="meta-info">加载中...</div>
        </div>
        <div class="right">
            <button class="btn" onclick="resetLayout()" title="重置布局">⟳</button>
            <button class="btn" onclick="fitGraph()" title="适应窗口">⛶</button>
            <button class="btn" onclick="toggleTheme()" id="theme-btn" title="切换浅色/深色">☀</button>
        </div>
    </div>
    <div id="main">
        <div id="call-graph-area">
            <div id="degrade-notice" style="display:none;position:absolute;top:12px;left:50%;transform:translateX(-50%);background:#d65d0e;color:#fff;padding:4px 12px;border-radius:4px;font-size:11px;z-index:10;"></div>
            <div id="cy-call"></div>
            <div id="node-info">
                <h4 id="ni-name">-</h4>
                <div class="kv"><span class="k">类型</span><span id="ni-kind">-</span></div>
                <div class="kv"><span class="k">文件</span><span id="ni-file">-</span></div>
                <div class="kv"><span class="k">行号</span><span id="ni-line">-</span></div>
                <div class="kv"><span class="k">被调用</span><span id="ni-fanin">-</span></div>
                <div class="kv"><span class="k">调用</span><span id="ni-fanout">-</span></div>
                <div class="kv"><span class="k">圈复杂度</span><span id="ni-cc">-</span></div>
                <div class="kv" id="ni-expand-row" style="display:none;"><span class="k">可展开</span><span id="ni-expand">-</span></div>
                <div id="ni-comment" style="margin-top:8px;padding:6px;background:#504945;border-radius:4px;font-size:11px;color:#ccc;display:none;white-space:pre-wrap;max-height:120px;overflow-y:auto;"></div>
            </div>
            <div id="edge-info" style="display:none;position:absolute;background:#504945;border:1px solid #83a598;border-radius:8px;padding:10px;max-width:400px;font-size:12px;z-index:10;">
                <div id="ei-header" style="color:#83a598;font-weight:bold;margin-bottom:6px;"></div>
                <div id="ei-body" style="color:#ebdbb2;white-space:pre-wrap;font-size:11px;line-height:1.5;"></div>
            </div>
        </div>
        <div id="divider"></div>
        <div id="card-panel">
            <div id="card-tabs">
                <div class="card-tab active" data-card="include" onclick="switchCard('include')">包含图</div>
                <div class="card-tab" data-card="type" onclick="switchCard('type')">类型图</div>
                <div class="card-tab" data-card="stats" onclick="switchCard('stats')">统计分析</div>
                <div class="card-tab" data-card="symbols" onclick="switchCard('symbols')">符号查询</div>
            </div>
            <div id="card-body">
                <div id="cy-side"></div>
                <div id="stats-panel">
                    <div class="stat-card" id="summary-card">
                        <h3>项目概览</h3>
                        <table id="sum-tbl"></table>
                    </div>
                    <div class="stat-card">
                        <h3>文件热力图</h3>
                        <table id="file-tbl"><tr><th>文件</th><th>行数</th><th>热力</th></tr></table>
                    </div>
                    <div class="stat-card">
                        <h3>函数热力图</h3>
                        <table id="func-tbl"><tr><th>函数</th><th>被调用</th><th>调用</th><th>圈复杂度</th></tr></table>
                    </div>
                    <div class="stat-card" id="anomaly-card">
                        <h3>异常检测</h3>
                        <div id="ano-list"></div>
                    </div>
                </div>
                <div id="symbol-panel">
                    <input id="search" type="text" placeholder="搜索函数/类/文件..." oninput="filterSymbols(this.value)">
                    <ul id="symbol-list"></ul>
                </div>
            </div>
        </div>
    </div>
    <script>
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
var data=window.CODEVIZ_DATA,cy=null,cySide=null,currentCard='include',WEBGL_THRESHOLD=1000;
var isLazyMode=false,fullGraphData=null,visibleNodeIds=new Set(),expandedNodeIds=new Set(),expansionChildren=new Map();
function heatColor(v){var r=Math.round(v*220+35),g=Math.round((1-v)*150+30),b=Math.round((1-v)*180+30);return'rgb('+r+','+g+','+b+')';}
function nodeShape(k){return'round-rectangle';}
function buildElements(graphData,symbols,stats){
var elements=[],symbolMap={};symbols.forEach(function(s){symbolMap[s.symbol_id]=s;});
var statsMap={};if(stats&&stats.function_stats){stats.function_stats.forEach(function(f){statsMap[f.function_id]=f;});}
var nodeSet=new Set();(graphData.nodes||[]).forEach(function(n){
if(nodeSet.has(n.id))return;nodeSet.add(n.id);
var sym=symbolMap[n.id]||{},fstat=statsMap[n.id]||{},maxFanIn=stats&&stats.maxFanIn||1,heatVal=(fstat.fan_in||0)/Math.max(maxFanIn,1);
var kind=sym.kind||n.type||'FUNCTION',label=n.label||sym.name||String(n.id),fileType='',fname='';
if(kind==='FILE_ENTITY'){var fp=(sym.file_path||n.label||'').toLowerCase();fileType=(fp.endsWith('.h')||fp.endsWith('.hpp')||fp.endsWith('.hxx')||fp.endsWith('.hh'))?'header':'source';}else{fname=(sym.file_path||'').split('/').pop()||'';}
elements.push({group:'nodes',data:{id:String(n.id),label:label,kind:kind,shape:nodeShape(kind),file:sym.file_path||'',line:sym.line||0,fan_in:fstat.fan_in||0,fan_out:fstat.fan_out||0,complexity:fstat.cyclomatic_complexity||0,heat:heatVal,comment:sym.comment||'',fileType:fileType,fname:fname}});
});
(graphData.edges||[]).forEach(function(e,idx){elements.push({group:'edges',data:{id:'e'+idx,source:String(e.source_id),target:String(e.target_id),relation:e.relation||'CALLS',weight:e.weight||1}});});
return elements;}
function initCallGraphLazy(){
var entryId=data.metadata&&data.metadata.entry_function_id;fullGraphData=data.call_graph||{nodes:[],edges:[]};
visibleNodeIds=new Set();expandedNodeIds=new Set();expansionChildren.clear();
if(!entryId||!fullGraphData.nodes.length){isLazyMode=false;initCytoscape(buildElements(fullGraphData,data.symbols||[],data.stats||{}),'cy-call');if(cy)markDeadEndNodes();return;}
isLazyMode=true;var symbolMap={};(data.symbols||[]).forEach(function(s){symbolMap[s.symbol_id]=s;});
var statsMap={};if(data.stats&&data.stats.function_stats){data.stats.function_stats.forEach(function(f){statsMap[f.function_id]=f;});}
var sym=symbolMap[entryId]||{},fstat=statsMap[entryId]||{},maxFanIn=data.stats&&data.stats.maxFanIn||1,heatVal=(fstat.fan_in||0)/Math.max(maxFanIn,1);
var kind=sym.kind||'FUNCTION',label=sym.name||String(entryId),fname=(sym.file_path||'').split('/').pop()||'';
visibleNodeIds.add(String(entryId));initCytoscape([{group:'nodes',data:{id:String(entryId),label:label,kind:kind,shape:'ellipse',file:sym.file_path||'',line:sym.line||0,fan_in:fstat.fan_in||0,fan_out:fstat.fan_out||0,complexity:fstat.cyclomatic_complexity||0,heat:heatVal,comment:sym.comment||'',isEntry:true,fname:fname}}],'cy-call');
var hasEntryOutgoing=fullGraphData.edges.some(function(e){return String(e.source_id)===String(entryId);});if(!hasEntryOutgoing&&cy)cy.getElementById(String(entryId)).data('isDeadEnd',true);}
function expandNode(nodeId){
if(!isLazyMode||expandedNodeIds.has(nodeId)||!fullGraphData)return;
var allEdges=fullGraphData.edges.filter(function(e){return String(e.source_id)===nodeId;});
if(!allEdges.length){var nodeSym=(data.symbols||[]).find(function(s){return s.symbol_id===parseInt(nodeId);}),extCalls=(data.external_refs||[]).filter(function(r){return r.caller_name===(nodeSym?nodeSym.name:'');}),el2=document.getElementById('ni-expand');if(extCalls.length>0){expandedNodeIds.add(nodeId);if(el2)el2.textContent='已展开';var nc2=document.getElementById('ni-comment');nc2.textContent='系统函数调用 ('+extCalls.length+'): '+extCalls.map(function(r){return r.callee_name;}).join(', ');nc2.style.display='block';return;}if(el2)el2.textContent='无调用关系';return;}
expandedNodeIds.add(nodeId);var calleeIds=new Set();allEdges.forEach(function(e){calleeIds.add(String(e.target_id));});
var symbolMap={};(data.symbols||[]).forEach(function(s){symbolMap[s.symbol_id]=s;});
var statsMap={};if(data.stats&&data.stats.function_stats){data.stats.function_stats.forEach(function(f){statsMap[f.function_id]=f;});}
var maxFanIn=data.stats&&data.stats.maxFanIn||1,newElements=[],newCalleeIds=new Set(),addedEdgeIds=new Set();
calleeIds.forEach(function(id){
if(!visibleNodeIds.has(id)){newCalleeIds.add(id);visibleNodeIds.add(id);
var nd=fullGraphData.nodes.find(function(n){return String(n.id)===id;}),sym=symbolMap[parseInt(id)]||{},fstat=statsMap[parseInt(id)]||{},heatVal=(fstat.fan_in||0)/Math.max(maxFanIn,1);
var kind=sym.kind||(nd?nd.type:'FUNCTION'),label=(nd&&nd.label)||sym.name||id,fn='';
if(kind!=='FILE_ENTITY'){fn=(sym.file_path||'').split('/').pop()||'';}
newElements.push({group:'nodes',data:{id:id,label:label,kind:kind,shape:'round-rectangle',file:sym.file_path||'',line:sym.line||0,fan_in:fstat.fan_in||0,fan_out:fstat.fan_out||0,complexity:fstat.cyclomatic_complexity||0,heat:heatVal,comment:sym.comment||'',fname:fn}});}});
allEdges.forEach(function(e,idx){var eid='e-lazy-'+idx+'-'+nodeId;newElements.push({group:'edges',data:{id:eid,source:String(e.source_id),target:String(e.target_id),relation:e.relation||'CALLS',weight:e.weight||1}});addedEdgeIds.add(eid);});
if(!newElements.length)return;cy.add(newElements);
newCalleeIds.forEach(function(id){var hasOut=fullGraphData.edges.some(function(e){return String(e.source_id)===id;});if(!hasOut)cy.getElementById(id).data('isDeadEnd',true);});
var pp=cy.getElementById(nodeId).position(),ncArr=Array.from(newCalleeIds),cc=ncArr.length;
if(cc>0){var arc=Math.PI*0.6,start2=Math.PI/2-arc/2;ncArr.forEach(function(id,i){var angle=start2+arc*i/Math.max(cc-1,1),nd2=cy.getElementById(id);if(nd2.length)nd2.position({x:pp.x+150*Math.cos(angle),y:pp.y+150*Math.sin(angle)});});}
newCalleeIds.forEach(function(id){var n2=cy.getElementById(id),exfn=n2.data('fname');if(!exfn)return;var fid='_fl_'+id;if(cy.getElementById(fid).length)return;cy.add({group:'nodes',data:{id:fid,label:exfn,isFileLabel:true},classes:'file-label',position:{x:n2.position().x,y:n2.position().y+n2.height()/2+10}});});
expansionChildren.set(nodeId,{edgeIds:addedEdgeIds,nodeIds:newCalleeIds});}function collapseNode(parentId){var children=expansionChildren.get(parentId);if(!children)return;children.edgeIds.forEach(function(eid){var el=cy.getElementById(eid);if(el.length)el.remove();});children.nodeIds.forEach(function(cid){collapseNode(cid);});children.nodeIds.forEach(function(cid){var fl2=cy.getElementById('_fl_'+cid);if(fl2.length)fl2.remove();var el=cy.getElementById(cid);if(el.length&&el.connectedEdges().length===0){el.remove();visibleNodeIds.delete(cid);}});expansionChildren.delete(parentId);expandedNodeIds.delete(parentId);}
function markDeadEndNodes(){if(!cy||!fullGraphData)return;cy.nodes().forEach(function(n){var nid=n.id(),hasOut=fullGraphData.edges.some(function(e){return String(e.source_id)===nid;});if(!hasOut)n.data('isDeadEnd',true);});}
function initCytoscape(elements,containerId){
var container=document.getElementById(containerId);if(!container)return;
if(typeof cytoscape==='undefined'){container.innerHTML='<div style="padding:40px;color:#d65d0e;text-align:center;"><h3>Cytoscape.js 未加载</h3></div>';return;}
var nodeCount=elements.filter(function(e){return e.group==='nodes';}).length,isLarge=nodeCount>1000,notice=document.getElementById('degrade-notice');
if(isLarge&&notice){notice.style.display='block';notice.textContent='大图模式: '+nodeCount+' 个节点，已启用性能优化';}
var isCall=(containerId==='cy-call');
try{var inst=cytoscape({container:container,elements:elements,
style:[{selector:'node',style:{label:'data(label)',color:'#bdae93','font-size':isLarge?'9px':'11px','text-valign':'center','text-halign':'center','shape':'data(shape)','width':'label','height':'label','text-wrap':'wrap','padding':'6px','border-width':1,'border-color':'#d65d0e','background-color':'#3c3836'}},
{selector:'edge',style:{width:isLarge?1:1.5,'line-color':'#504945','target-arrow-color':'#d65d0e','target-arrow-shape':'triangle','curve-style':'bezier'}},
{selector:'node[isDeadEnd]',style:{'border-color':'#928374','border-width':1,'border-style':'solid'}},
{selector:'node[isEntry]',style:{'color':'#fabd2f','font-weight':'bold'}},
{selector:'node:selected',style:{'border-width':3,'border-color':'#fe8019'}},{selector:'node[fileType="header"]',style:{'border-color':'#83a598','border-width':2}},{selector:'node[fileType="source"]',style:{'border-color':'#b8bb26','border-width':2}},{selector:'node.file-label',style:{'width':0,'height':0,'background-opacity':0,'border-width':0,'label':'data(label)','color':'#928374','font-size':'10px','text-valign':'bottom','text-halign':'center','overlay-opacity':0,'events':'no','text-margin-y':2,'min-zoomed-font-size':4,'z-index':-1}}],
layout:{name:'cose',padding:20},wheelSensitivity:0.15});
try{inst.nodes().forEach(function(n_){var fn_=n_.data('fname');if(!fn_)return;var fid_='_fl_'+n_.id();if(inst.getElementById(fid_).length)return;inst.add({group:'nodes',data:{id:fid_,label:fn_,isFileLabel:true},classes:'file-label',position:{x:n_.position().x,y:n_.position().y+n_.height()/2+10}});});}catch(e){console.warn('file-label err',e);}
if(isLarge){inst.hideEdgesOnViewport=true;inst.motionBlur=true;inst.textEvents='no';}
if(isCall){cy=inst;
cy.on('tap','node',function(evt){var node=evt.target,d=node.data(),hasOut=isLazyMode&&fullGraphData&&fullGraphData.edges.some(function(e){return String(e.source_id)===node.id();});if(hasOut&&!expandedNodeIds.has(node.id())){expandNode(node.id());if(expansionChildren.has(node.id()))return;}var area=document.getElementById('call-graph-area'),rect=area.getBoundingClientRect(),bb=node.renderedBoundingBox(),info=document.getElementById('node-info'),ox=bb.x2-rect.left+6,oy=bb.y2-rect.top+6;info.style.left=Math.min(ox,Math.max(rect.width-380,0))+'px';info.style.top=Math.min(oy,Math.max(rect.height-260,0))+'px';info.style.display='block';document.getElementById('edge-info').style.display='none';document.getElementById('ni-name').textContent=d.label;document.getElementById('ni-kind').textContent=d.kind;document.getElementById('ni-file').textContent=(d.file||'').split('/').pop();document.getElementById('ni-line').textContent=d.line;document.getElementById('ni-fanin').textContent=d.fan_in;document.getElementById('ni-fanout').textContent=d.fan_out;document.getElementById('ni-cc').textContent=d.complexity;var nc=document.getElementById('ni-comment');if(d.comment){nc.textContent=d.comment;nc.style.display='block';}else{nc.style.display='none';}
var er=document.getElementById('ni-expand-row'),ee=document.getElementById('ni-expand');if(isLazyMode&&fullGraphData){if(expandedNodeIds.has(node.id())){ee.textContent='已展开';}else{var cc2=fullGraphData.edges.filter(function(e){return String(e.source_id)===node.id();}).length;ee.textContent=cc2>0?cc2+' 个被调用函数':'无调用关系';}er.style.display='flex';}else if(er){er.style.display='none';}
var extCalls2=(data.external_refs||[]).filter(function(r){return r.caller_name===d.label.split('\n')[0];});if(extCalls2.length>0){var nc3=document.getElementById('ni-comment');nc3.textContent='系统函数调用 ('+extCalls2.length+'): '+extCalls2.map(function(r){return r.callee_name;}).join(', ');nc3.style.display='block';}});
function symById(eid){return(data.symbols||[]).find(function(s){return s.symbol_id===eid;})||{};}
cy.on('tap','edge',function(evt){var edge=evt.target,ed=edge.data(),srcId=parseInt(ed.source),tgtId=parseInt(ed.target),caller=symById(srcId),callee=symById(tgtId),cName=caller.name||('ID_'+srcId),tName=callee.name||('ID_'+tgtId),ei=document.getElementById('edge-info'),mp=edge.midpoint(),container=document.getElementById('call-graph-area'),rect=container.getBoundingClientRect(),cyRect=document.getElementById('cy-call').getBoundingClientRect(),zoomLvl=cy.zoom(),pan=cy.pan(),rpX=mp.x*zoomLvl+pan.x+cyRect.left,rpY=mp.y*zoomLvl+pan.y+cyRect.top;ei.style.left=Math.min(rpX-rect.left+8,Math.max(rect.width-400,0))+'px';ei.style.top=Math.min(rpY-rect.top+8,Math.max(rect.height-200,0))+'px';document.getElementById('ei-header').textContent=cName+' → '+tName;var body='',params=callee.parameters||[];if(callee.return_type)body+='返回: '+callee.return_type+'\n';if(params.length>0){body+='参数 ('+params.length+'):';params.forEach(function(p,i){body+='\n  '+(i+1)+'. '+p;});}else{body+='参数: 无';}document.getElementById('ei-body').textContent=body;ei.style.display='block';document.getElementById('node-info').style.display='none';});
cy.on('tap',function(evt){if(evt.target===cy){document.getElementById('node-info').style.display='none';document.getElementById('edge-info').style.display='none';}});
cy.on('cxttap','node',function(evt){var nid=evt.target.id();if(isLazyMode&&expansionChildren.has(nid))collapseNode(nid);});
cy.on('mouseover','node',function(evt){evt.target.connectedEdges().style('line-color','#E1F656');});
cy.on('mouseout','node',function(evt){evt.target.connectedEdges().style('line-color',null);});
cy.on('mouseover','edge',function(evt){evt.target.style('line-color','#E1F656');evt.target.source().style('border-color','#E1F656');evt.target.target().style('border-color','#E1F656');});
cy.on('mouseout','edge',function(evt){evt.target.style('line-color',null);evt.target.source().style('border-color',null);evt.target.target().style('border-color',null);});
}else{cySide=inst;
cySide.on('tap','node',function(evt){var d=evt.target.data(),area=document.getElementById('call-graph-area'),rect=area.getBoundingClientRect(),bb=evt.target.renderedBoundingBox(),info=document.getElementById('node-info'),ox=bb.x2-rect.left+6,oy=bb.y2-rect.top+6;info.style.left=Math.min(ox,Math.max(rect.width-380,0))+'px';info.style.top=Math.min(oy,Math.max(rect.height-260,0))+'px';info.style.display='block';document.getElementById('ni-name').textContent=d.label;document.getElementById('ni-kind').textContent=d.kind;document.getElementById('ni-file').textContent=(d.file||'').split('/').pop();document.getElementById('ni-line').textContent=d.line;document.getElementById('ni-fanin').textContent=d.fan_in||'-';document.getElementById('ni-fanout').textContent=d.fan_out||'-';document.getElementById('ni-cc').textContent=d.complexity||'-';var nc=document.getElementById('ni-comment');if(d.comment){nc.textContent=d.comment;nc.style.display='block';}else{nc.style.display='none';}document.getElementById('ni-expand-row').style.display='none';});}
}catch(e){console.error('Cytoscape init failed:',e);if(isCall)container.innerHTML='<div style="padding:20px;color:#d65d0e;">图形渲染初始化失败</div>';}}
window.switchCard=function(card){
currentCard=card;document.querySelectorAll('.card-tab').forEach(function(t){t.classList.remove('active');});document.querySelector('.card-tab[data-card="'+card+'"]').classList.add('active');
document.querySelectorAll('#card-body > div').forEach(function(d){d.style.display='none';});
if(card==='include'||card==='type'){document.getElementById('cy-side').style.display='block';if(cySide){cySide.destroy();cySide=null;}var gd=card==='include'?data.include_graph:data.type_graph;initCytoscape(buildElements(gd||{nodes:[],edges:[]},data.symbols||[],data.stats||{}),'cy-side');}
else if(card==='stats'){document.getElementById('stats-panel').style.display='block';renderStats();}
else if(card==='symbols'){document.getElementById('symbol-panel').style.display='block';}};
window.resetLayout=function(){if(cy)cy.layout({name:'cose',padding:20}).run();};
window.fitGraph=function(){if(cy)cy.fit();};
window.filterSymbols=function(q){document.getElementById('symbol-list').querySelectorAll('li').forEach(function(li){li.style.display=li.textContent.toLowerCase().includes(q.toLowerCase())?'':'none';});};
function renderSidebar(){var list=document.getElementById('symbol-list');list.innerHTML='';(data.symbols||[]).forEach(function(s){var li=document.createElement('li');li.textContent=s.name;li.title=s.qualified_name;li.onclick=function(){if(cy){var node=cy.getElementById(String(s.symbol_id));if(node.length){cy.animate({fit:{eles:node,padding:60},duration:400});node.select();}}};list.appendChild(li);});}
function renderStats(){
var stats=data.stats||{},meta=data.metadata||{},sp=document.getElementById('stats-panel');
sp.innerHTML='<div class="stat-card"><h3>项目概览</h3><table id="sum-tbl"></table></div><div class="stat-card"><h3>文件热力图</h3><table id="file-tbl"><tr><th>文件</th><th>行数</th><th>热力</th></tr></table></div><div class="stat-card"><h3>函数热力图</h3><table id="func-tbl"><tr><th>函数</th><th>被调用</th><th>调用</th><th>圈复杂度</th></tr></table></div><div class="stat-card"><h3>异常检测</h3><div id="ano-list"></div></div>';
var st=document.getElementById('sum-tbl');
st.innerHTML='<tr><th>项目</th><td>'+(meta.project_name||'-')+'</td></tr><tr><th>文件数</th><td>'+(meta.file_count||0)+'</td></tr><tr><th>函数数</th><td>'+(meta.function_count||0)+'</td></tr><tr><th>C编译器</th><td>'+(meta.c_compiler||'-')+'</td></tr><tr><th>C++编译器</th><td>'+(meta.cxx_compiler||'-')+'</td></tr>';
var ml=Math.max.apply(null,(stats.file_stats||[]).map(function(f){return f.code_lines||0;}).concat([1]));
var ft=document.getElementById('file-tbl');
(stats.file_stats||[]).sort(function(a,b){return b.code_lines-a.code_lines;}).slice(0,20).forEach(function(f){var h=(f.code_lines||0)/ml,tr=document.createElement('tr');tr.innerHTML='<td>'+f.file_path.split('/').pop()+'</td><td>'+f.code_lines+'</td><td><span class="hotbar" style="width:'+Math.max(h*120,4)+'px;background:'+heatColor(h)+'"></span></td>';ft.appendChild(tr);});
var symMap={};(data.symbols||[]).forEach(function(s){symMap[s.symbol_id]=s;});
var fnT=document.getElementById('func-tbl');
(stats.function_stats||[]).slice(0,20).forEach(function(f){var sym=symMap[f.function_id]||{},tr=document.createElement('tr');tr.innerHTML='<td>'+(sym.name||f.function_id)+'</td><td>'+f.fan_in+'</td><td>'+f.fan_out+'</td><td>'+f.cyclomatic_complexity+'</td>';fnT.appendChild(tr);});
var anoList=document.getElementById('ano-list'),ano=data.anomalies||{};
if(!(ano.circular_includes||[]).length){anoList.innerHTML='<p style="color:#b8bb26;font-size:13px;">未检测到循环包含</p>';}
else{(ano.circular_includes||[]).forEach(function(ci){var d=document.createElement('div');d.className='anomaly';d.textContent='循环包含: '+ci.file_cycle.join(' → ');anoList.appendChild(d);});}
}
function updateMeta(){var meta=data.metadata||{},mi=document.getElementById('meta-info');if(mi){var es=(data.symbols||[]).find(function(s){return s.symbol_id===meta.entry_function_id;});var en=es?es.name:(meta.entry_function_id||'-'),cli=meta.command_line||'',dm=cli.match(/-d\s+(\d+)/),om=cli.match(/-o\s+(\S+)/);mi.textContent=(meta.project_name||'-')+' | '+(meta.generated_at||'-')+' | 入口: '+en+' | 深度: '+(dm?dm[1]:'-')+' | '+(om?om[1]:'-');}}
var isLight=false;window.toggleTheme=function(){isLight=!isLight;document.body.classList.toggle('light',isLight);var btn=document.getElementById('theme-btn');if(btn)btn.textContent=isLight?'☾':'☀';applyCyTheme(isLight);};function applyCyTheme(light){var insts=[];if(cy)insts.push(cy);if(cySide)insts.push(cySide);var dc={bg:'#3c3836',edge:'#504945',text:'#bdae93',nodeBorder:'#d65d0e',selBorder:'#fe8019',entryText:'#fabd2f',deadBorder:'#928374',headBorder:'#83a598',srcBorder:'#b8bb26'},lc={bg:'#eee8d5',edge:'#93a1a1',text:'#657b83',nodeBorder:'#cb4b16',selBorder:'#268bd2',entryText:'#b58900',deadBorder:'#586e75',headBorder:'#268bd2',srcBorder:'#859900'},c=light?lc:dc;insts.forEach(function(inst){inst.style().selector('node').style({'background-color':c.bg,color:c.text,'border-color':c.nodeBorder}).update();inst.style().selector('edge').style({'line-color':c.edge,'target-arrow-color':c.nodeBorder}).update();inst.style().selector('node[isEntry]').style({color:c.entryText}).update();inst.style().selector('node[isDeadEnd]').style({'border-color':c.deadBorder}).update();inst.style().selector('node[fileType="header"]').style({'border-color':c.headBorder}).update();inst.style().selector('node[fileType="source"]').style({'border-color':c.srcBorder}).update();inst.style().selector('node.file-label').style({color:c.deadBorder}).update();});}
function initSplitter(){var divider=document.getElementById('divider'),left=document.getElementById('call-graph-area'),right=document.getElementById('card-panel');if(!divider||!left||!right)return;divider.addEventListener('mousedown',function(e){e.preventDefault();divider.classList.add('active');var startX=e.clientX,leftW=left.getBoundingClientRect().width,totalW=left.parentElement.getBoundingClientRect().width;function onMove(ev){var pct=((leftW+ev.clientX-startX)/totalW)*100;if(pct<20||pct>80)return;left.style.width=pct+'%';right.style.width=(100-pct)+'%';if(cy)cy.resize();if(cySide)cySide.resize();}function onUp(){divider.classList.remove('active');document.removeEventListener('mousemove',onMove);document.removeEventListener('mouseup',onUp);}document.addEventListener('mousemove',onMove);document.addEventListener('mouseup',onUp);});}
document.addEventListener('DOMContentLoaded',function(){updateMeta();renderSidebar();initCallGraphLazy();initSplitter();switchCard('include');});
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
        {"command_line", ctx.command_line.empty() ? "" : ctx.command_line},
        {"entry_function_id", ctx.entry_function_id},
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
            obj["comment"] = fit->comment;
        }

        // 补充复合类型注释（从 ctx.composites 查找）
        if (s.kind == SymbolKind::STRUCT || s.kind == SymbolKind::CLASS) {
            auto cit = std::find_if(ctx.composites.begin(), ctx.composites.end(),
                                    [id = s.symbol_id](const CompositeSymbol& c) {
                                        return c.symbol_id == id;
                                    });
            if (cit != ctx.composites.end() && !cit->comment.empty()) {
                obj["comment"] = cit->comment;
            }
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
        cobj["comment"] = csym.comment;
        comp_array.push_back(cobj);
    }
    root["composites"] = comp_array;

    // 调用图（优先使用完整边数据，兼容旧数据流）
    const auto& call_edges = !ctx.full_call_edges.empty() ? ctx.full_call_edges : ctx.call_edges;
        root["call_graph"] = convert_call_graph(call_edges, ctx.symbols);

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

    // 生成边（去重：合并相同调用关系的 weight）
    std::map<std::pair<uint32_t,uint32_t>, uint32_t> dedup;
    for (const auto& e : edges) {
        if (e.callee_id == std::numeric_limits<uint32_t>::max()) continue;
        dedup[{e.caller_id, e.callee_id}] += e.call_count;
    }
    for (const auto& [ids, w] : dedup) {
        json edge;
        edge["source_id"] = ids.first;
        edge["target_id"] = ids.second;
        edge["relation"] = "CALLS";
        edge["weight"] = w;
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
