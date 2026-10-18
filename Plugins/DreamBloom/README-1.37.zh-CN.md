DreamBloom 1.37：捏脸松手阈值 / 手脚改为纯 IK 拖拽

本机 F:/新建文件夹/MyProject 已更新源码、DLL、动画蓝图和 DA。原文件备份在工作目录 work/wardrobe/PinchThreshold66/Before。

捏脸配置
打开 DA_WardrobeCharacters（本机演示为 DA_FullBodyCharacters）→ Characters → 对应人物 → Pinch Regions → 对应部位 → Release → Release Trigger Distance。
默认 5 cm。数值越大，需要拖得越远才触发；设为 0，恢复每次松手都触发。
距离是从本次按下位置开始，鼠标射线在拖拽平面上的世界厘米位移，不是累计鼠标走过的路径，也不是脸颊骨骼形变距离。先于 Drag Sensitivity、Axis Scale、Max Distance 计算。
本轮达到过阈值就算满足，拉回后松手也会触发；每次重新抓取都重新计数。
满足阈值时，松手播放 ReleaseSound 随机池中本轮选中的声音，同时开始 ReleaseAnimation，沿用 ReleaseBlendTime 平滑混合。
未满足时，跳过这两个反馈；仍停止 GrabSound / MoveSound，脸颊与受力姿势正常恢复。
原捏脸 Body Left / Right / Up / Down 动作及动作内的痛苦表情保留。

手脚配置
Characters → Limb Drags 保留骨骼链、命中范围、Max Distance、Drag Sensitivity、Follow Time、Return Time。
手脚的 Body Left / Right / Up / Down、Reaction Strength 已从 DA 和代码删除。
Wardrobe Limb Reaction 的运行时和编辑器节点代码已彻底删除。Wardrobe Limb IK 保留，并保留脚部贴地、松手恢复、twist 骨骼处理与 compact pose 索引防崩保护。
拖手脚期间头、脖子、眼睛继续使用实时鼠标射线注视。没有新增手脚动作需求。

另一台电脑更新（已有此插件和角色基础资源的工程）
1. 在旧版插件仍能正常加载时，打开所有使用 Wardrobe Limb Reaction 的动画蓝图。删除这个节点，把它原来的输入姿势直接接到原来的下游节点。保留 Wardrobe Limb IK、Foot IK、Release、Look 和 KawaiiPhysics 节点。编译并保存，然后关闭 UE。
2. 备份原插件。用本包 Plugins/DreamBloom/Source 和 DreamBloom.uplugin 更新对应文件。Source 下只能有 DreamBloom、DreamBloomEditor，不要复制成 Source/Source。
3. 用目标电脑实际使用的引擎编译项目 Editor 目标（Rider Build 或对应 Build.bat，Win64 Development/DebugGame 配置跟工程一致）。自改源码引擎请不要复制本包官方引擎 Binaries；移开原 DreamBloom Binaries/Intermediate 后重编。
4. 打开工程，编译并保存自己的动画蓝图和 DA。设置各捏脸部位 Release Trigger Distance。自己的角色、相机和交互配置无需覆盖。
本包 Content 内的 AB_FullBodySequencePinch 已移除旧节点，DA_FullBodyCharacters 为演示配置。若自己改过这两个资源，按上面步骤调整，不要直接覆盖。
包内只提供这两个更新资产。模型、Skeleton、默认 Sequence、捏脸反馈动作、KawaiiPhysics 等沿用已有依赖；迁移到全新工程请用 UE 的“迁移”收集依赖。
以前制作的手脚 FBX / VMD / Blender / UE 动画文件保留供单独使用，当前手脚玩法不再引用它们。

验证
官方 UE5.8 Development Editor 编译链接通过。本机工程和测试工程各 6 项自动化回归通过。
涵盖零拖拽、阈值以下、恰好阈值、达到后拉回、重新抓取重置、0 阈值、灵敏度与形变限幅独立；摸脸动作直接混合、音频停止和循环状态；四肢实际蒙皮 IK、松手恢复、贴地、实时鼠标注视及非法 compact pose 索引保护。
自动化使用无界面模式验证生产拖拽与动画路径，未进行本次人工 PIE 试听。ReleaseSound 与 ReleaseAnimation 在生产代码中受同一阈值分支控制。
