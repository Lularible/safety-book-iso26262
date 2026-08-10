# 功能安全 — ISO 26262分析与代码实现

以免疫系统为叙事线索的功能安全技术书。兼顾ISO 26262标准分析、源码拆解与动手实现。

## 在线阅读

[📖 在线浏览本书](https://web-l.github.io/lularible-books/safety-book-iso26262/index.html)


## 这本书讲了什么

全书 44 节，分五章：

- **第一章（6 节）**：哲学地基 — 从免疫系统隐喻开始，途经哥德尔不完备定理、Therac-25 灾难、停机问题，最后用 alpha 粒子和思想实验让你站在安全工程师的视角看世界
- **第二章（18 节）**：ISO 26262 标准深度解析 — HARA、ASIL 判定、SPFM/LFM/PMHF、FMEDA、FFI、MISRA C、MC/DC、ASIL 分解、FTA/DFA，直到安全档案的完整论证链
- **第三章（8 节）**：安全机制逐个拆解 — 看门狗的三重监督、锁步核、ECC 内存保护、MPU/SMU、E2E 保护、CRC、程序流监控、安全状态与降级策略
- **第四章（6 节）**：量产源码走读 — 走进 Arctic Core 的开源实现，看 WdgM、E2E 库、CRC 引擎、RamTst 在工业级代码中如何落地（~6,500 行 C）
- **第五章（6 节）**：动手构建 safe-lite — 从零实现多槽位看门狗、E2E 保护、带 CRC 完整性的环形队列、故障注入框架，最后串联为一次完整的安全回路验证（~1,173 行 C）

## 适合谁读

汽车嵌入式软件工程师、功能安全工程师、系统架构师，以及所有想知道 ISO 26262 不只是"要求什么"、更是"为什么这样要求"和"在 C 代码里怎么实现"的人。

## 许可证

书籍内容：[CC BY-NC-ND 4.0](LICENSE) · safe-lite 源码：MIT

## 姊妹篇

本书是"汽车电子七部曲"系列中的一部。另外六部已发布：

- **[从沙子到车辙——一个工程师的理解](https://github.com/Lularible/from-sand-to-ruts)** — 从图灵机到 CAN 总线，从半导体物理到 AUTOSAR，一部为汽车电子工程师写的全景入门
- **[PTP 技术书——从思想实验到协议实现](https://github.com/Lularible/ptp-book)** — 从时间同步的思想实验开始，到 PTP 协议实现，逐机制拆解 + 动手实践
- **[HSM 技术书——从思想实验到安全基石](https://github.com/Lularible/hsm-book)** — 从岩画密码学到硬件安全模块，完整覆盖车载 HSM 的技术链路
- **[存储 技术书——在不可靠的硬件上构建可靠的数据家园](https://github.com/Lularible/storage-book)** — 一本关于存储技术演进与文件系统实现的深度技术书籍
- **[UDS 技术书——从望闻问切到UDS协议实现](https://github.com/Lularible/uds-book)** — 一本从诊断元问题出发，直通ISO 14229协议规范与AUTOSAR DCM源码、再到亲手实现UDS栈的技术书
- **[汽车嵌入式软件工程——用建筑学隐喻讲工程化](https://github.com/Lularible/swe-book)** — 工程方法论卷：架构原则与质量基础设施，附可运行的 CI 流水线教学项目 eng-lite

## 致谢

- 感谢 [@web-l](https://github.com/web-l) 构建并维护本系列的 [mdBook 在线阅读站](https://web-l.github.io/lularible-books/)，方便了大家阅读。

---

如果觉得有用，点个 ⭐ 就是最好的支持。当然，如果能顺手转发给身边需要的人，那就更棒了。🚗💨
