# UE58ToonShadingDemo

基于 **Unreal Engine 5.8** 的卡通渲染（Toon Shading）演示工程。

## 环境要求

| 项目 | 版本 |
|---|---|
| Unreal Engine | 5.8 |
| 渲染管线 | Substrate / Lumen / 光线追踪 |
| 目标平台 | Windows (DX12, SM6) |

## 下载

本仓库使用 **Git LFS** 管理资产文件，克隆前请先安装 [Git LFS](https://git-lfs.com/)：

```bash
# 1. 安装 Git LFS（若未安装）
git lfs install

# 2. 克隆仓库（会自动下载 LFS 资产）
git clone https://github.com/qimiaotianshi/UE58ToonShadingDemo.git
```

> 首次克隆需下载约 3 GB 资产，请耐心等待。

## 打开工程

1. 用 UE 5.8 打开 `Toon58ShadingDemo.uproject`
2. 提示需要编译模块时选择「是」
3. 首次打开会编译着色器，耗时较长

## 工程内容

- **角色模型**：Daniya、QianXiao、SuoMing（含骨骼动画）
- **场景资产**：SubwayTrain（地铁列车）、Stylized_Forest（风格化森林）、ArchVizToolsV2
- **内置插件**：
  - `DaniyaLipSync` — 口型同步
  - `DreamBloom` — 泛光后处理
  - `SequenceCharacterStudio` — 序列角色工具

## 从源码编译（C++ 工程）

```bash
# 生成工程文件
<UE安装目录>/Engine/Build/BatchFiles/GenerateProjectFiles.bat -project="Toon58ShadingDemo.uproject" -game

# 或用 Visual Studio 打开 .sln 后编译
```

## 同步修改

```bash
git add -A
git commit -m "描述你改了什么"
git push
```

## 注意事项

- `Content/`、`Plugins/` 下的资产与二进制均通过 LFS 存储，请勿删除 `.gitattributes`
- `Binaries/`、`Intermediate/`、`Saved/` 为本地生成内容，已在 `.gitignore` 中排除，无需提交
- 超大资产单文件可能超过 100 MB，必须经 LFS 上传，直接 push 会被 GitHub 拒绝
