# V2.1.1：修复特定音频的末尾空帧

音频 `/Game/Model/Daniya/Red/Daniya_当有人握住我的手` 的实际长度约为 13.035102 秒。MetaHuman 的完整音频范围为 391 帧，但实际求解结果只有第 0–389 帧包含控制数据，第 390 帧的整个 Map 为空。求解返回成功。

此前插件要求每一帧必须包含 `CTRL_expressions_jawOpen` 等控制，因此遇到最后这个空帧就报错。它不是模型缺少 jawOpen Morph，也不是用户主动排除了帧；报错中的 Reprocess without excluded frames 在这个场景下不准确。

不同音频时长在音频特征提取、动画重采样和 Performance 帧范围换算时，可能产生少量帧数差异。该音频触发了这一边界情况，其他音频未必触发。重新处理相同音频也可能重复出现。

## 本次修复

完整音频求解的最后最多两帧，如果整个控制 Map 为空且距离音频末尾不超过两帧，则在转换动画时沿用最后一个有效控制帧。保持帧数和时间位置，再使用原有首尾归零及 V2.1 静音闭嘴规则。

口型和表情转换共用该逻辑。原始 Performance 数据保留，不修改模型、音频文件或 MetaHuman 引擎代码。

中间空帧、只缺部分控制名、长段尾部空帧仍会报错，不会把真正损坏的结果全部填零后伪装成成功。

## 使用

插件版本为 2.1.1。安装更新后完整重启 UE，使用原来的音频、模型和 Generate 按钮即可，无需剪音频或修改 Controls Map。

本次对应的测试输出动画命名为 `AS_Daniya_Hand_TailFixed`，放在 `/Game/DaniyaTest/Generated`。


# Daniya Lip Sync 2.1：说话停顿时平滑闭嘴

本次修复了 MetaHuman 在无声或低音量停顿中仍输出张嘴权重的问题。新的一键生成流程默认读取 SoundWave 的实际 PCM 音频，根据音量压低五个说话口型。

## 更新后怎么用

1. 保存工作并完整重启 UE。
2. 打开 **Tools → Daniya Audio Animation**。
3. 选择音频、DaniyaToUe 和原来的强度设置。
4. 在 **Silence** 分类中保持 **Close Mouth In Silence** 勾选。
5. 点击 **Generate** 重新生成。旧动画不会被自动修改，重名会生成新的后缀版本。

蓝图的 **Generate Daniya Animation From Audio** 节点也增加了同样参数。旧节点实例如果没有显示新引脚，右键刷新节点（Refresh Node），重新编译蓝图；必要时重新放置该节点。已有口型、表情参数仍可照旧使用。

| 参数 | 默认值 | 调节方法 |
|---|---|---|
| Close Mouth In Silence | true | 静音闭嘴开关；关闭恢复 V2 原来的生成行为 |
| Silence Threshold Db | -42 | 音量低于该 RMS dBFS 阈值时闭嘴。提高到 -38/-35 会更容易闭嘴；降低到 -48/-52 可保留更轻的说话声 |
| Mouth Close Time | 0.08 秒 | 从完整口型收回零的渐变时间；越短闭嘴越快 |
| Mouth Open Time | 0.02 秒 | 恢复说话时的开嘴渐变时间 |

先用默认参数试听。只有当你的新音频背景噪声较大或录音电平很低时，再调整阈值。这个阈值依据音频文件本身的音量，不是系统扬声器音量，也不随 Sequencer 音轨的播放音量变化。

## 为什么不会每个字都突然闭嘴

算法每 10 毫秒分析一次音量，使用 20 毫秒 RMS 窗口；对非常短的字间间隙保留约 30 毫秒，再渐变闭嘴。开口前约 20 毫秒提前检测，减小辅音起始被切掉的情况。阈值上方还有 6 dB 渐变区间，避免刚过阈值就突然开到最大。

真正的停顿会在短暂保持和闭嘴过渡后到达**精确零权重**。声音很低的整个片段也会被当成停顿；轻声对白被误压低时应降低阈值。背景音乐持续存在时，音量检测不能可靠地区分人声与音乐，建议使用单独对白音轨。

五条口型曲线在原有平滑后再应用音量约束，防止相邻帧平滑把张嘴权重带回静音段。眉毛、眨眼和嘴角表情保持原样，不会因为说话暂停而整张脸突然恢复中性。

## 已经生成的动画怎么修

新增编辑器蓝图节点 **Close Daniya Mouth In Silence**，输入原始完整 SoundWave、目标 AnimSequence 和上述阈值/时间参数，输出成功 bool 与 Error。

建议先复制旧动画，再对副本调用一次，成功后 Ctrl+S 保存。此节点只修改 `あ、い、う、え、お` 五条曲线，保留其他曲线、骨骼轨道、动画长度以及 Additive 设置，不自动保存。

必须使用与动画时间从 0 对齐的原音频。截取片段、变速或有偏移的音频不能直接用于修正完整动画。

调整参数后请从原动画重新复制再处理；不要在同一个已经闭嘴处理过的副本上反复应用，否则衰减会累积。

V1 的 Create/Bake/Mouth Modifier 节点保持原行为；需要静音处理时，在其后接新增的 Close 节点。V2 的一键生成节点和工具窗口已自动包含这一步。

## 与身体动画叠加

本次修改没有自动改变动画的 Additive 设置。你已经设为 Local Space / Skeleton Reference Pose 的旧表情动画，可以复制后调用 Close 节点，设置会保留。新生成的动画如果要与身体动作一起播放，仍按之前的方法设置为 Additive。

## 安装和源码

已用原来的音频生成两份新动画，位于 `/Game/DaniyaTest/Generated`：

- `AS_Daniya_SilenceFixed`：开启静音闭嘴的修复版。
- `AS_Daniya_SilenceBaseline`：关闭静音闭嘴的对照版。

两份均包含表情。它们可在相同时间点比较，原有资源保持不变。

本次通过了编辑器、Win64 Development、Shipping 的插件编译；自动化测试覆盖纯静音、低电平停顿、短字间间隙、正常语音、反相立体声，以及真实音频。实际一键生成测试确认开关有效，且原有表情曲线不变。

当前项目插件更新为 **2.1.0**。备份包包含完整 C++ 源码与当前 UE 5.8.3 Win64 编译文件。其他项目安装时将插件文件夹放到项目 Plugins 目录，使用不同 UE 构建时重新编译。


# Daniya Lip Sync 2.0：输入音频，生成口型与表情动画

适用：当前 UE 5.8.3 Windows 项目。插件已放入 `MyProject/Plugins/DaniyaLipSync`。使用过程不需要 Python，也不需要自己创建 MetaHuman Performance、Controls Map、BuildWeights、循环或曲线写入蓝图。

## 先做一次

保存当前工作，关闭并重新打开 UE。插件加入了新的原生类型，需要完整重启。

在 Edit → Plugins 中确认 **Daniya Lip Sync** 和 **MetaHuman Animator** 已启用。当前项目已经配置完成。新功能用于编辑器内生成资源，不在 PIE 或打包游戏内执行音频分析；生成后的 AnimSequence 可以正常用于游戏或 Sequencer。

## 最省事：直接用插件窗口

打开 UE 顶部菜单 **Tools → Daniya Audio Animation**。中文界面中 Tools 对应“工具”，插件名称仍然是英文。

设置下列字段，点击窗口里的 **Generate**：

| 字段 | 填什么 | 建议初值 |
|---|---|---|
| Audio | 已导入的 SoundWave 音频资源 | `达尼亚_这样多累呀` |
| Mesh | Skeletal Mesh 资产，不是场景角色或组件 | `DaniyaToUe` |
| Mouth Gain | 口型开合强度 | `1.35` |
| Include Expressions | 是否生成额外表情，bool 开关 | 勾选：口型＋表情；不勾选：纯口型 |
| Expression Gain | 眉毛、眼睛和嘴角映射的强度 | `0.65` |
| Smooth | 平滑相邻帧 | 勾选 |
| Mood | MetaHuman 情绪选项 | `Auto Detect` |
| Mood Intensity | MetaHuman 情绪生成强度，范围 0–1 | `1.0` |
| Output Folder | 输出的 UE 内容路径 | `/Game/DaniyaTest/Generated` |
| Asset Name | 动画名字，不加 `.uasset` | 可留空自动命名 |

`Mouth Gain` 调节五个基础口型。`Expression Gain` 调节映射后的表情幅度。`Mood Intensity` 则输入 MetaHuman 求解器，影响其生成结果。把 Expression Gain 设为 0 会让表情曲线归零；彻底关闭额外表情生成请取消 Include Expressions。

输入类型是音频资产引用。如果手上是 WAV/MP3 文件，先在内容浏览器里导入，再用 Audio 选择器选它。不能在 Audio 输入框中填写 Windows 文件路径。

点击 Generate 后查看 **Status**。处理异步进行，可以继续在编辑器中工作。成功后：

- **Animation** 是已经保存的 AnimSequence，可直接引用。
- **Performance** 是本次自动创建、处理并保存的 MetaHuman Performance，可打开查看求解结果。
- **Status** 会显示输出路径和曲线数量。
- 输出目录中会出现动画和同名加 `_Performance` 的资源。

重复使用同一个名字会自动加 `_001` 等后缀，保留已有资源。一时间只运行一个 Daniya 生成任务；需要中止时点 **Cancel**，等待取消结束再生成。

## 蓝图中只调用一个节点

在 **Editor Utility Widget 的 Event Graph** 中搜索：

**Generate Daniya Animation From Audio**

如果没有 Editor Utility Widget：内容浏览器空白处右键 → Editor Utilities → Editor Utility Widget，打开后放一个 Button，在按钮的 OnClicked 事件中添加该节点。右键这个 Widget 资源选择 Run Editor Utility Widget 即可运行。

节点默认显示 Audio、Mesh、Mouth Gain、Include Expressions、Expression Gain。展开底部高级参数可以填写 Output Folder、Asset Name、Mood、Mood Intensity、Smooth。

最小连接关系：

```text
按钮 OnClicked
    → Generate Daniya Animation From Audio
        Audio = 音频资源
        Mesh = DaniyaToUe
        Mouth Gain = 1.35
        Include Expressions = true / false
        Expression Gain = 0.65

        Completed → 拆开 Result → Animation：保存到你的 AnimSequence 变量
        Failed    → 拆开 Result → Message：连接 Print String 或显示到界面
```

这是异步节点。执行输入表示“开始处理”，**Completed 才表示生成并保存成功**。不要在发起调用后立刻读取尚未完成的动画。把后续工作连接到 Completed。

Result 是原生 `Daniya Audio Animation Result` 结构体，可以右键引脚 Split Struct Pin，或使用 Break 节点：

| Result 字段 | 含义 |
|---|---|
| Success | 是否完整成功 |
| Animation | AnimSequence 对象引用 |
| Performance | 自动生成的 MetaHuman Performance |
| Saved Asset Path | 动画资源路径，例如 `/Game/.../AS_Name.AS_Name` |
| Message | 成功说明或失败原因 |
| Expression Curves | 实际写入的额外表情曲线名；纯口型模式为空 |

节点还会提供 Task 引用，可调用 Cancel，也可读取 Progress 和 Finished。音频阶段 Progress 到约 0.9，烘焙与保存完成后到 1；它不是精确剩余时间。

保存失败会走 Failed，并在可能的情况下保留 Result 中已经创建的内存资源，便于手动恢复；只有 Completed 才保证动画和 Performance 都已保存。

这个生成节点应放在编辑器工具的事件图中。普通 Blueprint Function 不允许放异步节点；游戏角色蓝图也不适合执行编辑器资源创建。

## 表情开关实际做什么

MetaHuman 可以根据音频生成 Full Face 动画，并提供自动或指定情绪、眨眼等选项。它推测的是一段合理的面部运动，不能仅靠声音还原说话者真实的表情。

关闭 Include Expressions：求解器使用 Mouth Only，输出五个 MMD 口型 `あ、い、う、え、お`。

开启 Include Expressions：求解器使用 Full Face 并开启生成眨眼，随后把结果映射到达尼亚已有的 15 个形变：

| 表情部位 | 使用的模型 Morph |
|---|---|
| 双眼眨眼 | `まばたき` |
| 左右眼睁大 | `左びっくり`、`右びっくり` |
| 左右眼收紧/笑眼 | `左笑い目`、`右笑い目` |
| 左右眉毛抬起 | `左上`、`右上` |
| 左右眉毛内侧抬起 | `左困る`、`右困る` |
| 左右眉毛压低 | `左怒り`、`右怒り` |
| 左右嘴角上扬 | `左口角上げ`、`右口角上げ` |
| 左右嘴角下压 | `左口角下げ`、`右口角下げ` |

达尼亚模型表情版共写入 20 条曲线。眉毛和眼睛采用权重限制，嘴角额外减弱，表情在首尾约 0.12 秒淡入淡出，降低多个形变叠加造成的过度变形。声音中没有明显情绪时，部分曲线很弱或为零是正常现象。

这套映射针对当前模型的 Morph 名称。选择另一个网格时，它必须具备五个 MMD 口型及相应骨架 Morph 元数据；开启表情还需要至少一个已支持的表情 Morph。部分表情 Morph 缺失时只生成可用部分，并在结果说明中提示。对于任意名字的模型，仍需另外配置或编写重定向映射，不能仅凭骨骼相似就自动变出表情。

本版本不写入头部或身体骨骼动作。音频还在原 SoundWave 里，AnimSequence 不包含声音轨道。

## 在 Sequencer 中播放

本次已经用你提供的音频准备好：

- 纯口型动画：`/Game/DaniyaTest/Generated/AS_Daniya_AutoMouth`。
- 口型＋表情动画：`/Game/DaniyaTest/Generated/AS_Daniya_AutoFace`。
- 表情演示地图：`/Game/DaniyaTest/Generated/L_Daniya_AutoFace`。
- 已对齐原音频的演示序列：`/Game/DaniyaTest/Generated/LS_Daniya_AutoFace`。

打开演示地图，再打开演示序列，点击 Sequencer 的播放按钮即可预览。这些是为本次音频准备的示例；以后生成节点返回动画与 Performance，播放时使用下面的方法添加音轨。

把达尼亚角色添加到 Sequencer，在它的 Animation 轨道选生成的动画，再添加 Audio 轨道选择原 SoundWave，将两者起点对齐。

如果使用已有身体动画，把面部曲线合并/叠加到现有动画工作流中；单独生成的资源主要包含面部 Morph 曲线。不要同时让另一套动画逻辑每帧覆盖相同的 Morph 值。

预览网格选择 DaniyaToUe。若模型显示材质异常，使用之前 demo 中支持 Morph Targets 的 `_LipSync` 材质副本。

## 常见问题

- **搜不到节点或 Tools 菜单**：完整重启 UE，确认插件启用；搜索完整英文名称。节点需在 Editor Utility 的事件图中使用。
- **没有输出**：看 Failed 的 Message 或工具窗口 Status；MetaHuman 原始诊断在 Output Log 中。自动求解失败不会静默生成成功的全零动画。
- **表情太大**：先把 Expression Gain 从 0.65 调到 0.3；口型太大则降低 Mouth Gain。
- **情绪与预期不符**：从 Auto Detect 改成 Sad、Happy、Neutral 等，重新生成一份比较。Neutral 下情绪强度可能没有效果。
- **模型缺少 Morph**：插件不能从骨骼网格体自动创造不存在的形变。
- **提示另一个任务正在处理**：等待 Completed/Failed；或 Cancel 后再试。不要连续点击 Generate。
- **结果保存失败**：查看 Message，检查磁盘空间、文件是否只读。不要把 Failed 当作成功使用。

## 旧节点仍可使用

`Build Daniya Mouth Weights`、`Validate Daniya Controls`、`Apply Daniya Mouth Weights`、`Get Daniya Mouth Frame`、`Bake Daniya Mouth Curves`、`Create Daniya Mouth Animation` 以及原生 Mouth Modifier 均保留。旧节点继续用于已有 Performance 和自定义工作流；新的“一键生成”节点自动完成了前置音频处理和保存步骤。

## 源码与安装

项目已安装 V2，无需重复解压。备用压缩包包含源码、当前 UE 构建的 Win64 DLL 和预编译文件。安装到另一个兼容项目时，关闭 UE，将 `DaniyaLipSync` 文件夹放入项目的 `Plugins` 目录，再打开项目启用插件。

更换 UE 构建或版本时需要重新编译。C++ 音频生成与工具窗口位于 Editor 模块；原有运行时嘴型计算、Morph 应用和生成后的动画播放保持可用于打包游戏。

## 本次验证

- 在当前引擎上通过编辑器、Win64 Development 和 Shipping 的插件编译。
- UE 自动化测试通过：原有口型数学与校验、表情范围/组合权重、Editor Utility 中异步节点的引脚与蓝图编译。
- 实际执行完整音频输入流程，分别生成 Full Face 和 Mouth Only 动画，并验证异步完成、错误输入、同时提交任务、自动避开同名资源及取消操作。
- 保存后重新启动验证：两份动画均为约 11.633 秒，源求解数据 348 帧；表情版 20 条曲线、纯口型版 5 条曲线，每条曲线 350 个关键点，数值有限且在 0–1 范围内，首尾归零。

自动测试中的 Python 仅用于驱动验证；你实际使用的求解调用、映射、烘焙、保存、节点和工具窗口均由 C++ 插件完成。


