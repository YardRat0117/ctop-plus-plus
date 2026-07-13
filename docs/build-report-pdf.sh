#!/bin/bash
# ctop++ 中期报告 PDF 构建脚本
# 用法：./build-report-pdf.sh

set -e
cd "$(dirname "$0")"

echo "==> 构建报告 PDF 镜像..."
docker build -t ctopp-report -f Dockerfile.report .

echo ""
echo "==> 生成 mid-term-report.pdf..."
docker run --rm -v "$(pwd):/docs" ctopp-report

echo ""
echo "==> 完成！输出文件：docs/mid-term-report.pdf"
ls -lh mid-term-report.pdf 2>/dev/null || echo "（PDF 生成在容器内，请检查挂载）"
