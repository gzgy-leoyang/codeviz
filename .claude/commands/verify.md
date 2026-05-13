# /verify — 构建 + 分析测试项目 + 打开报告

## 步骤

1. **构建项目**
   - 执行 `./build.sh`
   - 若失败，输出错误日志并停止

2. **分析测试项目**
   - `./build/output/codeviz -p /home/dd/Works/ReadSrc/test_project/ -o /tmp/codeviz_test_report.html`
   - 若失败，输出错误日志并停止

3. **打开报告**
   - `firefox /tmp/codeviz_test_report.html`

4. **汇报结果**
   - 告知用户构建状态和报告路径
