# 达妮娅丝袜拉拽 · UE 接入说明

已为 F:/ToonSubstrate/Demo 的角色展示界面接入独立丝袜、局部拉伸透明和松手回弹。对应两个现有达妮娅展示条目；运行时状态各自独立。

## 使用与参数

重新打开工程以加载已编译的 DreamBloom 插件，进入现有角色展示界面。按住鼠标左键拖动露出的丝袜区域，松手回弹。裙子、披风和鞋子挡住的区域不能隔着衣物抓取。开始角色反馈动作、切换角色或离开展示会取消正在进行的拉拽。

打开 /Game/Wardrobe/DA_WardrobeCharacters → Characters → 对应角色 → Stockings。

| 参数 | 默认值 | 作用 |
| --- | --- | --- |
| Enabled | 开启 | 控制该角色丝袜显示与抓取 |
| Surface | DA_DaniyaStockingSurface | 导出的独立网格及蒙皮、遮挡数据 |
| Material | MI_DaniyaStockings | 自定义丝袜材质或材质实例，直接用于渲染 |
| Surface Clearance | 0.05 cm | 在模型原有 1.2 mm 间距之外，增加少量贴合间距 |
| Influence Radius | 16 cm | 沿网格表面计算的局部影响范围 |
| Maximum Pull | 16 cm | 最大拉出距离 |
| Drag Sensitivity | 1 | 鼠标拖动灵敏度 |
| Foot Pull Ratio | 0.3 | 脚部拉伸距离相对腿部的比例 |
| Pinch Tip Radius / Sharpness | 0.4 cm / 2 | 捏起区域的尖端圆润度与衰减 |
| Follow Frequency / Damping | 5 / 0.65 | 拖动时跟随的弹性与阻尼 |
| Release Frequency / Damping | 11 / 0.85 | 松手后恢复的速度与阻尼 |

距离使用角色模型局部厘米，随角色 Actor 缩放。交互可先调 Maximum Pull 和 Influence Radius。

颜色、透明度、拉伸透明响应和纤维光泽统一在 `/Game/Wardrobe/Stockings/MI_DaniyaStockings` 的 `Stocking Appearance` 分组调整；其父材质是 `M_StockingPull`。保留现有外观值：StockingTint=(0.004, 0.003, 0.005)、BaseOpacity=0.97、StretchOpacityStrength=1.5、EdgeOpacity=0.985、PearlStrength=0.15。这五个参数已从 Stockings 结构删除，运行时直接使用 DA 指定的材质，不创建或覆盖其外观参数。

使用自己的材质时，通过 Vertex Color 的 R 通道读取应变（0–1），G 通道读取局部拉拽影响（0–1）；B 为 0，A 为 1。透明度、颜色和光泽如何响应该数据由材质决定。不同角色需要不同外观时，创建不同材质实例并分别配置到 DA 的 Material。只修改外观或更换材质无需重新烘焙。

## 模型与资源

独立丝袜从原 Blender 角色的腿部皮肤面复制，保留骨骼顶点组和权重，沿法线外扩约 1.2 mm，再细分一次，获得足够密度。Blender 中 8,678 顶点、15,984 三角面；UE 因接缝拆点为 8,687 顶点。它随腿部骨骼变形，拉拽只改变丝袜。

新 Blender 工程：F:/ToonSubstrate/Demo/Content/Model/Daniya/Red/Stockings/Source/Daniya_WithStockings.blend。
新 FBX：同目录 Daniya_Stockings.fbx。
原 C:/Users/panronglin/Desktop/三转二/无标题.blend 和已有 DaniyaToUe 身体资源保留。

UE 资源：

- /Game/Model/Daniya/Red/Stockings/SK_Daniya_Stockings：独立骨骼网格。
- /Game/Model/Daniya/Red/Stockings/SK_Daniya_Stockings_Skeleton：该源网格独立保存的骨架。
- /Game/Wardrobe/Stockings/DA_DaniyaStockingSurface：离线蒙皮数据及服装遮挡三角面。
- /Game/Wardrobe/Stockings/M_StockingPull：半透明丝袜材质。
- /Game/Wardrobe/Stockings/MI_DaniyaStockings：现有达妮娅使用的外观材质实例。
- /Game/Wardrobe/DA_WardrobeCharacters：两个现有角色条目的配置。

## 实现与 Unity 的对应

Unity 的 StockingMousePull 用鼠标射线确定抓点，以局部影响权重移动丝袜顶点，计算张力交给 Shader Graph，并在松手后用弹簧恢复。这里使用相同的拉拽、变薄和回弹机制。高张力区域变透明，视觉上像被扯开；目前没有持久破洞或删除三角面。

UWardrobeStockingComponent 先用角色最终的组件空间骨骼变换和逆绑定矩阵执行 CPU 蒙皮，再叠加局部变形，通过 UProceduralMeshComponent 绘制。它在 TG_PostUpdateWork 更新，等待角色动画的并行求值完成。烘焙数据随 DA 保存，运行时不依赖可能被打包剥离的骨骼网格 CPU 缓冲。

拾取检测丝袜真实三角面、世界遮挡以及角色服装三角面。影响范围使用接缝邻接和沿网格的最短距离，避免从一条腿直接传到另一条腿。外向约束减少向皮肤内拉入；脚部附带局部聚拢；阻尼弹簧控制跟随和恢复。

变形后的相邻边长相对蒙皮静止边长决定应变，写入顶点颜色 R，拉拽影响写入 G。材质据此减少不透明度，同时保留正常状态薄纱、边缘增密和程序化细颗粒光泽。

丝袜组件初始化时读取身体的光照通道，当前身体和丝袜均为通道 2。现有灯光的 Toon Light Intensity、Toon Light Color、Toon Shadow Color 和边缘光设置保留。丝袜采用半透明密度叠层，透出的腿部继续接受原来的延迟卡渲光照；叠层色使用曝光补偿，避免常规前向光照把黑色丝袜冲亮。此材质没有另行复制引擎的全部延迟卡渲高光计算。

烘焙同时把丝袜投射到 UE 身体的实际皮肤三角面，并传递重心插值骨骼权重；源网格验证没有穿入皮肤，平均间距为 0.1192 cm。材质使用 Before DOF 半透明阶段并开启正常深度测试，由原场景深度遮挡裙子、鞋子和其他物体。原身体、灯光、后处理和关卡没有被保存修改。

新增代码在 Plugins/DreamBloom/Source/DreamBloom 的 WardrobeStocking.h/.cpp；编辑器烘焙与预览在 DreamBloomEditor 的 WardrobeStockingEditorTools.h/.cpp。已有 WardrobeFeedback 路由丝袜按下、更新、松手；优先于四肢、捏脸及普通物理拖拽。插件增加 ProceduralMeshComponent 依赖。

## 调整模型后重新烘焙

工具目录：F:/ToonSubstrate/Demo/Plugins/DreamBloom/Tools/Stockings。

自己的新丝袜可在内容浏览器选中 Skeletal Mesh，右键选择“烘焙丝袜 Surface…”，或从顶部“工具 → 丝袜烘焙…”打开原生 C++ 窗口。选择身体、设置输出路径并烘焙，随后在角色 DA 中选择生成的 Surface；重新烘焙同一资源时勾选“覆盖已有 Surface”。这个入口不需要 Python 插件。具体操作见 `Tools/Stockings/README-BakeSurface.zh-CN.md`，Python 脚本保留为可选批处理入口。

从原始 Blender 工程重新生成丝袜（会更新上述新 Blender/FBX；原工程不被保存覆盖）：

```powershell
& 'C:/Program Files/Blender Foundation/Blender 5.2/blender.exe' --background 'C:/Users/panronglin/Desktop/三转二/无标题.blend' --python 'F:/ToonSubstrate/Demo/Plugins/DreamBloom/Tools/Stockings/create_daniya_stockings.py' -- --output 'F:/ToonSubstrate/Demo/Content/Model/Daniya/Red/Stockings/Source'
```

如果 UE 已有 SK_Daniya_Stockings，先在编辑器中重新导入该 FBX，再执行 import_stocking_assets.py 烘焙。脚本首次运行会导入缺少的资源；已有网格不会被自动重新导入。脚本会更新生成父材质的渲染设置，保留已有 MI_DaniyaStockings 的外观覆盖和角色 DA 中的交互参数。使用自定义材质的角色建议通过原生烘焙窗口只更新 Surface。

```powershell
& 'F:/ToonSubstrate/ToonLemalinSubstrateUnrealEngine-release/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' 'F:/ToonSubstrate/Demo/Toon58ShadingDemo.uproject' -EnablePlugins=PythonScriptPlugin -run=pythonscript -script='F:/ToonSubstrate/Demo/Plugins/DreamBloom/Tools/Stockings/import_stocking_assets.py' -unattended -NullRHI -nosound
```

烘焙工具从导入 LOD 数据读取顶点并按骨名对应身体，验证腿部绑定姿势。当前资源缩放为 1，腿部参考骨骼最大对齐误差为 0 cm。换成其他角色需要对应网格、身体及新的 Surface；当前烘焙器的腿骨名和服装槽名按达妮娅设置。

render_stocking_preview.py 使用正常编辑器主视口生成三种状态的预览，保存到工程 Saved/StockingPreview。以 UnrealEditor.exe 打开工程并传入 -EnablePlugins=PythonScriptPlugin -ExecutePythonScript=该脚本的绝对路径 -unattended -RenderOffscreen -nosound。脚本只在内存中加载关卡、暂停默认动作和创建预览相机，保留原灯光与后处理，不保存关卡。首次运行会等待加载资源的着色器编译，结束后关闭本次启动的编辑器。

## 验证与范围

移除 DA 外观参数后，新增了直接绑定材质、切换材质、保留自定义透明度和清空材质引用的回归检查，合并在 DaniyaSkinPullRestore 测试中。模块编译和全部 4 项丝袜测试通过；重启编辑器读取 DA/MI 后确认五个外观字段已删除、材质值保持一致。L_WardrobeMain 的自然、拉拽、回弹预览正常。

原生 C++ 烘焙窗口已编译并通过 DreamBloom.Stockings.Editor.NativeMenuAndWindow、ValidationAndBake 两项自动化测试：覆盖顶部菜单、骨骼网格右键菜单、真实 Slate 窗口截图、错误输入、实际达妮娅模型副本烘焙及覆盖保护。与原有两项丝袜拾取和回弹测试一起运行，共 4 项成功、0 项失败、0 项测试警告。窗口截图和测试报告包含在 C++ 工具版迁移包中。

自定义 UE 5.8.4 的 Development Editor 插件编译通过。DreamBloom.Stockings.PickingAndSpring、DreamBloom.Stockings.DaniyaSkinPullRestore 两项自动化测试通过，覆盖实际角色表面拾取、局部变形、应变、限幅、松手连续恢复、再次抓取、通道 2 同步、停用交互与 DA 关闭。另有 5 项 DreamBloom.Feedback 测试及展示菜单覆盖默认动作测试通过，共 8 项成功、0 项失败。展示菜单测试包含 1 条已有 AI 语音 TTS bundle 缺失警告。

使用 L_WardrobeMain 正常编辑器主视口和原卡渲灯光验证了自然、拉拽、回弹三种状态，截图随交付说明提供。高应变处露出皮肤，松手后覆盖恢复；裙边和鞋子保持遮挡。

未执行发布包打包和人工 PIE 操作验收。当前 CPU 蒙皮及网格更新适用于角色展示；大量角色同时出现时应另行测量性能。

本机渲染启动日志另有已有 StreamlineCore 缺少 sl.interposer.dll 的错误及 ArchVizTools 输入绑定警告，未改动这些已有插件配置。

本次及此前的原关卡预览日志都有 SubstrateForwardLighting.ush 第 386 行向量逻辑运算的着色器错误，涉及其他半透明材质（包括 M_ToonMaterial 的部分实例）。丝袜材质没有编译失败，本次未修改引擎着色器或这些其他材质。

