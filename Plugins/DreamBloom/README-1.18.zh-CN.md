# 空闲时看向鼠标 1.18

继续运行 /Game/WardrobePinch23/L_SingleDirectionPinchDemo，空闲0.8秒后移动鼠标，头、颈和眼睛平滑看向鼠标所在方向。基础Idle保持在AnimBP内。
揉捏、揉捏回弹/动作恢复、KawaiiPhysics交互、Sequence互动、相机站点切换期间暂停跟随，逐渐回到中性。交互结束后再等待Idle Delay恢复。鼠标离开游戏视口或按住鼠标按钮时暂停。运行输入由PlayerController鼠标坐标获取。

## DA配置
DA_PinchCharacters → Characters → Cursor Look：
- Enabled：开启跟随，其他人物默认关闭，配置骨骼后自行开启。
- Anchor Bone：用于确定鼠标目标深度的头部骨骼，Demo为頭。参考骨架坐标作为稳定锚点，避免头转动影响下一帧目标产生反馈。
- Forward Axis / Up Axis：Mesh组件空间的前方和上方，Demo +Y/+Z；UE常规角色可能是+X/+Z，需要按模型调整。鼠标射线上的目标点位于角色前方，避免射线穿过头部导致方向翻转。
- Bones：每个参与骨骼及权重。Demo 首0.25、頭0.5、目_L与目_R各0.25；双眼是两条分支，不能把两个眼睛权重相加理解为同一链。沿同一父子链权重之和建议不超过1。
- Max Yaw/Max Pitch：总体左右/上下限幅，默认30°/20°。
- Follow Speed：默认4，越小越慢。
- Idle Delay：默认0.8秒，交互结束后延迟。

## AnimBP配置
Demo改用 AB_DaniyaCursorLookV2：基础动作→组件空间→原身体反应节点（首、頭）→新增目_L/目_R两个 Wardrobe Pinch 节点（Reaction Bone勾选）→原脸颊变形节点→本地空间→输出。
跟随叠加在配置了Reaction Bone的节点上；DA填骨骼名后也要在AnimBP添加对应节点。其他人物可沿用相同方法，节点Bone To Modify填写自己的骨骼名。FBX会把点改成下划线，请以UE Skeleton骨骼列表为准。推荐将KawaiiPhysics放在骨骼跟随之后。
当前实现使用骨骼旋转；只有贴图/Morph而没有眼球骨骼的模型需要额外绑定，不能直接使用这个骨骼方案。较大基础动作或坐姿改变头部位置时，需要调整锚点或后续扩展，当前Demo使用站立参考姿势。

## 保留功能与验证
保留自由揉捏、Body动画曲线表情、0.15灵敏度（实际工程配置）及松手回弹，不恢复PressMorph/ReactionAnimation。
编译与10项自动化测试通过，包含实际AnimBP头/颈/眼睛输出、反向鼠标改变视线、揉捏期间暂停、结束后恢复和光标失效恢复，以及原功能回归。未完成UE窗口人工鼠标试玩。
更新后完整退出UE再打开，DLL无法仅通过覆盖磁盘文件替换已加载版本。
迁移包包含插件、更新DA/AnimBP/关卡及Demo依赖；不同引擎需重新编译。更新前备份：Saved/WardrobeBackup-20261010-CursorLook28。
