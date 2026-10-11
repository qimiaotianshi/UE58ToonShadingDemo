# DreamBloom 1.11：Daniya 脸颊揉捏 Demo

已安装到 F:\新建文件夹\MyProject。新增 Content/WardrobePinch21；原来的地图、人物 DA 与原始无标题.blend 保留。

## 使用 Demo

1. 重启 UE，加载更新后的 DreamBloom 与 DreamBloomEditor 模块。
2. 打开 /Game/WardrobePinch21/L_CheekPinchDemo，点击 Play。
3. 在人物展示区域按住左键，从左右脸颊附近向外或上下拖动；松开后回弹。
4. 右键横拖旋转人物，中键平移，滚轮缩放。

Demo 有一个人物与脸部近景相机。实际揉捏范围绑定在稳定的头骨上；按下后固定选中部位，直到松开。此 Demo 未配置目的地地图。

## Blender 交付

桌面三转二/Daniya_CheekPinch_Rigged.blend 是修改版。原始无标题.blend 未覆盖。

- 增加 DB_Pinch_Cheek_L、DB_Pinch_Cheek_R，挂在原来的头骨下。
- 每侧 78 个脸颊顶点以连续衰减权重绑定到辅助骨骼；原有权重按比例保留。
- 新增 DB_Cheek_L_Bulge、DB_Cheek_L_Press、DB_Cheek_R_Bulge、DB_Cheek_R_Press 四个局部修形。
- 修改版 Blender 保留原来的 186 个 Shape Key，并额外添加修形；辅助骨骼侧向移动的驱动器用于预览修形联动。
- 修复并打包原模型的贴图路径；清理修改版中的 MMD 碰撞辅助物，保留可见模型与骨架。
- UE 导出 FBX 包含新修形，原有大量表情只保留在 Blender 编辑源中。需要把旧表情也用于 UE 时可从修改版重新导出。

## UE 配置

SK_DaniyaPinch：新模型；AB_DaniyaPinch_Skeleton：独立新骨骼；AB_DaniyaPinch：揉捏测试动画蓝图。
DA_PinchCharacters：左右脸颊的揉捏配置。
DA_PinchMenu、BP_PinchGameMode：专用 Demo 菜单与 GameMode。

动画蓝图接法：原始输入姿势 → Local To Component → Wardrobe Pinch（左）→ Wardrobe Pinch（右）→ Component To Local → Output Pose。
Demo 输入是参考姿势。正式使用时替换成你的待机动作、状态机或 Sequence 输入。揉捏节点叠加在输入姿势上，未在人物 DA 或 Showcase 添加 Idle Animation。

正式蓝图中，每个辅助骨骼添加一个 Wardrobe Pinch 节点并指定 Bone To Modify；将节点接在身体动画之后，KawaiiPhysics 之前。运行时节点会从所属 WardrobeShowcase 读取对应骨骼的揉捏偏移，动画工作线程只使用数值快照。

DA_WardrobeCharacters 的人物条目新增 Pinch Regions：
- Bone：有局部蒙皮权重的变形辅助骨骼。
- Anchor Bone：稳定点击区域的骨骼/Socket，留空使用变形骨骼。
- Offset：Anchor 骨骼局部坐标；骨架若有缩放，需要按该局部空间设置。
- Hit Radius：世界厘米单位的点击球半径，乘人物组件缩放，不乘 FBX 根骨骼缩放。
- Max Distance：最大拉动厘米数，Demo 2.5 cm。
- Axis Scale：辅助骨骼局部方向灵敏度，某一分量设 0 可以禁止该方向。
- Frequency、Damping Ratio：弹簧频率与阻尼；Demo 6、0.8。
- Pull Morph、Press Morph：向外拉与向内压的修形名称。
- Pull Axis：骨骼局部正向拉伸方向，决定 Morph 权重正负。
- Morph Strength：修形强度。
- Draw Pinch Regions：人物条目调试开关，运行时显示点击球。

本版结构可用于脸颊、大腿等其他部位，但每个部位的辅助骨骼与权重仍需在模型中制作；Demo 只制作左右脸颊。

## 声音

每个 Pinch Region 配 Grab Sound、Move Sound、Release Sound。
每项可以配置 Sound Wave/Cue、Volume、Pitch、Min Interval。
Move Sound Distance 决定累计移动多少厘米后播放揉动声音；按住不动不会不断发声。

Demo 使用三个本地合成的轻提示音作为可替换样例，没有附带人物语音。可替换成自己的揉捏反馈音与语音。人物声音不按定时器随机播放。

## 输入与恢复

左键按下优先选择揉捏区域，其次选择 KawaiiPhysics。揉捏超过拖动阈值后松开不触发原 Interactions 点击 Sequence；轻点仍保留点击行为。
按住时使用按下位置的屏幕朝向平面计算拖拽，目标距离限制在 DA 上限内，松开后弹簧回零。
切换人物、进入地图、退出界面或开始 Sequence 交互会清理揉捏状态与修形。
右键横拖继续旋转人物，纵拖不调整相机俯仰。

## 验证

- UHT、MSVC、运行时模块和编辑器模块 DLL 链接通过。
- 8 项自动测试通过，0 失败、0 测试警告。
- 新测试直接加载本次导入的模型、Morph、AnimBP 与 DA，检查点击空处不命中、脸颊命中、最大拉动距离、FBX 缩放下的实际骨骼厘米位移、Morph 联动、另一侧独立、松开回弹和切换清理。
- 安装到实际 MyProject 后，揉捏测试再次通过：1 项通过、0 失败、0 测试警告。
- Blender 中生成了中性、侧拉、按压预览。
- UE Demo 已能启动并初始化相机与菜单。本机 Windows 窗口截图连续超时，因此没有完成人工鼠标操作和 UE 最终材质视觉验收。
- 材质使用原始漫反射贴图的基础材质，不等同于原项目三转二渲染管线。原模型眼睛叠层等细节可继续调整。

## 迁移

DreamBloom 目录包含完整源码与 Resources；Content/WardrobePinch21 包含 Demo 模型、骨骼、动画蓝图、材质、贴图、声音、DA、GameMode 与地图。
源码引擎机器用自己的引擎重新编译，保持 KawaiiPhysics 插件可用。不要使用本机官方引擎 DLL。
仅本机官方引擎/Binaries/Win64 适用于本机 UE5.8.3 BuildId 55116800，包含 DreamBloom 和 DreamBloomEditor 两个模块。
备份：F:\新建文件夹\MyProject\Saved\WardrobeBackup-20261009-Pinch21。
