MiniLaunch2000 - Launcher 2000 风格小图标启动器

功能：
- 单 EXE + INI 配置
- Win7～Win11 原生 WinAPI
- 16x16 / 24x24 / 32x32 图标
- 窗口宽度随图标数量自动变化，超过 MaxColumns 后自动换行
- 左键点击启动程序
- 右键菜单：添加程序、打开/编辑 INI、重新加载、置顶、退出
- 无标题栏模式下，按住空白处左键可拖动窗口
- 托盘图标：双击隐藏/显示，右键菜单

使用：
1. 把 MiniLaunch2000.exe 和 MiniLaunch2000.ini 放在同一目录。
2. 运行 EXE。
3. 右键空白处 -> 添加程序，或直接编辑 INI。
4. 改完 INI 后右键 -> 重新加载。

INI 关键项：
[Settings]
IconSize=16       图标大小
MaxColumns=16     每行最多图标数量；少于这个数量时窗口会自动变短，最短 1 个图标宽度
Columns=16        旧配置兼容项，可不用
CellWidth=24      每个格子的宽度
CellHeight=24     每个格子的高度
Margin=4          边距
StayOnTop=1       置顶
ShowCaption=0     0=无标题栏，1=普通窗口标题栏

[Item1]
Name=Notepad      鼠标提示名称
Path=notepad.exe  程序/快捷方式/文件夹/网址
Args=             参数
WorkDir=          工作目录
Icon=             单独指定图标文件，可留空
IconIndex=0       图标序号

编译：
- Visual Studio：打开 “x64 Native Tools Command Prompt for VS”，运行 build_vs.bat
- MinGW-w64：运行 build_mingw.bat

建议：
INI 文件保存为 UTF-16 LE with BOM 或 ANSI。中文路径建议用 UTF-16 LE with BOM。


版本 v3.2：
- 基于 v3.1。
- 退出时自动保存窗口位置到 INI：WindowX / WindowY。
- 同时记录 WindowW / WindowH 供参考；当前窗口大小仍按图标数量自动计算。
- 下次启动自动恢复上次位置，若位置跑到当前屏幕外则回退为系统默认位置。


[v3.3]
- 修复右键菜单“退出”直接 PostQuitMessage 导致不触发 WM_DESTROY、位置不保存的问题。
- 退出时先 SaveWindowPlacement()，再 DestroyWindow()。
- 增加 WM_CLOSE 保存路径，避免其他关闭方式漏保存。

v3.4 更新：EXE 图标 + Tray 图标
- 新增 MiniLaunch2000.ico、resource.h、MiniLaunch2000.rc。
- build_mingw.bat 会先用 windres 编译资源，再链接到 EXE。
- build_vs.bat 会先用 rc.exe 编译资源，再链接到 EXE。
- EXE 文件图标、窗口图标和托盘图标均使用 MiniLaunch2000.ico。


v3.5.2 透明度说明
------------------
TransparentMode=1 时：
- BackgroundOpacity：只控制黑色工具条背景/边框透明度，0~255。
- IconOpacity：只控制图标透明度，0~255，建议保持 255。
- 旧版 Opacity 仍兼容；如果没有 BackgroundOpacity，会读取 Opacity 作为背景透明度。

v3.5.4 Hover Border / Glow
--------------------------------
HoverBorder=1              ; 鼠标悬停图标时显示圆角边框
HoverGlow=1                ; 鼠标悬停图标时显示淡光晕
HoverBorderColor=0x4DA3FF  ; RGB 颜色，也支持 #4DA3FF
HoverBorderOpacity=170     ; 边框透明度 0~255
HoverBorderWidth=1         ; 边框宽度 1~6
HoverGlowOpacity=55        ; 光晕透明度 0~255
HoverGlowSize=3            ; 光晕扩展像素 0~12
HoverRadius=5              ; 圆角半径 0~16

v3.6 MenuTheme
--------------
新增 Settings/MenuTheme：
  MenuTheme=Auto   跟随 Windows 应用主题（Win10/11），Win7 自动浅色
  MenuTheme=Dark   强制右键菜单暗黑（受系统支持情况影响）
  MenuTheme=Light  强制右键菜单浅色

本版本只处理右键菜单主题，不改半透明工具条本体界面。


开发约定：从 v3.7.1 开始，所有新增窗口必须跟随 MenuTheme=Auto/Dark/Light，自适应暗黑/浅色；不得再直接使用裸 MessageBox 作为新增交互窗口。
