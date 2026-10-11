# DreamBloom 1.10：KawaiiPhysics 鼠标交互与相机切换音效

已安装工程：`F:\新建文件夹\MyProject`。关闭并重新打开 UE 后加载新插件。此次新增反射字段，不建议用 Live Coding 替代重启。

## 你的正式人物怎么配

1. 场景中的 WardrobeShowcase → Character：挂上人物模型和自己的动画蓝图。
2. 动画蓝图原来的初始动作 → Local To Component → KawaiiPhysics → Component To Local → Output Pose。已有 KawaiiPhysics 的蓝图保留原来的接法即可。
3. 在 `DA_WardrobeCharacters → Characters → 对应人物 → Physics Touch` 勾选 **Enabled**。

只需要这一个新开关。不再配置 Node Tags，也不需要为了物理点击创建 Interactions 碰撞区域、Physics Asset 或给骨骼打 Tag。程序自动发现主动画实例、Linked Anim Instance 与 Post Process Anim Instance 中的 KawaiiPhysics 节点。

Camera Tag / Character Tag 仍用于配对相机与场景人物，与此次删除的物理 Node Tags 是两回事。

前提是 KawaiiPhysics 节点本身已经正常解算：Root Bone / Additional Root Bones 应是裙子、头发等骨骼链，Alpha 有效，骨骼有权重且动画蓝图正在输出。普通动画蓝图不会因为勾选 Enabled 就自动生成布料解算。

## 鼠标操作

- 左键轻点：命中模拟骨骼时施加短促外力；已有的部位 Sequence 点击交互仍可触发。
- 左键按住拖动：如果按下时命中模拟骨骼，就拖动附近裙摆或头发，不再同时旋转人物。拖动后松开不触发部位 Sequence。
- 左键按下时没命中模拟骨骼：沿用原来的横拖旋转人物。
- 右键上下拖动：相机俯仰。
- 中键拖动：平移相机。
- 滚轮：相机缩放。
- 松开鼠标、失去鼠标捕获、Q/E 切换、进入地图、退出界面时，清除本次物理拖拽外力。原有 KawaiiPhysics 的约束与碰撞继续生效。

这里的“刚体检测”实际使用 KawaiiPhysics 的模拟骨骼位置与 PhysicsSettings.Radius。它不是 Chaos 刚体拖拽，也不是拖动节点的 Sphere/Capsule Limits（这些 Limits 是挡住头发/裙摆的碰撞限制体）。根骨骼不受拉动。骨骼间没有碰撞球覆盖的空隙可能点不到，可略增 Hit Padding，或调整节点自身的碰撞半径。

## 可调参数

| 参数 | 用途 | 默认值 |
|---|---|---|
| Hit Padding | 鼠标命中容差，叠加在模拟骨骼的碰撞半径外 | 2 cm |
| Radius | 命中后影响周围骨骼的范围，不是命中球大小 | 30 cm |
| Strength | 轻点推力强度 | 100 |
| Duration | 轻点外力持续时间 | 0.25 s |
| Falloff | 距离衰减，越大越集中 | 2 |
| Cooldown | 轻点推力冷却 | 0.15 s |
| Radial Blend | 向外推与沿鼠标射线推的混合 | 0.3 |
| Drag Stiffness | 拖拽跟手程度 | 12 |
| Max Drag Distance | 相对于按下位置的最大拖拽距离 | 40 cm |
| Max Drag Speed | 每秒最大拉动速度 | 200 cm/s |
| Draw Debug | 点击时显示模拟骨骼命中球；拖动时显示目标线 | 关闭 |

拖拽目标位于按下位置的屏幕朝向平面，移动距离与速度都有上限。最终形变还受节点原有的刚度、阻尼、骨长和碰撞约束影响，不是把裙摆硬锁到鼠标。

## Q/E 相机切换音效

在 `DA_WardrobeCharacters → Characters → 目标条目 → Transition Sound` 配置：

- Sound：Sound Wave / Sound Cue。
- Volume：音量。
- Pitch：音调。
- Min Interval：最小播放间隔。

进入哪个条目就使用哪个条目的音效，在成功开始切换时播放一次。切换进行中再次按 Q/E、重复选择当前条目、目标绑定失败、初次初始化均不会播放此切换音效。Sound 留空则沿用 `DA_WardrobeAudio → Station Switch`。配置了人物条目音效时不会叠加播放全局切换音效。音量仍受全局 Enabled、Master Volume 和并发限制控制；没有全局 Audio DA 时条目音效也能独立使用。

## 已生成 Daniya 测试资源

路径：`Content/WardrobeTouch20`。

- `AB_Daniya_PhysicsTouch`：指定 DaniyaToUe 骨骼，使用 DaniyaToUe_Anim 作为蓝图内部的循环初始动画；31 条裙摆/头发根骨骼链；接入 KawaiiPhysics。
- `DA_DaniyaTouchCharacters`：两个人物／相机条目，Enabled 已勾选，Transition Sound 使用现有 S_UI_Mode。
- `DA_DaniyaTouchMenu`、`BP_DaniyaTouchGameMode`：测试关卡专用配置。
- `L_DaniyaTouchTest`：两个人物、两台相机和灯光。打开后 Play，在人物展示区域拖拽，Q/E 切换。

测试蓝图使用基础物理参数用于验证鼠标交互，没有替代你正式蓝图里的完整防穿模和材质配置。初始动画已在 AnimBP 内，不需要在 Showcase 或人物 DA 配 Idle Animation。

## 验证与范围

- UHT、MSVC、DLL 链接通过。
- 独立测试工程 7 项自动测试通过，0 失败、0 警告，涵盖相机景深、MPC/Sequence、相机切换、射线命中、拖拽限速与释放等。
- 指定 Daniya 模型 + 新 AnimBP 在实际 MyProject 工程中再次执行节点命中、拖拽与释放测试：1 项通过，0 失败、0 测试警告。
- 测试地图重新加载后核对了两个人物、两台相机、动画蓝图和专用 GameMode 绑定。
- 以上为无渲染自动测试；尚未做人工鼠标操作和裙摆最终视觉效果验收。
- 本机项目中部分其他资产仍报 UE5-Main 124/123 版本不兼容；本次模型、骨骼和所选初始动画可以加载。本次没有修改资源的版本号。

## 迁移到另一台电脑

1. 将更新包中的 `DreamBloom` 放入目标工程 `Plugins`，更新源码和 Resources。
2. 修改过引擎源码的电脑用自己的引擎，通过 Rider 的 Development Editor / Win64 编译工程。不要复制本机官方引擎 DLL。
3. 本次测试针对本机 KawaiiPhysics 1.21.0 API。目标电脑保留自己使用的 KawaiiPhysics 插件；本包不替换它。
4. `仅本机官方引擎` 下的 DLL 仅适用于当前官方引擎 BuildId 55116800。正式源码引擎需重新编译。
5. 可选测试资源放在 `可选测试资源/Content/WardrobeTouch20`。它们引用 `/Game/Model/Daniya/Red/DaniyaToUe`、对应骨骼与 `DaniyaToUe_Anim`、`/Game/Wardrobe/Audio/S_UI_Mode`。跨工程建议在 UE 内对测试地图执行 Migrate，让 UE 收集依赖；仅复制这个文件夹不会包含模型和动画。

备份位置：`F:\新建文件夹\MyProject\Saved\WardrobeBackup-20261007-Touch20`。旧人物、地图与正式 DA 未被改写。
