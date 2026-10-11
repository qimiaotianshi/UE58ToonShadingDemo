# 单向揉捏与动作表情 1.13

打开 /Game/WardrobePinch23/L_SingleDirectionPinchDemo 并 Play。按住左键拖拽脸颊：沿部位配置方向向外拉增加力度，往回拖减弱，反向和垂直方向不产生新的揉捏。松手弹簧恢复。右键横拖旋转、中键平移、滚轮缩放沿用。

## DA 配置
DA_PinchCharacters → Characters → Pinch Regions：
- Reaction Animation：每部位一个普通 AnimSequence，和人物同一个 Skeleton。第0秒中性，末帧最大受力；在动作里 K 头、颈、身体和面部表情。力度映射到动画时间，按动作原有过渡采样，随后以差值叠加到基础姿势。请不要再将资产设置成 UE Additive。
- Stretch Direction：Mesh 组件空间的允许方向，Demo 左脸 +X，右脸 -X；根据自己模型轴向设置，任意非零向量均可。游戏抓取时将方向投影到屏幕所在平面，所以角色旋转后仍按人物自身方向拉伸。方向几乎正对镜头时无法可靠投影，该部位不会开始揉捏。
- Max Distance：达到最大力度的拉伸距离，单位 cm。Reaction Strength：反应幅度；Reaction Exponent：力度响应指数。
- Bone / Anchor Bone / Offset / Hit Radius：原有变形骨骼与点击区域。Pull Morph / Pull Axis 可保留为脸颊形状修正；Demo 不再使用 Press Morph。Axis Scale 是历史字段，本版单向模式不用它。
- Frequency / Damping Ratio：力度及回弹平滑；Grab/Move/Release Sound：原有声音反馈。

旧 Reaction Left/Right/Up/Down、Reaction Right/Up Axis，以及单独 Eye Close/Brow Morph 配置和对应代码已移除。旧 DA 不会自动猜测新的动作及方向，请手动填 Reaction Animation 和 Stretch Direction，或参考新的 Demo DA。1.12插件已备份到 Saved/WardrobeBackup-20261009-SingleDirection23。

## 如何在动作文件中 K 表情
面部用 Morph Target 时，在 Blender Shape Keys 中 K 闭眼、眉毛、嘴部等，导出动画后确认 UE 动画编辑器 Curves 中存在同名曲线；也可直接在 UE 动画曲线编辑器里 K。运行时自动读取与 Mesh Morph 同名的动画曲线，不用逐个在 DA 配置。Demo 曲线：DB_EyeClose_L/R、DB_BrowPain_L/R、DB_MouthPout。
本模型 FBX 导入漏掉部分曲线，已经将 Blender 中对应的关键帧数值写回 A_Pinch_L/R 动画资产。曲线完全保存在动画文件里，游戏运行无额外曲线文件依赖。
面部用骨骼动画时，需要将相关面部骨骼加入 AnimBP 的 Wardrobe Pinch 节点并勾选 Reaction Bone，和头/身体节点一样。目前这些节点叠加旋转；需要面部平移/缩放的复杂骨骼绑定尚未实现。Demo 使用 Morph 曲线完成表情。
基础 Idle 保留在 AnimBP 内。AnimBP 的五个身体 Reaction Bone 节点后接两个脸颊变形节点，KawaiiPhysics 可放在后面。

## Blender
桌面/三转二/Daniya_SingleDirectionPinch.blend：A_Pinch_L/R 为身体动作，Face_Pinch_L/R 为同一揉捏的表情动作（Blender 将骨架与 Shape Keys 分别存放 Action，UE 已合并到一个 AnimSequence）。中性1帧、中间16帧、最大31帧，可继续调整。原始无标题.blend未修改。

方向判断使用抓取时的固定鼠标平面、固定骨骼轴，避免被动画移动的头部反复改变力度。重新抓取尚在恢复的脸颊时继承剩余偏移，减少跳变。

## 验证与范围
C++与编辑器模块编译通过，9项自动化测试通过：同向受力、反向归零、垂直拖动无反应、向回拖减弱、动作嘴部/闭眼曲线、身体与头朝拉动方向、松手和禁用恢复，以及旧功能回归。预览为Blender渲染，未完成UE窗口人工鼠标试玩。材质继续使用简单贴图材质。

迁移包复制 Plugins/DreamBloom、Content/WardrobePinch21（材质与声音依赖）和 WardrobePinch23。修改源码引擎/不同版本请删插件Binaries与Intermediate后用目标工程重新编译。需要先备份目标工程。
