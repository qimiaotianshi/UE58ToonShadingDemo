# BakeSurface：制作自己的丝袜后如何接入

BakeSurface 是 DreamBloomEditor 提供的 C++ 编辑器烘焙函数。现在可以直接通过原生工具窗口使用，无需启用 Python 插件。入口代码位于：

- Plugins/DreamBloom/Source/DreamBloomEditor/Public/WardrobeStockingEditorTools.h
- Plugins/DreamBloom/Source/DreamBloomEditor/Private/WardrobeStockingEditorTools.cpp
- Plugins/DreamBloom/Source/DreamBloomEditor/Private/WardrobeStockingBaker.cpp：Slate 窗口、顶部菜单和骨骼网格右键入口。

烘焙从导入模型中提取丝袜顶点、UV、三角面、骨骼权重，以及身体衣物的遮挡三角面，生成运行时可用的 Surface 数据。运行时的拉拽、应变透明和回弹也由 C++ 实现。

## 最方便的使用方法

1. 在 Blender 制作独立的紧身丝袜网格，使用达妮娅兼容的骨骼名称、绑定姿势和腿部权重，保留 UV，并保证拉拽区域有足够细分。
2. 把 FBX 导入 UE，导入类型选择 Skeletal Mesh。等待导入结束。
3. 使用本项目对应的自定义引擎，编译 Development Editor / Win64，并重新打开编辑器，确保 DreamBloom 和 DreamBloomEditor 加载。
4. 在内容浏览器中只选中你新导入的丝袜 Skeletal Mesh，不要选身体模型、骨架或多个资源。
5. 右键选择 **“烘焙丝袜 Surface…”**，或者从顶部 **“工具 → 丝袜烘焙…”** 打开窗口。检查丝袜网格和身体；当前默认身体是 DaniyaToUe。
6. 设置输出路径，点击 **“烘焙 Surface”**。默认路径是 `/Game/Wardrobe/Stockings/DA_<丝袜网格名称>_Surface`。成功后点击 **“定位生成资源”**。
7. 打开 `/Game/Wardrobe/DA_WardrobeCharacters`，在目标角色的 `Characters → Stockings` 中：打开 `Enabled`，把 `Surface` 设置成刚生成的数据，`Material` 使用 `/Game/Wardrobe/Stockings/MI_DaniyaStockings` 或自己的丝袜材质，然后保存 DA。
8. 进入角色展示界面，按住左键拖动露出的丝袜区域，松手回弹。组件初始化时从身体读取光照通道；当前达妮娅使用通道 2。

第一次使用不需要编辑任何脚本。如果 Surface 已存在，勾选窗口中的 **“覆盖已有 Surface”** 后重新烘焙。窗口会拒绝覆盖其他类型的资源；运行或模拟期间不能烘焙。

Stockings 配置保留资源引用、贴合间距、拉拽、弹簧及调试参数。颜色和透明度等外观在材质或材质实例中处理；C++ 只提供 Vertex Color R=应变、G=拉拽影响（范围 0–1）。当前外观实例为 `MI_DaniyaStockings`，父材质为 `M_StockingPull`。修改外观或更换材质无需重新烘焙。

## 可选的 Python 批处理入口

`bake_stocking_surface.py` 是可选的便捷包装：检查选择和输出路径，然后调用 `unreal.WardrobeStockingEditorTools.bake_surface`；它不实现拉拽算法。仅使用原生窗口时不需要该脚本或 Python 插件。

需要自动化执行时，启用 Python Editor Script Plugin 和 Editor Scripting Utilities，通过“工具 → 执行 Python 脚本”执行它。可以修改脚本顶部参数，例如：

```python
STOCKING_ASSET = "/Game/Model/Daniya/Red/Stockings/SK_MyStockings"
BODY_ASSET = "/Game/Model/Daniya/Red/DaniyaToUe"
OUTPUT_ASSET = "/Game/Wardrobe/Stockings/DA_MyStockingSurface"
ALLOW_OVERWRITE = False
VALIDATE_ONLY = False
```

这些是 UE 内容路径，不是磁盘路径；不要写 `Content/`、`.uasset` 或对象名称后缀。`VALIDATE_ONLY = True` 可以只检查选择、依赖和输出路径，不写入资源；它不会执行骨骼姿势验证，姿势由正式烘焙过程检查。

使用 UnrealEditor-Cmd 执行而不打开编辑器界面时，设 `SYNC_CONTENT_BROWSER = False`，关闭烘焙后的内容浏览器定位。

也可以在 Editor Utility Blueprint 中调用 `Bake Surface` 节点（类别 Wardrobe / Editor / Stockings），输入 Stocking、Body 和 Package Path，输出相同的 Surface 数据。

## 模型适用范围

当前烘焙器为达妮娅配置，识别 `足D_L`、`足D_R`、`ひざD_L`、`ひざD_R` 等腿骨和“肌肤”等身体材质槽。只把 Body 换成其他角色资产，不能保证支持另一个角色。

烘焙时会保留新网格的拓扑和 UV，把顶点贴到 UE 实际皮肤表面并外扩约 1.2 mm，同时插值皮肤骨骼权重。因此它适合紧身丝袜；明显褶皱、厚袜或宽松造型需要调整烘焙器才能保留形状。

运行时读取 Surface 中保存的顶点和权重，不会自动读取重新导入后的模型。修改顶点、拓扑、UV、权重或绑定姿势后，都应重新烘焙。

BakeSurface 会保存丝袜 Skeletal Mesh、它的骨架和生成的 Surface，并清空丝袜网格的 Physics Asset 引用。请使用独立丝袜网格；它通过真实三角面拾取，不需要该物理资产。身体和角色 DA 不由烘焙工具修改，Surface 仍需手动配置到角色 DA。

## 常见提示

- 没有“丝袜烘焙”菜单：先编译本项目的 Development Editor / Win64，使用对应的自定义引擎重新打开工程。
- 右键没有入口：只选中一个 Skeletal Mesh，或使用顶部工具菜单。
- Surface 已存在：换输出路径，或勾选“覆盖已有 Surface”。
- `reference pose does not match` / `bone map failure`：检查骨骼名称、绑定姿势和 FBX 缩放。完整原因见 Output Log 中的 STOCKING_BAKE / STOCKING_IMPORT_ALIGNMENT。

`import_stocking_assets.py` 是原达妮娅丝袜的导入、材质生成和配置初始化脚本，路径固定。使用自己新导入的丝袜时，请用原生窗口；`bake_stocking_surface.py` 留作可选的批处理入口。
