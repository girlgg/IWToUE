# IWToUE 虚幻引擎插件

如果您在使用过程中遇到任何问题，请在 [GitHub 上提交 Issue](https://github.com/girlgg/IWToUE/issues) ，或者加入 QQ 群：`817519915` 进行交流。

如果您觉得这个插件对您有帮助，请在 GitHub 上点亮 Star ⭐ 支持我们！这对我们非常重要。

## 安装步骤

1.  从 [Releases 页面](https://github.com/YOUR_USERNAME/YOUR_REPOSITORY/releases) 下载最新的发布包（`.zip` 文件）。
2.  如果您的虚幻引擎项目根目录中尚不存在 `Plugins` 文件夹，请创建一个。
3.  将下载的 `.zip` 文件内容解压到 `Plugins` 文件夹中。您应该会得到类似 `YourProject/Plugins/IWToUE/` 的目录结构。
4.  重新启动您的虚幻引擎编辑器。
5.  在编辑器中，转到 `编辑 (Edit)` > `插件 (Plugins)`。
6.  搜索 "IWToUE"，确保该插件已被启用，如果编辑器提示，请重新启动。

## 导入格式支持特性

*   **导入支持:** 支持导入来自 Infinity Ward 引擎游戏的资产。
    *   支持的格式：`SEModel`, `SEAnim`, `Cast`。
    *   **推荐:** 通常建议使用 `Cast` 格式进行导入，以获得更好的效果，现已完全放弃对 `SEModel` 和 `SEAnim` 的进一步支持和维护。
    *   关于 `Cast` 文件结构和库，请参考：[dtzxporter/cast](https://github.com/dtzxporter/cast/tree/master/libraries)
    *   对于Cast格式当前只支持对以下引擎的资产进行自动材质：`IW8`, `IW9`, `T10`
*   **灵活的导入选项:**
    *   `ConvertRefPosition` (转换位置): 勾选此项会转换位置数据。如果导入的模型出现拉伸或变形，请尝试取消勾选此项。
    *   `ConvertRefAnim` (转换旋转): 勾选此项会转换旋转数据。如果导入的模型旋转不正确，请尝试取消勾选此项。
    *   **重要提示:** 对于不同的动画文件，这两个选项的最佳勾选组合可能不同！请根据实际效果进行尝试。
*   **原生动画转换:** 动画会被完全转换为标准的虚幻引擎格式，允许在编辑器中直接预览和使用，无需额外的转换工具。
*   **动画叠加:** 可利用虚幻引擎内置的 `Apply Additive` 节点实现动画叠加，无需其他插件进行处理。
*   **自动材质与贴图导入:** 插件能够自动检测、导入并分配部分引擎的模型关联的材质和贴图（需要 UE 5.4.4 及以上版本）。
*   **Blender 导出提示:** 当您使用 Blender 导出模型为 FBX 格式时，请确保根骨骼的名称**必须**是 **`Armature`**。

| 格式      | 支持的资产类型                 | 备注                                                                 |
| :-------- | :--------------------------- | :----------------------------------------------------------------- |
| `SEModel` | 静态网格体, 贴图数据     | 目前仅能导入静态网格体。可能可以支持读取材质/贴图信息。 |
| `SEAnim`  | 动画 | 使用 [SeAnimModule](https://github.com/girlgg/SeAnimModule) 插件转换动画。|
| `Cast` | 静态网格体，骨骼网格体, 动画 | **推荐格式。** 提供最全面的导入支持。 |

### shader支持列表

*   **T10**
*   **IW9**

## 兼容性说明

### 版本支持

| 虚幻引擎版本 | 支持情况 |
| :- |  :- |
| **5.4 及之后** | 完全支持 |
| **5.3 及之前** | 不支持 |

## 内置导入器

### 支持的引擎版本

| 支持的 IW 引擎 | 支持的游戏版本 | 支持的资产类型 | 备注 |
| :- | :- | :- | :-  |
| IW9-T10 | CoD 21 - 黑色行动6 | 正在编写中 | 需要`Cordycep`工具Dump游戏 |
| IW9-JUP | CoD20 - 现代战争III 2023 | 模型（支持Lod导入），音频，图片，地图 | 需要`Cordycep`工具Dump游戏 |
| IW9 | CoD19 - 现代战争II 2022 | 正在编写中 | 需要`Cordycep`工具Dump游戏 |
| IW8-S4 | CoD18 - 先锋 | 暂未支持 | 暂无计划 |
| IW3-T9 | CoD17 - 黑色行动：冷战 | 暂未支持 | 暂无计划 |
| IW8 | CoD16 - 现代战争 | 暂未支持 | 暂无计划 |
| IW3-T8 | CoD15 - 黑色行动4 | 暂未支持 | 暂无计划 |
| S2-H1 | 现代战争重制版 | 暂未支持 | 暂无计划 |
| S2-H2 | 现代战争2重制版 | 暂未支持 | 暂无计划 |
| S2 | Cod14 - 二战 | 暂未支持 | 暂无计划 |
| IW7 | CoD13 - 无限战争 | 暂未支持 | 暂无计划 |
| IW3-T7 | CoD12 - 黑色行动III | 暂未支持 | 暂无计划 |
| S1 | CoD11 - 高级战争 | 暂未支持 | 暂无计划 |
| IW 6 | CoD10 - 幽灵 | 暂未支持 | 暂无计划 |
| IW3-T6 | CoD9 - 黑色行动II | 暂未支持 | 暂无计划 |
| IW 5 | CoD8 - 现代战争3 | 暂未支持 | 暂无计划 |
| IW3.0修改版 | CoD7 - 黑色行动 | 暂未支持 | 暂无计划 |
| IW 4 | CoD6 - 现代战争2 | 暂未支持 | 暂无计划 |
| IW 3 | CoD5 - 战争世界 | 暂未支持 | 暂无计划 |
| IW 3 | CoD4 - 现代战争 | 暂未支持 | 暂无计划 |
| Treyarch NGL | CoD3 | 暂未支持 | 暂无计划 |
| IW 2 | CoD2 | 暂未支持 | 暂无计划 |
| id Tech 3 | CoD | 暂未支持 | 暂无计划 |

*   **支持的IW引擎:** 指游戏资产来源的 Infinity Ward 引擎版本。
*   **支持的游戏版本:** 已知兼容的特定游戏构建版本。来自其他版本的资产可能也能工作，但无法保证。
*   **支持的资产类型:** 插件可以在虚幻引擎中导入和创建/处理的资产种类。
*   **支持Lod:** 仅支持部分游戏从内置导入器导入Lod

## 地图导入器

支持从[DotnesktRemastered](https://github.com/girlgg/DotnesktRemastered)导出的json格式的地图的导入

## 待修复问题

*   **物理资产问题:** 当前插件自动创建的物理资产存在形体问题，需要手动重新创建。

### 说明

- 内置导入器初次初始化游戏时将耗费非常多的时间，将会构建文件索引和哈希值。因为插件使用SQLite数据库索引这些信息。
- 将命名哈希（`wni` 格式）放到插件目录下，插件将会自动扫描并更新命名，请到 `Cordycep` 维护者的 discord 频道赞助获取命名值哈希表。

## 致谢 | 引用

资产导出器`Greyhound`：[Scobalula/Greyhound](https://github.com/Scobalula/Greyhound)

Dump工具`Cordycep`：[Scobalula/Cordycep](https://github.com/Scobalula/Cordycep)

Cast格式：[dtzxporter/cast](https://github.com/dtzxporter/cast)

Se格式：[dtzxporter/io_model_semodel](https://github.com/dtzxporter/io_model_semodel)

地图导入：[timing1337/Mappie](https://github.com/timing1337/Mappie)

动画转换：[dest1yo/MDA](https://github.com/dest1yo/MDA)

最初版本Se格式导入：[Unreal-SeTools](https://github.com/djhaled/Unreal-SeTools)

天空盒：[Scobalula/Aether](https://github.com/Scobalula/Aether)

材质和HDR解析：[Rendering of Call of Duty: Infinite Warfare](https://research.activision.com/publications/archives/rendering-of-call-of-dutyinfinite-warfare)

旧版本COD地图导入：[sheilan102/C2M](https://github.com/sheilan102/C2M) [Scobalula/Husky](https://github.com/Scobalula/Husky)

信息数据库：[CoDDB](https://coddb.net/Home) [ImSimpy/BO6-Codenames](https://github.com/ImSimpy/BO6-Codenames) [ImSimpy/MWIII-Asset-Names](https://github.com/ImSimpy/MWIII-Asset-Names)

文件压缩 | 解压：[ate47/HashIndex](https://github.com/ate47/HashIndex) [Scobalula/GreyhoundPackageIndex](https://github.com/Scobalula/GreyhoundPackageIndex)

脚本：[xensik/gsc-tool](https://github.com/xensik/gsc-tool/) [zonetool](https://github.com/ZoneTool/zonetool) [EthanC/Jekyll](https://github.com/EthanC/Jekyll)