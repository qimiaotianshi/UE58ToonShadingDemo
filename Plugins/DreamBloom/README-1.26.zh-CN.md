# DreamBloom 1.26 · 纯骨骼揉捏

捏脸不再需要BlendShape。已移除PinchRegions中的PullMorph、PullAxis、MorphStrength，移除揉捏时的SetMorphTarget、身体反应动画Morph曲线采样，以及SetPinchExpressionCurve编辑器接口。

脸颊形变使用Bone指定的辅助骨骼和蒙皮权重。配置：Bone、可选Anchor Bone、Hit Radius、Max Distance、Axis Scale、Drag Sensitivity；AnimBP的Wardrobe Pinch节点Bone To Modify填写对应骨骼，不勾Reaction Bone。蒙皮权重的渐变范围决定局部拉伸形状，建议逐渐衰减到周围皮肤，避免硬折角。

松手弹簧、Cheek Recovery Speed、BodyLeft/Right/Up/Down、ReactionReturnTime、音效、鼠标视线功能继续保留。Body反应现在只采样骨骼旋转，不采样Morph曲线；需要皱眉/闭眼/嘟嘴时用已蒙皮的面部骨骼制作反应动画，并在AnimBP为这些骨骼接对应Reaction Bone节点。没有这些面部骨骼时，仅有骨骼旋转的部位会反应。

模型里已有Morph资源没有删除；其他Sequence、普通动画或SequenceCharacterStudio可以继续使用它们。揉捏代码既不设置也不清零这些Morph，避免影响其他表情轨道。

升级后原DA中的骨骼、区域和回弹配置保留，已删除字段自动忽略。无需重做原骨骼。插件已更新到MyProject，关闭并重新打开UE生效。

DLL官方UE5.8.3 BuildId55116800，源码引擎移除插件Binaries/Intermediate后重新编译Development Editor / Win64。备份work/wardrobe/BonePinch39/Backup。

保留粉色UI、默认Sequence、物理权重渐变和精简TransitionSound。运行测试覆盖骨骼拖拽/双向反应/力度/回弹/恢复，并检查揉捏不覆盖独立Morph数值。未人工检查新制作的纯骨骼脸部蒙皮视觉质量。
