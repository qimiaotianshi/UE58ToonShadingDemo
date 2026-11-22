# UE58ToonShadingDemo

基于 **Unreal Engine 5.8** 的卡通渲染（Toon Shading）演示工程，含 AI 语音对话功能。

## 环境要求

| 项目 | 版本 |
|---|---|
| Unreal Engine | 5.8 |
| 渲染管线 | Substrate / Lumen / 光线追踪 |
| 目标平台 | Windows (DX12, SM6) |
| 解压工具 | 7-Zip 或 Bandizip（用于合并分卷包） |

---

## 下载工程（给使用者）

工程分为两部分：**代码走 git 仓库，资产走 Release 附件**。资产包里**已包含完整的 AI 运行库**，不需要再去网盘或任何其它地方下载。

### 第 1 步：克隆代码仓库

仓库只保存源码、配置、文档和脚本，体积很小，几秒钟即可完成。**不需要安装 Git LFS**。

```bash
git clone --depth 1 https://github.com/qimiaotianshi/UE58ToonShadingDemo.git
cd UE58ToonShadingDemo
```

> 请务必加 `--depth 1`（浅克隆）。历史提交里曾用过 Git LFS，完整克隆会尝试下载旧的 LFS 对象而失败。

### 第 2 步：下载资产包

打开 [Releases 页面](https://github.com/qimiaotianshi/UE58ToonShadingDemo/releases)，找到最新版本，把 **全部** 形如下面的分卷文件下载到同一个文件夹：

```
UE58Assets_v1.7z.001
UE58Assets_v1.7z.002
UE58Assets_v1.7z.003
...
```

- 每卷约 1.9 GB，完整数据约 30 GB，共 16 卷左右
- **必须下载全部卷**，缺一卷都解不开
- 内容包括 `Content\`（含 `Content\AI\` 的 SoVITS 语音运行库）和 `Plugins\`（含插件资源与二进制）

### 第 3 步：解压

把分卷放在同一个文件夹，用 7-Zip 或 Bandizip **右键第一个文件 `.001`** → 解压到工程根目录（即 `Toon58ShadingDemo.uproject` 所在的目录）。

解压后目录结构应如下：

```
Toon58ShadingDemo\
├── Toon58ShadingDemo.uproject
├── Config\
├── Source\
├── Content\          ← 来自资产包
│   └── AI\           ← AI 运行库，已包含
└── Plugins\          ← 来自资产包
```

> 分卷会自动合并，不需要手动拼接。

### 第 4 步：打开工程

用 UE 5.8 打开 `Toon58ShadingDemo.uproject`。首次打开会编译模块和着色器，耗时较长。

---

## 同步修改（给作者）

### 改了代码、配置、文档、脚本

直接双击工程目录下的 **`SyncToGitHub.bat`**，输入一句改动说明后回车即可。

内部执行 `git add -A` → `commit` → `push`。因为 `.gitignore` 已排除所有资产与二进制，推送不会再触发 Git LFS，也就不会再遇到额度超限的问题。

### 改了资产、模型、动画、音频、AI 文件

**这些改动不会自动同步到 GitHub**，需要重新打资产包并发布新 Release：

1. **关闭 UE 编辑器**
2. 双击 **`PackAssets.bat`**
3. 输入版本号（如 `v2`，直接回车则用 `v1`）
4. 等待打包完成（首次约 30–90 分钟，日志在输出目录的 `pack_log.txt`）
5. 到 [新建 Release 页面](https://github.com/qimiaotianshi/UE58ToonShadingDemo/releases/new)，tag 填同样的版本号，把生成的 **全部 `.7z.00*` 分卷** 上传并发布

`PackAssets.bat` 会把 `Content\` 和 `Plugins\`（**包含 AI 目录**）按 1900 MB 分卷压缩，输出到工程目录外层的 `Releases\<版本号>\`，不会被 git 扫到。

> 网页一次上传十几个 2 GB 文件容易中断，建议分批拖入；也可改用 GitHub CLI：
> ```bash
> gh release create v2 D:\UE\UE58ToonShadingDemo\Releases\v2\*.7z.* --title "v2" --notes "更新资产"
> ```

### 拉取别人的改动覆盖本地

双击 `PullFromGitHub.bat`（需输入大写 `YES` 确认）。注意它只同步**代码部分**；资产包仍需按上面「下载工程」的第 2、3 步手动更新。

---

## 为什么这样设计

GitHub 免费账号的 Git LFS 额度是 **1 GB 存储 + 1 GB/月流量**，而本工程资产约 30 GB（其中 AI 运行库 17 GB），远超免费上限，继续用 LFS 会一直被拒绝推送。

因此改为：

| | 内容 | 通道 |
|---|---|---|
| git 仓库 | 源码、配置、文档、脚本（纯文本，几十 MB） | `SyncToGitHub.bat` |
| Release 附件 | `Content\`、`Plugins\`（含 AI），约 30 GB | `PackAssets.bat` |

好处是推送稳定、不受 LFS 额度限制、别人克隆代码极快。代价是**资产不再有逐文件版本历史**，每次改资产都要重新打包发版。

---

## 工程内容

- **角色模型**：Daniya、QianXiao（含骨骼动画、捏脸动画与语音）
- **场景资产**：SubwayTrain、Stylized_Forest、Hillside、ArchVizToolsV2 等
- **卡通渲染**：CelShading 材质、ToonProfile 系列查找纹理
- **AI 语音**：SoVITS 运行库 + `WardrobeAISpeech` / `WardrobeSpeechGraphNode` 插件代码
- **内置插件**：`DaniyaLipSync`（口型同步）、`DreamBloom`（泛光后处理）、`SequenceCharacterStudio`（序列角色工具）、DLSS / Streamline 系列

## 从源码编译（C++ 工程）

```bash
<UE安装目录>/Engine/Build/BatchFiles/GenerateProjectFiles.bat -project="Toon58ShadingDemo.uproject" -game
```

或用 Visual Studio 打开生成的 `.sln` 后编译。

## 注意事项

- **不要删除 `.gitignore`** —— 它决定哪些数据不进 git，删掉后大文件会被重新提交并导致推送失败
- **不要删除 `.gitattributes`** —— 本仓库已不再使用 LFS，该文件只做文本换行规范化
- `Binaries/`、`Intermediate/`、`Saved/`、`.vs/` 是本地生成内容，已忽略，无需提交
- 不要把备份压缩包、AI 运行环境放在工程目录里指望 git 同步；它们一律走 Release
- 历史提交中曾使用 LFS，那些旧版本里的资产是指针文本；需要回滚旧资产时请从对应 Release 下载
