# 揉捏灵敏度 1.16

新增 DA_PinchCharacters → Characters → Pinch Regions → Deformation → Drag Sensitivity。
默认0.25：鼠标拖动距离转换为揉捏距离时乘以四分之一，脸颊变形、表情动画与四方向身体动作同步降低响应。达到最大效果需要原来的四倍拖动距离。最大脸颊幅度保持不变。

推荐先试0.25；仍敏感改0.1～0.15；太迟钝改0.4～0.5；1恢复旧灵敏度。各部位独立配置。
Max Distance仍控制脸颊最大变形幅度，不用为了降低灵敏度把它调大。Reaction Strength只控制动作和表情的最大强度；Reaction Exponent>1让轻微拖动时的动作更柔和。

保留自由方向拖拽、身体四方向跟随与动作文件控制表情。重新抓取正在恢复的脸颊不会把已存在的偏移重复乘灵敏度。
继续运行 /Game/WardrobePinch23/L_SingleDirectionPinchDemo。更新插件后完整退出UE再打开。
编译与9项测试通过，新增验证1cm鼠标平面位移只造成约0.25cm变形、轻微位移表情低于0.1，同时长距离拖拽仍能达到完整反应。
迁移包含插件和完整Demo Content；不同引擎请重新编译插件。旧版与DA备份：Saved/WardrobeBackup-20261009-Sensitivity26。
