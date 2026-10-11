# DreamBloom 1.8：场景机位与人物切换

Q/E 现在切换关卡中预先摆好的相机和人物组合。角色始终各自留在场景里，切换不会更换模型、移动人物到同一个位置，也不会替换人物的动画蓝图。

## 安装和迁移

1. 保存工程，关闭 UE 编辑器。此次涉及 UPROPERTY 和类型变动，请使用完整重新编译，不要用 Live Coding 更新。
2. 备份原 `Plugins/DreamBloom`，使用包内 `DreamBloom/Source`、`DreamBloom/Resources` 和 `DreamBloom.uplugin` 替换对应内容。Source 建议整目录替换，防止已删除的旧文件留下参与编译；如自行改过插件代码，先对比合并。
3. 自定义源码引擎电脑：不使用本机编译的 DLL。生成工程文件，在 Rider 中选 **Development Editor / Win64**，编译你的工程 Editor Target，再打开工程。纯蓝图工程需要通过引擎构建插件或先添加一个 C++ 类来生成编译目标。
4. 本机官方引擎 `F:/UE_5.8`：源码与已编译的 `UnrealEditor-DreamBloom-18.dll` 已放入 MyProject，模块清单已指向新版；**保存并重启 UE 后生效**，不需要另外运行脚本。另附“本机官方引擎安装.ps1”用于需要时重新安装，必须先关闭 UE；该脚本只针对这台电脑，不用于其他源码引擎。
5. 旧 DA 不需要用示例 DA 覆盖。加载新版后检查配置再保存；被删除的字段不再显示。如蓝图调用了已移除的函数，需要删除或替换对应节点后重新编译蓝图。

本机源码修改前备份：`F:/新建文件夹/MyProject/Saved/WardrobeBackup-20261006-Stations18`。

## 在自己的关卡中配置

1. 在 World Settings 中将 GameMode Override 设置为 `WardrobeMenuGameMode`，或使用继承它的蓝图。
2. 每个人物使用一个 `WardrobeShowcase`（已有的 Animated Showcase 若继承它可继续使用）。在其 **Character** 骨骼网格组件配置模型、材质、Animation Mode = Use Animation Blueprint、Anim Class。待机动作和 KawaiiPhysics 仍由你的 AnimBP 负责。
3. 把人物摆在窗边、沙发旁等位置。分别摆好 CameraActor 或 CineCameraActor。
4. 关卡里添加一个 **WardrobeSceneDirector**，整个展示关卡只放一个。在 **Menu Data** 指定自己的 `DA_WardrobeMenu`。
5. 展开 **Stations** 数组，按 Q/E 顺序添加条目。用吸管给每个条目的 **Camera**、**Character** 指定关卡中的相机和人物，填写 **Label**。
6. **Initial Station** 指定首次进入时使用哪一组，编号从 0 开始。返回展示关卡时优先恢复本次游戏上次选择的组。

配对使用保存到关卡的 Actor 直接引用，不需要给相机和人物起同名 Tag。复制一组人物后，请检查新条目引用，避免仍指向旧人物。建议管理器、相机、人物放在同一个常驻关卡；World Partition / Data Layer 项目应确保这些 Actor 提前加载。此功能不会自动加载一个尚未加载的分区。

## 每个人物的 Interactions

- 人物 Actor 的 **Interaction Data** 指向一个 `WardrobeCharacterData` 类型的 DA，可以继续使用原来的 `DA_WardrobeCharacters`。
- **Interaction Profile Index** 选择该 DA 的 `Characters` 数组项，0 是第一个。不同人物可以用不同 DA，也可以共用 DA 的不同数组项。
- 不指定 Interaction Data 时，读取 `Menu Data → Character Data`，再按该人物的 Profile Index 取配置。
- 原有 Interactions、骨骼/Socket、形状、偏移、随机权重、不连续重复、冷却、最大互动时长和互动 Sequence 都保留。
- 互动 Sequence 若需要绑定当前人物，给 Sequencer 人物绑定设置标签，并在 Reaction 的 **Character Binding Tag** 填同一个值。这个标签用于 Sequence 重绑定，与相机人物配对无关。绑定对象应是当前 WardrobeShowcase Actor，动画轨道挂它的 Character 组件。留空则保留 Sequence 自己保存的绑定。

Q/E 切换时会停止旧人物的互动 Sequence 并恢复状态。过渡时两边都不能点击；镜头到位后，仅启用目标人物的互动。持续按键期间不会排队重复切换。点击 UI 标签也使用相同流程。

## 相机过渡参数

参数放在 **目标条目** 上；无论从哪组进入它，都使用它的参数。

| 参数 | 用途 |
|---|---|
| Duration | 过渡秒数，默认 1.5；0 为立即切换 |
| Progress Curve | Curve Float，横轴 0～1 为时间，纵轴为进度。建议起点 (0,0)、终点 (1,1)，单调上升；未指定时缓入缓出 |
| Departure Offset | 三次贝塞尔第一个控制点的偏移，以出发镜头局部坐标表示，单位 cm |
| Arrival Offset | 第二个控制点的偏移，以目标镜头局部坐标表示，单位 cm |
| Allow Character Rotation | 是否允许左键横拖旋转这个人物；坐姿建议关闭 |
| Draw Transition Path | 管理器调试开关，在切换过程中绘制青色路径 |

两个控制点的基准位于起终点连线的 1/3、2/3 处。默认两个偏移都是 `(0,0,60)`，会形成抬高的弧线；都设为 `(0,0,0)` 就是直线路径。局部 X 为前方、Y 为右方、Z 为上方。比如两个偏移的 Y 都设为 100，可以让路径向侧面弯曲；镜头旋转会改变这些局部方向。

空间路线由偏移决定，走得快慢由 Progress Curve 决定。相机位置、旋转、FOV 会过渡，旋转使用四元数最短路径。镜头之间的特殊后处理、投影模式差异不提供完整电影级混合，建议使用一致的相机后处理设置。路径不会自动避开墙壁家具，需要在场景中调试偏移和时长。

浏览操作作用于临时预览镜头，不修改摆在关卡里的相机：左键横拖转角色、右键纵拖俯仰、中键平移、滚轮缩放。点击重置返回当前组原始镜头和人物朝向；互动 Sequence 播放期间锁定这些操作。镜头到达后使用目标相机当时采样的视图，不持续跟踪该相机后续的动画。

## 删除和保留内容

- 删除人物自动随机语音及全部延迟、音量、间隔、重复限制、计时和播放代码。
- 删除目的地 Sequence 选择、地图/Sequence 切换开关、自动播放目的地 Sequence、循环/暂停/停止播放 HUD。
- 删除旧的同一个 Actor 替换模型逻辑，以及人物 DA 中 Mesh、Materials、Scale Multiplier、Anim Blueprint 字段；这些改为各人物组件上的配置。
- 目的地只保留地图，`DA_WardrobeMenu → Levels` 配置地图和图片，**Menu Level** 指定返回展示界面的关卡。
- 保留地图加载页、UI 音效和部位互动随机 Sequence。UI 音效 DA 中 **Station Switch** 控制 Q/E 和标签切换音效，沿用原 ActionSwitch 字段保存的数据。
- 不删除你 Content 中的声音、Sequence、模型资产。

## 示例与验证

包内可选示例 `Content/WardrobeExamples18` 拷入工程 Content，打开 `L_StationExample`。其中有两个人物占位方块、两个相机、管理器和两个 DA，可直接参考引用关系。方块使用引擎自带 SkeletalCube，不是成品人物；部位条目尚未配置互动 Sequence，需要你填入自己的资源。

源码已在本机官方 UE 5.8.3 编译。自动化测试覆盖曲线、FOV、机位切换、互动启停和已删除字段；独立示例用于检查运行时加载。原工程的自定义 UE5-Main 124 资源兼容问题不由此更新解决，不对原场景做重存或版本号修改。自定义引擎上的人物、KawaiiPhysics 和实际互动动画仍需在你配置完成后实机检查。
