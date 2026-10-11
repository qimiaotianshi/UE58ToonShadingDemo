# DreamBloom 1.8.1：DA 配置与 Actor Tag 识别

此版本取代 1.8 的关卡管理 Actor 方案。`WardrobeSceneDirector` 类已删除，不需要放置管理 Actor。Q/E 相机过渡由菜单内部管理。

## 配置位置

打开原来的 `DA_WardrobeCharacters`。`Characters` 每个条目同时配置：

| 字段 | 示例与用途 |
|---|---|
| Label | 窗边人物，顶部标签名称 |
| Camera Tag | `Camera_Window`，相机 Actor 的 Tag |
| Character Tag | `Person_Window`，人物 Actor 的 Tag |
| Duration | 进入此人物镜头的秒数，默认 1.5；0 为立即切换 |
| Progress Curve | Curve Float：X 是 0～1 时间，Y 是进度；未指定则缓入缓出 |
| Departure Offset / Arrival Offset | 贝塞尔曲线控制点偏移，单位 cm，分别沿起点/终点相机局部坐标 |
| Allow Character Rotation | 是否允许左键横拖旋转，坐姿可以关闭 |
| Interactions | 该人物自己的部位、骨骼、范围、随机互动 Sequence 和冷却 |

DA 顶层的 **Initial Character** 设置初始条目编号（从 0 开始）；返回菜单时优先恢复本次游戏上次选择。**Draw Transition Path** 用于调试路径。

示例：

| DA 条目 | Camera Tag | Character Tag |
|---|---|---|
| 0 窗边人物 | Camera_Window | Person_Window |
| 1 沙发人物 | Camera_Sofa | Person_Sofa |

## 给场景对象设置 Tag

1. 选中窗边 CameraActor 或 CineCameraActor，在 Details 搜索 **Tags**，找到 **Actor → Tags** 数组，新增 `Camera_Window`。
2. 选中窗边的 `WardrobeShowcase` 人物（或继承它的 Animated Showcase 蓝图），在 **Actor → Tags** 新增 `Person_Window`。
3. 沙发那一组按表格配置。
4. 模型、材质、AnimBP 仍在每个人物的 Character 骨骼网格组件上配置。待机和物理仍由 AnimBP 负责。

使用 **Actor Tags**，不是 Component Tags、Gameplay Tags，也不是 World Outliner 中显示的名称。每个配置 Tag 必须唯一匹配一个已加载的 Actor。Camera Tag 必须对应相机，Character Tag 必须对应 WardrobeShowcase。不要让不同人物条目指向同一个人物。

程序在每次切换时重新查找 Tag。Tag 为空、重复、缺失、对象类型错误或人物没有模型时，拒绝切换，显示条目编号和原因；Output Log 搜索 `WARDROBE_TAG_BINDING` 可以定位。World Partition/Data Layer 中尚未加载的 Actor 不会被找到，请让这些相机与人物随展示关卡加载；本功能不会自动加载分区。

人物 Actor 上的 Interaction Data / Interaction Profile Index 改为运行时只读，**不用手动填写**，会自动使用当前 DA 条目的 Interactions。

## 菜单配置与切换行为

- `DA_WardrobeMenu → Character Data` 指向自己的 `DA_WardrobeCharacters`。
- `DA_WardrobeMenu → Menu Level` 指向展示关卡，目的地仍只使用 Levels。
- 已有默认 `DA_WardrobeMenu` 路径的工程继续使用原配置。自定义路径可以在继承 `WardrobeMenuGameMode` 的 GameMode 蓝图 Class Defaults 的 **Menu Data** 中指定，再设为该关卡的 GameMode Override。已有自定义 Menu Widget 的 Data 配置也可继续使用。
- Q/E 或顶部标签切换：停止旧人物互动，镜头沿曲线移动，完成后只启用新人物互动。过渡中再次按键不会排队；相机调整和点击互动锁定到结束。
- 两个偏移默认 `(0,0,60)`，形成抬高弧线；都填零则为直线。速度曲线建议从 (0,0) 单调上升至 (1,1)。路径不自动避障，需要结合家具墙壁调试。
- 浏览时操作临时相机，不改关卡里摆好的相机。旋转/FOV 平滑过渡；不同镜头的特殊后处理不提供完整混合，建议保持一致。到位使用切换时采样的相机视图，不持续跟随该相机后续动画。
- 原自动随机语音、目的地 Sequence 模式保持删除；部位互动 Sequence 和 UI 音效保留。
- Reaction 的 Character Binding Tag 仍是 **Sequencer 绑定标签**，用于将互动轨道绑定当前人物，和此处用于查找场景 Actor 的 Tag 不是一回事。

## 更新和迁移

本机 `F:/新建文件夹/MyProject` 已更新源码与 `UnrealEditor-DreamBloom-181.dll`，模块清单指向新版。保存并重启编辑器后生效；不要使用 Live Coding 加载这次类型变动。

若已经在自己的场景放置 1.8 的 WardrobeSceneDirector，请移除它，将原条目的相机/人物关系按上表填写到人物 DA。该类已删除，旧关卡可能显示缺失类提示。新版不再读取它的配置。自己的模型、地图和 DA 没有被覆盖或重存。

另一个电脑使用修改过源码的引擎时，安装包内 **DreamBloom/Source、Resources、DreamBloom.uplugin**，保留自己的 Content，使用该电脑的引擎在 Rider 编译 **Development Editor / Win64**。不要用“仅本机官方引擎”目录中的 DLL。Source 建议备份后整目录替换，避免已删除旧文件残留；自行改过插件需合并修改。

本机修改前备份：`F:/新建文件夹/MyProject/Saved/WardrobeBackup-20261006-Tags181`。

新示例在 `Content/WardrobeExamples181/L_TagExample`：包含两个引擎占位人物、两台带 Tag 的相机，以及 DA 和 GameMode 蓝图，**没有管理 Actor**。互动条目是配置示例，需要填自己的互动 Sequence。旧 `WardrobeExamples18` 示例仍在 Content 中并已另行备份；它使用已删除的管理 Actor，请使用新的 `WardrobeExamples181` 示例。

验证：官方 UE 5.8.3 编译通过；自动化覆盖 Tag 缺失、重复、类型错误、曲线/FOV、到位切换互动与 Sequence 取消；新示例通过 NullRHI 游戏启动检查。未在原人物资源上验证视觉和 KawaiiPhysics，原来的自定义资源版本不兼容问题不属于本次修改。
