# 自由拖拽与动作表情 1.14

本版保留1.13的动作文件表情，撤回单向拖拽限制。左右、上下、斜向拖动均恢复变形；Axis Scale再次生效。Stretch Direction字段和方向投影/反向归零代码已删除。

继续打开 /Game/WardrobePinch23/L_SingleDirectionPinchDemo 测试（保留资产路径，避免重配）。该地图名字来自上一版，实际行为已经恢复自由拖拽。
DA_PinchCharacters → Pinch Regions：每部位继续填写一个 Reaction Animation，闭眼、皱眉、嘴部和身体反应从动作骨骼/同名Morph曲线采样；力度取拖拽距离，不区分方向。动作第0秒中性、末帧最大受力，不要设置UE Additive。当前一个动作的身体倾斜方向由该动作决定；自由拖拽不会自动改变动作朝向。

原四方向动作配置没有恢复；本次只撤回单向限制。Max Distance控制最大拉伸，Axis Scale控制各轴变形比例，Frequency/Damping Ratio控制恢复。Grab/Move/Release Sound沿用。
继续沿用1.13模型、AnimBP、A_Pinch_L/R及Blender源文件，未重做模型或表情关键帧。完整曲线和骨骼配置细节见包内《动作表情使用说明》，其中关于单向限制和Stretch Direction的描述已由本说明替代。

编译通过；9项自动化测试通过，包括反向和垂直拖拽产生变形/动作反应、动作闭眼和嘴部曲线、力度变化和恢复。没有完成UE窗口人工试玩。

迁移包包含Plugins/DreamBloom及Content/WardrobePinch21、WardrobePinch23；已有相同1.13资源只需更新插件。修改过源码或不同版本引擎，需要在目标工程重新编译插件。旧版插件备份：Saved/WardrobeBackup-20261009-FreeDrag24。
