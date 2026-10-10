# Sequence Character Studio 1.2 · 表情专用（UE 5.8.3）

插件现在只负责在 Level Sequence 中 K BlendShape / Morph Target 和导入音频生成的表情曲线。身体动作使用 UE 官方 Control Rig。旧的插件骨骼编辑界面、KeyBone、AddCharacterTrack 和 BoneFilter 已移除。

## 当前工程示例

打开 Content/SequenceCharacterStudioDemo/L_ControlRigBlendShapeDemo，再打开 LS_ControlRigAndBlendShapes。

- FK Control Rig · 身体：官方原生 UFKControlRig，绑定 DaniyaToUe 的骨架。已给頭、首、上半身、腕_L、腕_R打帧，0～12秒做轻微转头、上身倾斜和双臂摆动。
- BlendShapes · 表情：插件的独立浮点控制轨道，导入 AS_Daniya_SilenceFixed 的20条表情曲线、7000个关键帧；时长11.63秒。
- 音频轨道：对应已有的“达尼亚_这样多累呀”声音。

选择人物后，使用官方 Control Rig 编辑模式选中控制器，用W/E/R编辑；在 Sequencer 开启 Auto Key 或点击通道旁的打帧按钮。FK会控制骨骼的局部变换，移动手腕不会自动带动整条手臂，没有自动锁脚；需要IK时再使用官方带IK的Control Rig蓝图。当前绑定直接保存在Sequence里，无需额外创建一个自定义原生骨骼插件。

示例是普通 SkeletalMeshActor；打开关卡，在编辑器中播放Sequence查看。相机和基础灯光已放在示例关卡里，可以在视口选择相机预览。

## K表情

1. 在场景里放置带 Morph Target 的人物，创建或打开Level Sequence，选中人物。
2. Window > Sequence Character Studio，点击“为选中人物添加 / 打开表情轨道”。
3. 搜索表情名称，拖动或直接输入数值。修改会在当前帧自动打关键帧，范围-1～1。拖动时间轴本身不会写帧。
4. Sequencer展开BlendShapes轨道，使用通道数值或Curve Editor继续调整。

表情面板和表情通道均取消固定0.1步进，使用普通浮点数的连续拖动规则，显示最多6位小数，可以输入0.00125等细小数值。按住Ctrl拖动可进一步细调，键盘输入最准确。表情面板始终自动打帧，与官方身体控制器的Auto Key开关是不同入口。

表情Rig保留参考骨架用于姿势传递，但不会生成任何骨骼Transform控制器。它在最终动画层替换表情曲线，避免覆盖官方FK的身体姿势。不要再用其他动画轨道或每帧蓝图同时覆盖相同Morph。

## 音频表情导入

使用原有Tools > Daniya Audio Animation生成AnimSequence，再在表情面板选择该动画。配置起始显示帧、强度和是否替换区间，点击“导入为可编辑的BlendShape关键帧”。原来的DaniyaLipSync插件不变。

按Sequence显示帧率采样名称匹配的Float曲线，线性插值，保留非整帧的动画尾部。只导入Morph，不导入骨骼。默认替换导入区间的同名表情关键帧，区间外和其他表情保留。整次操作可Ctrl+Z。原动画不修改。

导入后可以逐帧调整表情、删除或补关键帧。重新导入会覆盖相应范围的手调结果，先复制Sequence版本。工具不自动保存、延长播放区间或添加声音，请在官方音频轨道中配置原始声音和起始帧。锁定/只读资源和非法参数拒绝导入。

## 旧版本升级

当前工程的LS_BoneAndExpressionDemo和LS_AudioExpressionEditable已升级：旧示例中的骨骼曲线完整复制到官方FK轨道，表情曲线保留并换成精细数值Section。备份在本次工作目录work/wardrobe/SequenceStudio32/Backup_1.1。

其他旧的混合轨道不会自动丢弃骨骼关键帧。添加表情轨道遇到旧混合Section会拒绝升级；请在旧版本先备份并通过UE官方烘焙/Control Rig流程转移身体动作，再新建1.2表情轨道、重新导入表情。不要直接删除唯一的旧动画数据。纯表情旧Section在点击添加轨道时升级，原始浮点关键帧和切线保留。

## 编辑器接口

- SequenceCharacterTools.AddMorphTrack(Sequence, Component)：绑定指定Mesh Component，创建或复用表情轨道。
- KeyMorph(Sequence, Track, Morph, DisplayFrame, Value)：按显示帧率写入表情浮点关键帧。
- ImportAnimationMorphCurves(Sequence, Track, Animation, StartDisplayFrame, Gain, bReplaceRange)：导入已有动画曲线，返回结果和写入数量。

以上仅编辑器接口。游戏内使用标准LevelSequencePlayer播放保存的Sequence即可。默认Restore State，停止后恢复；可用官方Sequencer更改完成状态。

## 安装/迁移

关闭UE，把Plugins/SequenceCharacterStudio完整复制到目标Plugins目录。Source、Content/CR_SequenceCharacter.uasset都要保留；这个生成Rig资产用于游戏世界播放。更新时移除旧版本的Source后再复制新Source，避免残留已删除的cpp文件。

DLL对应官方UE5.8.3，BuildId55116800。修改源码的引擎或其他BuildId：移除插件Binaries、Intermediate，重新生成工程文件，在Rider选择Editor / Development Editor / Win64编译。不要修改资源版本号。

OptionalDemo只附关卡与Sequence，依赖当前工程的Daniya模型、材质和声音。迁移到其他工程请使用UE的Migrate携带依赖，或用自己的模型新建。插件不依赖DreamBloom、KawaiiPhysics和特定骨架，也不会生成模型缺失的Morph或蒙皮。

## 验证范围

官方UE5.8.3编译通过。自动化覆盖：纯浮点控制器、0.00125细小数值、无效Morph、Section无固定步进、曲线导入的帧率/偏移/强度/非整帧结尾/锁定、官方FK与表情的游戏世界共同求值和实际Mesh形变。示例在保存重开后检查轨道类型、曲线和身体播放。

未验证Shipping打包、其他引擎版本及所有第三方AnimBlueprint/物理解算组合。此次采用自动化检查，未完成可见编辑器界面的截图检查。
