# DreamBloom 1.21：角色默认循环Sequence

## 配置

DA_WardrobeCharacters → Characters → 每个人物 → Default Sequence：
- Default Sequence：默认展示用的Level Sequence，进入当前人物的展示状态后无限循环。

场景中的CharacterTag仍用来识别AWardrobeShowcase，CameraTag仍识别配套相机。默认Sequence配置在每个人物条目里，没有新增Director。

创建Sequence时，把场景中正确的WardrobeShowcase人物加入Sequencer，在它的Character骨骼网格组件上添加Animation轨道和动作。播放起止范围决定循环长度；把首尾姿势做一致可避免循环跳变。可以添加BlendShape轨道、声音和其他场景轨道。人物直接使用Sequence内已经配置的引用，默认播放不进行绑定覆盖，也不额外调整播放速度。组件绑定需位于人物Actor的子级。不要绑定一个另行生成的副本人偶，却期待展示人物会同步。

默认Sequence的Camera Cuts不接管视角，展示镜头继续使用场景相机与QE切换。需要动态背景、灯光和相机参数时可以使用普通轨道；不要在默认Sequence里直接覆盖人物Actor的Transform，否则会与右键横拖旋转竞争。默认Sequence中的音频每次循环也会重播，只有想重复的声音才放进去。

## 运行流程

当前人物启用展示 → 从默认Sequence播放起点无限循环。
点击有效Interactions部位 → 停止并恢复默认Sequence写入的状态 → 播放随机交互Sequence一次。
交互自然结束、超时或CancelInteraction(true) → 恢复交互状态 → 从头重新播放默认Sequence，不接着先前时间续播。
QE切换人物、禁用展示、加载/退出 → 停止默认和交互Sequence；目标人物到达并启用后开始自己的默认Sequence。

这次不增加动作混合时长；默认与交互是两次独立Sequence播放。首尾姿势需要连续时，在Sequence里制作衔接段。交互使用原有Max Interaction Duration限制，较长交互需相应增大。

无效交互资源/绑定不会停止已有默认动作。默认Sequence留空时不播放默认Sequence，保留现有AnimBP状态供旧工程兼容。默认Sequence和交互都强制RestoreState，防止KeepState轨道在切人后留下旧状态。播放期间不要由其他蓝图或自动播放的LevelSequenceActor同时驱动同一人物。

## 当前工程示例

/Game/WardrobePinch23/L_SingleDirectionPinchDemo
/Game/WardrobePinch23/DA_PinchCharacters → Characters[0].DefaultSequence
/Game/WardrobePinch23/LS_DefaultDisplay

已用原有A_PinchIdle建立1秒默认展示Sequence，绑定示例人物的Character组件。默认动作由该Sequence驱动；AnimBP资产未删除。

本功能不要求AnimBP内继续配置默认Idle，但KawaiiPhysics、Wardrobe Pinch和鼠标视线跟随本身依赖动画节点。普通Sequence Animation轨道可能替换主AnimInstance，因此在需要物理/揉捏叠加的角色上应使用合适的Sequencer Slot配置，或把后置处理放在Post Process AnimBP中，并确认Sequence轨道没有跳过Post Process AnimBP。Control Rig轨道和第三方动画蓝图组合也需按角色检查。不能仅删除原AnimBP的全部节点并期待这些效果继续存在；本更新没有自动改写用户的物理/揉捏动画图。

## 蓝图调试

AWardrobeShowcase新增StartDefaultSequence / StopDefaultSequence / IsDefaultSequencePlaying / GetDefaultSequencePlayer。正常游戏流程会自动调用，无需自己写Tick播放。GetDefaultSequencePlayer可以查看当前时间和播放状态。

日志搜索WARDROBE_DEFAULT_SEQUENCE、WARDROBE_INTERACTION_SEQUENCE、WARDROBE_STATION_ARRIVED可观察默认开始、交互和切人。Sequence人物绑定失败时先检查Sequencer的Actor绑定是否是当前关卡，。

## 更新和迁移

关闭UE，复制更新包的Plugins/DreamBloom到工程Plugins，保留其他插件。DLL对应官方UE5.8.3、BuildId55116800。修改源码引擎：保留Source与Resources，移除插件Binaries和Intermediate，重新生成项目文件，用Rider编译Development Editor / Win64。

OptionalDemo包含新默认Sequence和配置后的DA，依赖原WardrobePinch23模型、动画、关卡与其他原素材。在另一个工程中直接配置自己的DA即可；需要示例时用UE Migrate带上依赖。覆盖DA会替换该DA中的自定义配置，请自行决定是否使用示例DA。

更新前备份：work/wardrobe/DefaultSequence34/Backup，包含原插件和原DA_PinchCharacters。

## 验证

UE5.8.3编译通过。11项自动化测试覆盖新默认循环、重复启用不重播、绑定失败保留默认、交互自然结束/超时/取消后重播、KeepState恢复、切人禁用/EndPlay，以及原相机、互动、Kawaii、揉捏和视线功能。新测试用真实LevelSequencePlayer与实际Actor Transform验证竞争和恢复；测试没有使用渲染视口。

示例DA与Sequence已保存。尚未人工试玩、验证Shipping打包或所有第三方AnimBP/ControlRig组合。默认动作切换不提供通用动画姿势交叉混合。

## 1.21更新

默认Sequence的人物绑定标签、播放速度字段与运行时代码已删除；DA仅保留Default Sequence。Interactions原有配置保持不变。
