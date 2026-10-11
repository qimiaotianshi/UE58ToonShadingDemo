# 脸颊受力反应 Demo 1.12

## 开始测试
打开 /Game/WardrobePinch22/L_PinchReactionDemo，点击 Play。左键按住脸颊向左右上下拖动；力度越大，单侧闭眼、眉毛和头/上身倾斜越明显。松手恢复。右键横拖旋转人物、中键平移、滚轮缩放保持原有行为。

本次使用《无标题.blend》的模型，在上一版辅助骨骼绑定上制作动画。原始文件未改动。可编辑文件：桌面/三转二/Daniya_PinchReaction.blend，Action Editor 可选择 A_PinchIdle、A_PinchPull_Left/Right/Up/Down。四个动作第1帧为中性，第16帧为最大受力，第31帧保持；可以继续手工调整这些关键帧。

## DA 配置
打开 DA_ReactionCharacters → Characters[0] → Pinch Regions。
- Reaction Left / Right / Up / Down：四个标准动画资产，同一骨骼。插件采样最后一帧相对第0秒的旋转差值作为 Additive，资产本身不要再设置成 UE Additive，避免重复计算。
- Reaction Right Axis / Up Axis：选择四个动作的组件空间轴，Demo 为 -X/+Z，已针对 FBX 导入坐标修正，使头部朝鼠标方向移动；左右命名以模型轴向为准，镜头朝向不同时可以调整。
- Reaction Strength：身体跟随的最大权重，0关闭；Reaction Exponent：力度响应指数，>1弱拖更柔和。
- Eye Close Morph / Brow Morph：单侧闭眼和眉毛 Morph 名称；对应 Strength 控制最大表情强度。
- Max Distance、Frequency、Damping Ratio：最大拉伸距离与回弹速度/阻尼，沿用上一版。
- Grab / Move / Release Sound：沿用现有声音反馈，可替换。

Demo AnimBP：A_PinchIdle → Local To Component → 五个 Wardrobe Pinch 反应骨骼节点（上半身、上半身3、上半身2、首、頭，勾选 Reaction Bone）→ 两个脸颊辅助骨骼节点（不勾选）→ Component To Local → Output。实际角色可以把自己的 AnimBP 基础姿势接进来，并将 KawaiiPhysics 放在揉捏之后；Demo 本身没有新增 KawaiiPhysics 解算链。

反应仅给这些已配置节点的骨骼叠加旋转，腿和根骨骼不变。拖拽基于抓取时固定平面和坐标，不跟着已经倾斜的头重新计算，避免越拖越偏。表情与身体随同一弹簧恢复。切换角色/开始其他交互会清零。

## 本次范围与验证
这是第一版脸颊受力样例；没有新增大腿揉捏绑定，也没有制作完整复杂的痛苦表演。保留上一版 WardrobePinch21。材质继续使用原贴图加简单材质，尚未恢复原模型完整卡通材质效果。
UHT、C++ 和编辑器模块编译通过；9项自动化测试通过，包含真实导入模型的左右/上下方向、力度、单侧表情、AnimBP头/身体输出与恢复，以及上一版功能回归。Blender预览已检查。未完成 UE 可视窗口中的人工鼠标试玩，预览图来自 Blender。

## 迁移
复制包内 Plugins/DreamBloom 与 Content/WardrobePinch21、WardrobePinch22。Resources 必须一起迁移。不同引擎或修改过源码的引擎请删除插件 Binaries/Intermediate 后，在目标工程重新编译；不要直接使用本包官方5.8.3 DLL。已有对应内容时先备份。
