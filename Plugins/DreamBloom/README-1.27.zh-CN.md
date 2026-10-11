# DreamBloom 1.27：使用Body动作中的痛苦表情

脸颊揉捏仍使用骨骼蒙皮；PullMorph、PullAxis、MorphStrength未恢复，DA不增加任何新的表情配置。

Body Left / Right / Up / Down动作文件内的表情曲线恢复读取，按鼠标方向与揉捏力度采样，并与身体动作使用同样的方向混合。第0秒为中性，动作末尾为最大反应。动画中的Float曲线名必须与Mesh已有Morph Target名一致；模型没有同名Morph时自动跳过。曲线写在动作文件里即可，无需逐项配DA。

Demo使用DB_EyeClose_L/R、DB_BrowPain_L/R、DB_MouthPout。松手后按Reaction Return Time平滑恢复表情与身体，脸颊仍按自身弹簧/恢复速度回弹。

该方案不要求制作脸颊拉伸Morph，但仍需要模型具备动作文件使用的闭眼、皱眉、嘟嘴Morph。如果使用纯骨骼表情，在Body动画里K面部骨骼，并为相应骨骼添加Reaction Bone节点。

当前工程：/Game/WardrobePinch23/L_SingleDirectionPinchDemo。插件更新后完整重启UE。

官方UE5.8.3 BuildId55116800。源码引擎删除插件Binaries/Intermediate后在Rider编译Development Editor / Win64。备份work/wardrobe/BodyExpression41/Backup。

保留默认Sequence、物理淡入淡出、粉色UI和精简DA。不要让其他轨道同时覆盖相同表情Morph。未新增模型/动画资产，也未恢复旧的SetPinchExpressionCurve编辑器接口；直接使用UE动画曲线编辑器制作表情。
