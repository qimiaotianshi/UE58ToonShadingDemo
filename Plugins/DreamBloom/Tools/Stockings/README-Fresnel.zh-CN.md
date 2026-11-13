# 丝袜菲涅尔与运行时法线

菲涅尔恒为 1 的原因已定位为运行时网格重新计算法线时的方向错误。

源丝袜三角面绕序的叉积方向与 Surface 中的烘焙法线相反。检查中有 15,912 个有效三角面反向、0 个同向，另有 72 个退化或接近退化的三角面。旧代码直接使用叉积累加顶点法线，因此运行时法线朝向皮肤内部。

UE 默认菲涅尔计算为：

```text
F = Base + (1 - Base) * pow(1 - max(0, dot(N, V)), Exponent)
```

当可见表面的 N·V 为负时，它被截成 0，F 就是 1。原丝袜 Custom 节点使用 abs(N·V)，因此之前没有暴露方向问题。

代码现在初始化每个三角面相对烘焙法线的朝向符号，更新时使用该符号重建变形后法线。三角面索引、UV、材质及原有烘焙资源都保持原样，不需要重新烘焙。正常拉拽产生的法线旋转仍被保留。

## 在材质中使用

无贴图法线时可以使用 VertexNormalWS；需要考虑法线贴图时使用 PixelNormalWS。N 和 CameraVectorWS 要在同一坐标空间中。双面丝袜建议使用对正反面一致的表达式：

```hlsl
float F = pow(1 - saturate(abs(dot(normalize(N), normalize(V)))), Exponent);
float Opacity = lerp(BaseOpacity, EdgeOpacity, F)
              * (1 - saturate(VertexColor.R * StretchOpacityStrength));
```

节点对应：Normalize N/V → DotProduct → Abs → Saturate → OneMinus → Power；结果连接 Lerp 的 Alpha，Lerp 的 A/B 分别为基础透明度和边缘透明度。再乘上应变驱动的衰减，输出到 Opacity。先用 Exponent=2～5 调整边缘范围，外观值由自己的材质决定。

当前保存的实验图中，Opacity 为 qwer × Fresnel，暂时没有接入应变透明度；直接将 F 作为透明度会使正面接近透明、边缘更不透明。使用上面的 Lerp 可以独立控制正面和边缘覆盖。F 本身与拉拽应变是两种不同的输入。

## 验证

DreamBloom 和 DreamBloomEditor 编译成功，4 项丝袜自动化测试通过，其中新增检查覆盖运行时法线与烘焙法线同向、正面菲涅尔接近基础反射值以及掠射角趋近 1。原关卡使用当前用户材质完成修复前后渲染对比；M_StockingPull 和 MI_DaniyaStockings 文件哈希保持一致。
