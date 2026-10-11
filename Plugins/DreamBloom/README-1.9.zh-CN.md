# DreamBloom 1.9 配置说明

本版保留 DA + Actor Tag 相机/人物切换，增加场景相机景深同步、KawaiiPhysics 点击推动和 MPC + Sequence 点击动效。

## 1. 场景相机景深

临时预览相机现在把后处理设置写入 CameraComponent，修复 UE 的 CameraActor 视图路径绕过 CalcCamera 导致景深丢失的问题。到位后每帧读取 Tag 对应场景相机的后处理参数，包括 CineCamera 的焦距相关景深、光圈、聚焦设置及后处理权重。

在 CineCameraActor 中配置：

- Focus Settings → Focus Method = Manual 或 Tracking。
- Manual Focus Distance 设置距离（UE 默认 cm）；Tracking 时设置 Actor to Track。
- Current Aperture 调光圈，例如 1.8～2.8，数值越小通常景深越浅。
- Post Process Blend Weight 建议为 1。设为 0 会按你配置关闭相机后处理。
- 游戏画质中 Post Processing 不能关闭景深，`r.DepthOfFieldQuality` 应大于 0。

使用普通 CameraActor 时，在 Post Process Settings 中勾选要覆盖的景深参数。场景中的后处理体积、曝光和画质设置仍会参与最终画面。

切换镜头时插值聚焦距离、光圈、传感器宽度和后处理权重；其他后处理使用目标镜头设置，不做完整电影级后处理混合。相机的摆放位置仍不被浏览平移/旋转修改；这些操作作用于临时镜头。Tracking 焦点距离按照场景相机的位置求值，手动平移临时镜头后如需严格重构构图，请重置镜头。

## 2. 点击裙摆的 KawaiiPhysics 配置

### 插件要求

本版 DreamBloom 声明了 KawaiiPhysics 插件依赖。本机以 **KawaiiPhysics 1.21.0 的现有源码/API** 编译验证，使用 `AddExternalForcesToComponent`、`RemoveExternalForcesFromComponent`、`GetStepDeltaTime` 和 Simulation Space 转换接口。另一台电脑若用更旧或改过 API 的 KawaiiPhysics，需要用该版本重新编译并核对接口；不要替换你已有的物理解算插件。

### AnimBP

1. 裙摆必须有蒙皮骨骼，并已经被 KawaiiPhysics 节点正确解算。纯静态网格或没有裙摆骨骼的模型不能靠此功能产生布料形变。
2. 常规动画图：`Idle / 状态机 → Slot（需要接 Sequence 时）→ Local To Component → KawaiiPhysics → Component To Local → Output Pose`。若使用 Post Process AnimBP，则从 Sub-graph Input 接入身体姿势，不能用 T Pose 作为基础姿势。
3. 在 KawaiiPhysics 节点配置裙摆 Root Bone、Exclude Bones、碰撞和刚度等，让裙摆先能正常模拟。
4. 项目设置 → Gameplay Tags 中新增 `Wardrobe.Touch.Skirt`。选中裙摆 KawaiiPhysics 节点，将 **Kawaii Physics Tag** 设为这个 Gameplay Tag。头发节点使用其他 Tag 即可隔离。
5. 不必在 AnimBP Event Graph 手写鼠标事件，不必手动添加外力实例。DreamBloom 会在点击时找到匹配节点并添加短暂外力，结束或切换人物时移除；主 AnimBP、Linked Instance 和 Post Process Instance 由 KawaiiPhysics 的组件查找接口处理。

### 人物 DA

`DA_WardrobeCharacters → Characters → 对应人物 → Physics Touch`：

| 参数 | 建议起点 / 意义 |
|---|---|
| Enabled | 开启 |
| Node Tags | `Wardrobe.Touch.Skirt`，精确匹配节点的 Gameplay Tag；留空影响全部 KawaiiPhysics 节点 |
| Radius | 30 cm，只推动点击点半径内的模拟骨骼 |
| Strength | 100，近似局部推动速度 cm/s；建议从 30～100 开始，结果也受刚度和阻尼影响 |
| Duration | 0.25 秒 |
| Falloff | 2，越大越集中于点击中心 |
| Cooldown | 0.15 秒 |
| Radial Blend | 0 沿鼠标射线向里推，1 从点击中心向外推；默认 0.3 |
| Draw Debug | 显示青色作用范围球 |

### 点击范围

鼠标射线需要命中人物 Physics Asset 碰撞体或已配置的 Interactions 形状；它不是逐三角形检测蒙皮表面。裙摆没有碰撞体时，可以在人物 DA 的 **Interactions** 添加裙摆范围：

- Bone Or Socket 填裙摆对应骨骼/Socket，选择球、胶囊或盒，调整 Offset/Radius/Box Extent。
- 勾选 **Physics Touch Only**，Reactions 可以留空。这样点击只产生物理推动，不会提示缺少互动 Sequence。
- 不要勾选 Occluder Only，那个选项用于挡住后方点击。

手势沿用现有逻辑：左键按下后松开且没有超过拖动阈值时触发物理推动；拖动旋转人物不触发。处于镜头过渡或人物互动 Sequence 时不接受新的推动。推动使用独立的动画外力，不启动 Chaos ragdoll。连续点击替换前一次推动，避免外力无限叠加。

作用半径以点击命中的碰撞体表面为中心，按模拟骨骼的位置计算。如果显示了调试球却没有明显反应，检查球是否覆盖裙摆模拟骨骼、Node Tags 是否一致、节点 Alpha/LOD 是否启用、刚度是否过高。不要仅靠不断增加强度解决配置问题。

## 3. MPC + Sequence 点击动效

创建 **WardrobeClickEffectData** 类型的 Data Asset，例如 `DA_ClickFeedback`。在 `DA_WardrobeMenu → Click Effect` 指定它。

新 DA 中配置：

- Collection：材质参数集 MPC。
- Sequence：每次点击播放的 Level Sequence。
- World Position Parameter：Vector，默认 `ClickWorldPosition`，RGB = 世界 XYZ，A = 1。
- Screen UV Parameter：Vector，默认 `ClickScreenUV`，RG = 视口 UV，B = 是否命中场景/人物（0/1），A = 1。名字留 None 可不写。
- Time Parameter：Scalar，默认 `ClickTime`，当前游戏世界秒数。名字留 None 可不写。
- Min Interval：触发最小间隔，默认 0.05 秒。
- Max Duration：动效安全上限，默认 5 秒。
- Use Character Plane：射线未命中时，使用经过当前人物原点、朝向相机的平面。
- Fallback Distance：没有人物平面时沿鼠标射线取距离，默认 500 cm。

点击没有天然唯一的“鼠标世界位置”。本版优先用 Visibility 射线命中点，再考虑人物互动碰撞形状，最后使用上述平面/距离策略。天空、透明表面或没有碰撞的模型可能使用回退位置。

左键按下时触发动效，包含 Slate 菜单按钮和角色区域；事件不被消费，按钮仍正常响应。拖动的起始按下也会出现一次动效。功能限定展示菜单，加载过程中不触发。后处理绘制在 3D 场景层，不能覆盖 Slate/UMG 控件本身；若希望动效盖在按钮文字上方，需要额外做 UMG 层效果。

### Sequence 怎么做

1. 在点击 Sequence 中添加 **Material Parameter Collection Track**，指定同一 MPC。
2. 新建参数，比如 `ClickRadius`、`ClickIntensity`、`ClickWidth`，在轨道上打关键帧：0 秒半径 0/强度 1，0.8 秒半径 100/强度 0。
3. 位置、UV、时间参数由代码写，**不要在 Sequence 里给这三个参数打关键帧**，否则会覆盖鼠标输入。
4. 使用独立 MPC，避免人物互动 Sequence 同时修改同一组参数。新点击会停止并恢复上一次效果，然后写新位置，从头播放；一个 MPC 对应一个当前点击，不支持多个点击圈同时保留。
5. 可以在 Sequence 里控制后处理体积和材质参数。摄像机切镜轨道被禁用，避免抢走角色预览镜头；不要在点击 Sequence 中放人物 Transform 或其他无关轨道。
6. 可以把后处理材质预先放在相机或 PostProcessVolume 的 Blendables 中，默认强度设 0，再用 MPC 曲线控制显示。也可把 PostProcessVolume 转成 Sequence Spawnable，用 Sequence 管理生命周期。

### 材质怎么做

Material Domain 选 Post Process。用 `SceneTexture: PostProcessInput0` 作为底图，读 MPC：

```text
d = Distance(Absolute World Position, ClickWorldPosition.rgb)
ring = 1 - SmoothStep(0, Max(ClickWidth, 0.1), Abs(d - ClickRadius))
Emissive = PostProcessInput0.rgb + ring * ClickIntensity * RingColor
```

世界空间效果依赖 Scene Depth，只在有深度的表面出现。屏幕空间效果改为 `Distance(ScreenPosition.ViewportUV, ClickScreenUV.rg)`，注意横纵比校正，半径改用 UV 单位。Time 节点使用游戏时间，勿开启 Ignore Pause，否则与 ClickTime 时间源不同。

## 4. 示例资源与安装

包内可选 `Content/WardrobeFeedback19` 包含：

- `MPC_ClickFeedback`：位置、UV、时间、半径、强度和宽度。
- `LS_ClickFeedback`：0.8 秒扩散及强度衰减。
- `DA_ClickFeedback`：已经引用上面两个资源。
- `M_ClickRing`：世界空间后处理圆环材质，需加入场景相机/后处理体积的 Blendables。

这些是配置示例，未替换你的原有 DA 或场景。使用时将示例 DA 填到你的 Menu DA，并把材质放进后处理。

本机源码和版本化 DLL 更新后，保存并重启 UE 生效，不用 Live Coding。自定义引擎电脑使用包内完整 Source，备份后整目录替换，再用 Rider 编译 Development Editor / Win64；保留自己的 Content 和 KawaiiPhysics 插件。不要拷入本机 DLL 或 Intermediate。旧 `WardrobeInteractionQA.cpp`、`WardrobePreviewQA.cpp` 不能残留。

验证范围：官方 UE 5.8.3 编译；5 项自动化检查（景深/光圈实时更新、局部外力范围/失效、MPC 写入和 Sequence 参数求值，以及相机/人物切换回归）。示例资源已生成，原角色裙摆的视觉力度、真实鼠标操作和 KawaiiPhysics 最终动画效果未在用户场景中验证，需要结合你的骨骼和节点配置调节。
