# 脸颊恢复速度与鼠标视线衔接 1.19

继续打开 /Game/WardrobePinch23/L_SingleDirectionPinchDemo。

DA_PinchCharacters → Characters → Pinch Regions → Release → Cheek Recovery Speed：只控制松手后脸颊弹簧的恢复速度倍率，默认2，比1.18更快；1为旧速度，3更快，0.5更慢。不改变按住时灵敏度，也不改变身体/表情Reaction Return Time。Release Frequency与Release Damping Ratio继续控制弹簧基础频率/回摆阻尼。

DA_PinchCharacters → Characters → Cursor Look → Pinch To Look Blend Time：揉捏动作恢复末段与看向鼠标的衔接时间，默认0.35秒。身体恢复默认0.65秒时，松手约0.30秒后开始逐渐加入视线，边恢复身体边看向鼠标。设更大可更早接入（不会超过身体恢复时长）；更小接入更晚、更快。
Follow Speed仍控制视线跟随平滑速度。揉捏松手衔接不再等待脸颊完全静止，也不额外等待Idle Delay；其他普通空闲状态仍保留Idle Delay。

保留原有Body动画表情、自由拖拽、已配置灵敏度、回弹；不恢复PressMorph/ReactionAnimation。现有参数除新参数外保持不变。
编译、10项自动化测试通过，新增验证身体恢复尚未结束时视线已开始过渡；原弹簧回摆、动作表情、方向、鼠标跟随测试通过。尚未完成UE可视窗口人工试玩。
更新后完整退出UE再打开。迁移需要插件及DA配置；不同引擎重新编译插件。备份：Saved/WardrobeBackup-20261010-Blend29。
