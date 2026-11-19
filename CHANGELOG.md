v3.7.1
--------------------------------
* 删除图标确认窗口改为自适应暗黑/浅色的原生 WinAPI 自绘确认窗口。
* 后续新增窗口约定：必须使用同一套 MenuTheme=Auto/Dark/Light 主题逻辑。

MiniLaunch2000 v3.6 Menu Theme
--------------------------------
Base: v3.5.4 Hover Border / Glow

Added:
- MenuTheme=Auto/Dark/Light controls the system right-click menu theme.
- Auto follows Windows app theme on Windows 10/11.
- Dark forces dark menu on supported Windows versions.
- Light forces light menu.
- Windows 7 automatically keeps the standard light menu because the dark menu API is unavailable.

Kept:
- Transparent toolbar UI unchanged.
- HoverBorder / HoverGlow unchanged.
- PressFeedback unchanged.
- Save position, tray icon, EXE icon, autosize unchanged.

MiniLaunch2000 v3.5.4
--------------------------------
+ Added HoverBorder: rounded highlight border when mouse hovers over an icon.
+ Added HoverGlow: optional soft glow behind the hovered icon cell.
+ Added INI options: HoverBorder, HoverGlow, HoverBorderColor, HoverBorderOpacity, HoverBorderWidth, HoverGlowOpacity, HoverGlowSize, HoverRadius.

v3.7
--------------------------------
+ 在图标上右键可删除此图标。
+ 删除后会同步清理 INI 中对应 Item 配置，并自动重排 Item 编号。
