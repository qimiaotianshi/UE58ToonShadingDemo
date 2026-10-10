# DreamBloom 1.24 · Sequence切换时物理渐变

## DA配置

DA_WardrobeCharacters → Characters → Sequence Physics Blend：
- Physics Fade Time：淡出与淡入共用的时间，默认0.2秒；填0立即切换。
- Reset Physics On Sequence Switch：默认开启，新Sequence已开始提供姿势后，在下一次Actor Tick重置动画实例物理，包含Post Process AnimBP。关闭可保留模拟状态。

淡出采用平滑曲线：当前权重→0，旧Sequence继续播放；到0后停止/恢复旧Sequence并播放新Sequence；再从0→1。交互自然结束使用Pause At End保留末帧，先淡出再恢复默认Sequence。淡出期间禁止再触发交互；QE、退出等取消会清理待切换任务。

默认Sequence仍使用自身人物引用，未恢复人物绑定标签和速度字段。

## 权重传入动画蓝图

在需要控制Kawaii的AnimBP中：
1. 新建Float变量SequencePhysicsWeight，默认1。
2. Event Blueprint Update Animation → Get Owning Actor → Cast To WardrobeShowcase → Get Sequence Physics Weight → Set SequencePhysicsWeight。Cast失败时设为1，以兼容编辑器预览或其他Actor。角色是Actor，不能使用Try Get Pawn Owner。
3. AnimGraph选择KawaiiPhysics节点，Alpha Input Type设Float；将Alpha暴露为引脚。
4. 把变量SequencePhysicsWeight连到每个需要渐变的KawaiiPhysics节点Alpha。原本还有自己的Alpha时，使用原Alpha × SequencePhysicsWeight。
5. Compile并保存。不要在AnimGraph工作线程里直接查询Actor；EventGraph先缓存到Float变量。

GetSequencePhysicsWeight是人物上的BlueprintPure接口，范围0～1。IsSequenceSwitching用于调试；StartDefaultSequence等播放接口保持原功能。仅连接节点的Alpha，不要把权重乘到外力或阻尼上来代替Alpha。

代码不会自动改写用户现有AnimBP，安装插件后必须完成上述连接，否则节点仍以原Alpha运行，无法得到淡入淡出。

## 保证Sequence播放时还经过Kawaii

优先使用主AnimBP的Slot路径：
- Sequence中人物Animation段的Force Custom Mode关闭，Slot Name填写DefaultSlot（或你自定义的同名Slot）。
- AnimBP中基础Pose → Slot(DefaultSlot) → Local To Component → KawaiiPhysics → Component To Local → Output Pose。
- Slot名字必须与Sequence段一致，所有默认和交互Sequence都使用相同路径。Wardrobe揉捏/视线节点可以放在Slot之后、Kawaii之前。基础Pose可以是Ref Pose；默认动作由Sequence提供，不需要在AnimBP重新配置默认Idle。
- UE会把Sequence动画送入Slot作为动态Montage，保留主AnimBP实例；不要在Sequence内再改AnimClass或AnimationMode。

如果Control Rig或其他设置需要强制接管主实例，使用Post Process AnimBP：Input Pose（Sub-Input） → Component Space → Kawaii → Local Space → Output。指定到SkeletalMesh的Post Process Anim Blueprint，并确认Sequence动画段未启用Skip Post Process Anim BP（具体UI名以引擎为准）。同样在Post Process AnimBP EventGraph读取Owning Actor权重；不要依赖被接管的主AnimBP中的变量。

只有在实际输出Pose经过Kawaii节点时，Sequence动作才能享用物理。权重渐变本身不能解决节点被完全绕过的问题，也不能自动混合两条Sequence的身体姿势。首尾身体动作跨度过大仍需在Sequence里制作衔接姿势。

## 建议参数

先试0.2秒。还明显抖动时可试0.3～0.4秒，保持Reset Physics开启。Kawaii节点可启用自身Warm Up相关设置，但不要每帧调用ResetDynamics。

## 安装与验证

关闭UE后更新Plugins/DreamBloom并重开。DLL官方UE5.8.3 BuildId55116800；源码引擎移除插件Binaries/Intermediate后用Rider重新编译Development Editor / Win64。保留Source/Resources。

保留少女粉UI。备份在work/wardrobe/PhysicsBlend37/Backup。

编译通过，11项原功能及新增状态流程测试通过；覆盖0.2秒淡出中点、0权重切换、0.2秒淡入、自然结束/取消回默认和禁用后不启动待播Sequence。未人工验证用户模型的物理视觉效果或自动修改其动画图。

## 1.24更新

淡出/淡入两个字段合并为Physics Fade Time。旧的两个字段不再使用；升级后在DA重新填写这个共用时间，默认0.2秒。
