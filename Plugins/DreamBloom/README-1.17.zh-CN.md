# 松手回弹与Body动画表情 1.17

继续打开 /Game/WardrobePinch23/L_SingleDirectionPinchDemo。
保留已配置的拖拽灵敏度（当前工程0.15）和自由方向身体跟随。PressMorph字段、状态及驱动代码已删除；ReactionAnimation字段和读取/兼容代码已删除。已有模型中的历史Press Morph形状资源没有参与运行，无需重导模型。

DA_PinchCharacters → Pinch Regions：BodyLeft/Right/Up/Down动画现在同时提供身体骨骼与表情Morph曲线，斜向拖动按相同方向权重混合两者。每个动画第0秒中性、最后一帧最大反应，普通动画，不设置UE Additive。骨骼/曲线按力度采样，Morph名字需要与人物Mesh一致。
Demo为左右脸分别配置 A_PinchBody_L_* / A_PinchBody_R_* 共8个动画，每个都已含对应单侧闭眼、眉毛、嘟嘴曲线。以后直接编辑Body动画即可，不需要单独表情插槽。

## 松手配置
- Release Frequency：释放时脸颊弹簧频率，默认2Hz；更小恢复更慢。独立于按住拖拽时的Frequency。
- Release Damping Ratio：默认0.45，<1有回摆，越小回摆越明显；1接近无振荡。
- Reaction Return Time：身体与表情平滑回到基础动画的时间，默认0.65秒，可试0.4～1秒。
松手保留当时动作与表情方向，按平滑曲线退回基础姿势，不因脸颊弹簧越过零点而突然切换另一方向动作。身体和表情不跟随脸颊高频来回翻转。重复鼠标捕获释放也不会重新启动恢复计时。
揉捏有位移时松手不再误触普通点击，静止轻点仍保留Interactions。切换角色或其他Sequence交互仍按原行为清除当前揉捏状态。

## 验证
C++与编辑器模块编译、9项自动化测试通过。测试覆盖松手当帧动作保持、下一帧表情/变形保留、脸颊弹簧越过中性产生回摆、最终恢复，以及方向/灵敏度回归。没有完成UE窗口人工试玩。

迁移：插件＋更新DA＋新Body动画必须一起迁移；完整包包含Demo及依赖。修改源码或不同版本引擎请重新编译插件。更新DLL后完整退出UE再打开。旧版及Demo备份：Saved/WardrobeBackup-20261009-Release27。
