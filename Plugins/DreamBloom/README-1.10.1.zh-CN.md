# DreamBloom 1.10.1：右键横拖旋转人物

安装工程：F:\新建文件夹\MyProject。重启 UE 后生效。

本版输入：
- 左键：KawaiiPhysics 点击／按住拖拽，以及原有部位 Sequence 点击交互。左键未命中物理骨骼时也不再旋转人物。
- 右键左右拖动：旋转当前人物。只有鼠标 X 位移生效。
- 右键上下拖动：不调整相机俯仰。
- 中键拖动：平移相机。
- 滚轮：缩放。

已删除 PitchCamera 函数和输入调用，并更新界面底部操作提示。
旋转仍遵守 DA_WardrobeCharacters 中当前条目的 Allow Character Rotation 开关，以及相机切换／Sequence 交互期间的输入限制。

Physics Touch 与 Transition Sound 的配置继续沿用 1.10。无需重做 DA、人物或动画蓝图。

更新包中的 DreamBloom 是完整源码与 Resources。源码引擎电脑使用自己的引擎重新编译；仅本机官方引擎文件夹里的 DLL 仅供本机 UE5.8.3 BuildId 55116800 使用。
可选测试资源引用原工程中的 Daniya 模型、骨骼、动画和音效，跨项目迁移请在 UE 中 Migrate 测试关卡以收集依赖。

验证：修改后的 C++ 编译及 DLL 链接通过；已核对右键仅使用 Delta.X，俯仰函数及左键转角色分支已删除。未进行人工鼠标操作验收。
备份：F:\新建文件夹\MyProject\Saved\WardrobeBackup-20261007-RightDrag1011。
