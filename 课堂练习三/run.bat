@echo off
chcp 65001 >nul
cd /d "%~dp0"
echo 正在检测 images\holes.jpg 中的完整圆孔 ...
"build\Release\hole_count.exe" "images\holes.jpg" --out results
echo.
echo 处理完成，结果图在 results 文件夹。
pause