# 头和身体方向跟随修复 1.15

1.14只恢复了脸颊自由拖拽，但身体仍从单一动作采样，导致始终朝同一方向。这是实现遗漏。本版恢复四方向身体动作混合，表情继续读取动画文件中的闭眼、皱眉、嘴部曲线。

继续打开 /Game/WardrobePinch23/L_SingleDirectionPinchDemo。任意方向拖拽脸颊：头和身体按左右/上下方向反应，斜向混合。表情按距离力度变化，松手恢复。
DA_PinchCharacters → Pinch Regions：
- Reaction Animation：表情动画（A_Pinch_L/R）。与当前 Mesh 同一 Skeleton，首帧中性，末帧最大表情。
- Body Left/Right/Up/Down：对应四个身体反应动作（已配置 A_BodyPull_*），同一 Skeleton；这些动作只用于骨骼反应，里面的表情曲线不会影响当前表情。
- Body Right Axis/Up Axis：Mesh组件空间判断方向的轴。Demo 为 -X/+Z，已针对导入轴向验证；名字指用于选择动作的轴，不限制拖拽。
- Reaction Strength/Exponent：反应权重和力度响应。

四方向身体动作均为空时，为兼容已有DA仍用Reaction Animation驱动身体，这种配置身体方向固定。需要身体跟随鼠标时必须配置四个Body动作。Demo已全部配置，无单向拉伸限制。无需恢复单独闭眼/眉毛配置。
角色左右命名沿用模型骨骼；屏幕位置会随镜头及角色朝向变化。

编译与9项自动化测试通过，新增验证实际头部向正/负拖拽方向移动、向上/下倾斜，而非仅检查变形有无发生。未完成UE可视窗口人工鼠标试玩。
更新包含插件以及DA_PinchCharacters和四个新身体动画。若编辑器已经打开，需要完整退出后重新打开，已加载的DLL不会因磁盘更新自动替换。不同引擎需重新编译。
备份在 Saved/WardrobeBackup-20261009-BodyDirection25，包含更新前插件和整个WardrobePinch23。
