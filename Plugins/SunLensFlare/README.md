# Sun Lens Flare

移植自 UE4.26 MainMap 中当前 LensFlareActor 的太阳镜头光晕效果，适配本机 UE5.8.4 / Substrate 工程。

## 使用

打开 `Content/Render/SunLensFlare/L_SunLensFlareDemo`，或双击本插件目录中的 `OpenDemo.bat`。

在其他关卡使用时，将 `Content/Render/SunLensFlare/BP_SunLensFlareActor` 拖入关卡。指定 `Sun Light` 为场景的 Directional Light；自动查找会优先选 Atmosphere Sun Light Index 0。也可关闭自动查找，使用 `Manual Sun Rotation`。旋转表示光线传播方向，与 Directional Light 的 Forward Vector 一致。

开启编辑器视口 Realtime。颜色、强度、亮度阈值、Ramp Index 与淡入淡出在 Actor Details 的 Lens Flare 中调整。Ghost、Halo、Glare 的形状参数分别在三个 `MI_MainMap_*` 材质实例中调整。完整预设保存在 `DA_MainMapLensFlare`。

## 移植内容

- 保留当前 MainMap 使用的三组原始网格（含打包 UV 和顶点颜色）、材质形状计算、渐变纹理与实例参数。
- 原 Kuro GI 管理器改为独立运行时 Actor，支持编辑器预览和运行时配置切换。
- 原专有的太阳方向节点改为插件传入的 `SunDirectionWS`。
- 原专有场景采样节点改为 Scene View Extension 与 1×1 HDR 渲染目标。GPU 以原 PC 的三个偏移点采样场景颜色及深度，取颜色最大值与反向 Z 最小值，再做时间平滑。材质保留亮度 smoothstep、遮挡平方和相机朝向衰减。
- 所有 UE 资产位于 `/Game/Render/SunLensFlare`。运行时代码和 Shader 位于此插件；Editor 模块用于资产图导入和检查。

## 采样参数

`Sun Sample Radius UV` 默认 0.008，对应原 PC 的 UV 偏移范围。`Sample Response` 控制时间平滑速度。`Sky Depth` 默认 0，仅将清空深度的天空视为可见；正值可将超出此距离的几何体视为天空。`Sample Scene` 关闭时可以独立检查形状。

亮度阈值以去除 UE PreExposure 的场景线性颜色计算。UE4 与 UE5 的天空、曝光、Tone Mapper 不同，最终画面亮度需要按目标关卡调整。

演示关卡使用 100000 lux 日光、固定 EV100=12，Actor 的 `Intensity` 设为 4096 补偿此曝光。Actor 默认值仍为 1；原始三个材质实例的形状参数保持 MainMap 数值。`Intensity` 在颜色淡入淡出之后乘入，调整亮度不会改变淡入淡出的时间。可先按目标关卡的曝光调整倍率，再微调三个实例。

## 范围

当前实现针对桌面 SM5/SM6 的 Deferred Renderer。每个 Actor 使用一个主要视图的采样历史；分屏、立体视图与移动 ES3.1 尚未适配。可使用不透明几何体遮挡太阳。半透明物体的遮挡取决于它是否写入场景深度。

新建内容不会更改工程默认关卡或原有业务插件。
