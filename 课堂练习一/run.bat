@echo off
chcp 65001 >nul
cd /d "%~dp0"
echo 正在处理 images\paperclips.jpg ...
"build\Release\paperclip_count.exe" "images\paperclips.jpg" --out results
echo.
echo 处理完成，结果图在 results 文件夹。
pause