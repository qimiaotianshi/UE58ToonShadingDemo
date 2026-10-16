DreamBloom 1.36 · 手脚 IK 拖拽与身体反馈

在 DA_WardrobeCharacters 的 Characters 每个人物条目中新增 Limb Drags。
当前演示使用 /Game/WardrobeFullBodyPinch42/DA_FullBodyCharacters，已经配置左手、右手、左脚、右脚。
运行 /Game/WardrobeFullBodyPinch42/L_FullBodyPinchDemo，在人物显示区域按住鼠标左键拖动手腕或脚踝；松手平滑恢复。右键横拖旋转人物，中键平移，滚轮缩放。

每个 Limb Drags 条目：
- Label / Enabled：名称与启用开关。
- Upper Bone / Joint Bone / End Bone：上臂—肘—手腕，或大腿—膝盖—脚踝。必须使用真正带蒙皮权重的变形骨骼。允许三者之间有扭转/辅助骨骼，但必须属于同一父子链。
- Foot：脚部勾选，暂时接管同名脚踝的 Wardrobe Foot IK；松手逐渐恢复贴地。
- Hit Radius：点击命中球半径（cm），演示手 8、脚 10。Hit Offset 为可选的骨骼/Socket 局部偏移；演示为 0。带缩放的骨骼会缩放这个偏移，优先先调整半径。
- Max Distance：鼠标能拉动的最大距离（cm），演示手 35、脚 30。IK 另外限制在手臂/腿真实长度内，不拉长骨骼。
- Drag Sensitivity：默认 0.65，降低后同样鼠标距离只产生较小的拖动。
- Follow Time：默认 0.08 秒，跟手平滑时间。0 为立即跟随。
- Return Time：默认 0.45 秒，松手后 IK 与身体反馈返回当前默认 Sequence 的时间。0 为立即恢复。
- Body Left / Right / Up / Down：四个同 Skeleton 的 AnimSequence。按抓取时屏幕方向选择/混合；横纵斜拉会混合两个方向。
- Reaction Strength：默认 0.65，演示 0.5，整体身体反馈权重。
- Draw Limb Drags：在人物条目上开启后画出青色命中球和黄色目标线，用于调命中范围。

四方向动作的制作规则：
动作第一帧为中性姿势，最后一帧为最大拉扯反馈；中间可用曲线安排自然过渡。程序按当前拉扯距离采样动作时间，再将“相对第一帧的变化”叠加到仍在播放的默认 Sequence 上。
可以在动作内 K 头、颈、胸腰、肩臂和表情曲线。手脚最后由 IK 保持在拖拽目标，避免身体倾斜把被抓住的端点带离鼠标。不要在这些动作中 K 角色整体位移来模拟移动玩法；这是原地姿势交互。
演示复用了已有 A_Belly_* 四方向反馈动作，四方向资源引用按原动作的轴向约定映射到当前正面相机的屏幕方向；可以给每只手/脚替换成专用动作。换相机朝向或换模型后请按屏幕方向预览自己配置的四个动作。
表情仍可以使用动作内已有的表情曲线；本功能没有新增捏脸 BlendShape 变形。

当前模型的骨骼配置：
左手：腕_L → ひじ_L → 手首_L；右手：腕_R → ひじ_R → 手首_R。
左脚：足D_L → ひざD_L → 足首D_L；右脚：足D_R → ひざD_R → 足首D_R。
不要改成 MMD 无权重的 足IK、足首_L/R 控制骨骼。

动画蓝图连接顺序：
默认 Sequence/Slot → 原揉捏节点 → Wardrobe Release Blend → Wardrobe Limb Reaction → Local To Component → Wardrobe Foot IK → Wardrobe Limb IK → Component To Local → KawaiiPhysics → Output Pose。
当前 AB_FullBodySequencePinch 已配置。其他人物 AnimBP 可在编辑器 Python 调用：
unreal.WardrobePinchEditorTools.add_limb_drag_nodes(unreal.load_asset('/Game/你的动画蓝图'))
此工具要求原图已有 Wardrobe Release Blend 和 Wardrobe Foot IK；会在正确的位置各插入一个节点并编译。自建图可手动添加这两个节点。两个节点从拥有它们的 AWardrobeShowcase 的当前 DA 条目读取配置，不需要蓝图 Tick 传入鼠标位置。
使用 AWardrobeShowcase 场景人物及已有 Character Tag/Camera Tag 配对；本功能不增加新的 Tag。

输入与恢复：
手脚命中优先于脸/身体揉捏和 KawaiiPhysics；每次只拖一个部位。手脚上的点击由拖拽玩法消费，不会同时触发原 Interactions Sequence。相机切换/进入交互/禁用人物时清理拖拽；松手或窗口失去鼠标捕获时开始恢复。原脸颊弹簧、摸脸、音频和默认 Sequence 机制继续使用原配置。
地面需要阻挡 Foot IK 的 Trace Channel。脚拖动时按拖动目标控制，松手恢复到有地面补偿的默认姿势。本功能限制肢体伸长，没有实现角色移动、攀爬或全身物理布娃娃。

迁移：
合并 Plugins/DreamBloom。若另一台电脑已经有自己的模型/DA/AnimBP，可以只更新插件，在自己的 DA 加 Limb Drags，在自己的 AnimBP 加两个节点。
本增量包 Content 只含当前演示修改过的 DA_FullBodyCharacters 和 AB_FullBodySequencePinch，它们依赖原模型、默认 Sequence 和 A_Belly_* 动作；完整演示需要从 UE 内容浏览器迁移 DA 和 AnimBP 的依赖。合并前保存自己的 DA 配置。
DLL 是本机官方 UE5.8 编译产物。自改源码引擎请用包内 Source 在目标工程对应引擎上重新编译，不要混用旧 DLL。
