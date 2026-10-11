# 花间来信 · UE5.8 登录与回忆剧场

## 使用
当前已打开的旧编辑器需要重启一次，以加载新安装的原生插件。独立运行预览已另行启动，可直接体验。
打开工程后，打开 Content/DreamBloom/Maps/L_DreamBloom，点击编辑器 Play。
输入昵称（可留空），点「启程」进入，再点「开启回忆 · 循环播放」。
Sequence 播放时可以暂停/继续、从头播放，或返回花园。返回后播放停止。
这是本地游客入口，不连接账号服务器，不保存密码。

## 工程内容
- 启动关卡：/Game/DreamBloom/Maps/L_DreamBloom（由原 Untitled 场景另存）
- UI 蓝图：/Game/DreamBloom/UI/WBP_DreamBloomLogin
- GameMode 蓝图：/Game/DreamBloom/UI/BP_DreamBloomGameMode
- 背景纹理：/Game/DreamBloom/Art/T_DreamGarden
- 原 Sequence：/Game/NewLevelSequence，30 fps，150 帧，5 秒。
- 原生 UI 源码：Plugins/DreamBloom/Source/DreamBloom
- 运行时插画、字体和图标：Plugins/DreamBloom/Resources
- 原文件备份：Saved/DreamBloomBackup/20260914-original

在 GameMode 蓝图的 Class Defaults 中修改 Memory Sequence 可换播放内容；Login Widget Class 可换 UI 子类。
UI 使用原生 Slate 绘制并封装为 UUserWidget。WBP 是可继承入口；具体布局与配色在 DreamBloomWidget.cpp 中编辑，修改 C++ 后需重新编译插件。窗口按 1600×900 设计尺寸等比缩放。
复制场景中保留了原有 Actor；没有修改原 Sequence 的镜头、关键帧、时长。

## 美术与资源许可
整体参考《无限暖暖》的轻盈幻想气质，采用雾蓝、奶油白、浅粉与细金线。没有使用游戏提取素材、游戏 Logo 或角色图片。
背景花门与植物插画为本次原创程序绘制。
- Noto Serif SC：Google Fonts，SIL Open Font License 1.1。https://github.com/google/fonts/tree/main/ofl/notoserifsc
- Cormorant Garamond：Google Fonts，SIL Open Font License 1.1。https://github.com/google/fonts/tree/main/ofl/cormorantgaramond
- Lucide Icons：ISC，部分 Feather 衍生图标另附 MIT。https://github.com/lucide-icons/lucide

原字体、实例化后的常规字重字体、完整许可文本、资源 URL 清单均保留在 Resources。SVG 颜色被调整为白色以便 UE 按主题染色。

## 打包与维护
插件属于 Runtime 模块，包含源码与本机 UE5.8.2 的编辑器 DLL；在其他引擎版本或打包目标中需要使用 Visual Studio 重新编译。
Resources 使用 RuntimeDependencies 的 NonUFS staging；启动关卡与资源路径已经配置。此次验证针对本机编辑器/Standalone，不代表已完成 Shipping 打包。
卸载时禁用 DreamBloom 插件，恢复备份 DefaultEngine.ini，并重新选择原关卡。原关卡和原 Sequence 保持完整。

## 已完成验证
UE5.8.2 实际运行：自然循环 3 次，暂停 2 秒期间播放时间稳定，Sequence 摄像机接管正常，返回后播放器停止。
最终运行截图为 1600×900；同一布局也在 888×500 窗口验证。原 Untitled、NewWorld 和 NewLevelSequence 的 SHA-256 与修改前完全一致。
