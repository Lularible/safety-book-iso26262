# Functional Safety: ISO 26262 Analysis and Code Implementation

A technical book on functional safety narrated through the metaphor of the immune system. It combines an in-depth reading of ISO 26262, source-code walkthroughs, and hands-on implementation.

<p align="center">
  <a href="../README.md"><img src="../assets/lang-zh.svg" alt="中文" height="30"></a>&nbsp;&nbsp;&nbsp;<a href="README.md"><img src="../assets/lang-en-active.svg" alt="English" height="30"></a>
</p>

> 🌐 Chinese version: [safety-book (中文)](../README.md). Chinese chapters live in [`chapters/`](../chapters/).

## Read Online

[Browse this book online (Chinese)](https://web-l.github.io/lularible-books/safety-book-iso26262/index.html)

## What This Book Covers

The book has 44 sections in five parts:

- **Part 1 (6 sections): A 1.5-Billion-Year Survival Lesson.** Starts from the immune-system metaphor, passes through Gödel's incompleteness theorems, the Therac-25 disaster, and the halting problem, then uses alpha particles and a thought experiment to put you in a safety engineer's shoes.
- **Part 2 (18 sections): The Weights and Measures of Risk.** HARA, ASIL determination, SPFM/LFM/PMHF, FMEDA, FFI, MISRA C, MC/DC, ASIL decomposition, FTA/DFA, and the full argument chain that ends in a safety case.
- **Part 3 (8 sections): Hunting Faults: Eight Gates.** Watchdog triple supervision, lockstep cores, ECC memory protection, MPU/SMU, E2E protection, CRC, program-flow monitoring, and safe states with degradation.
- **Part 4 (6 sections): Production Implementation Under the Microscope.** Walks through Arctic Core's open-source implementation to see how WdgM, the E2E library, the CRC engine, and RamTst land in industrial-grade code (roughly 6,500 lines of C).
- **Part 5 (6 sections): Building an Immune System from Scratch.** Implements a multi-slot watchdog, E2E protection, a ring queue with CRC integrity, and a fault-injection framework from scratch, then wires them into one complete safety-loop verification (roughly 1,173 lines of C).

No prior knowledge of ISO 26262 is required. If you write embedded C for a living, you already have enough background.

## Quick Start

Read online: browse the Markdown files under `en/chapters/`, in section order (1.1 through 5.6).

Recommended: VS Code with the Markdown Preview Enhanced extension, or Typora / Obsidian.

Building and running the teaching code:

```bash
git clone https://github.com/Lularible/safety-book-iso26262.git
cd safety-book-iso26262
make -C safe-lite
./safe-lite/safe-lite
```

The Part 4 walkthroughs use the open-source [openAUTOSAR classic-platform](https://github.com/openAUTOSAR/classic-platform) project. It is not vendored in this repository. Clone it separately as `classic-platform/` if you want to follow the source line by line:

```bash
git clone https://github.com/openAUTOSAR/classic-platform.git classic-platform
```

The safe-lite source code is part of this repository under `safe-lite/`.

## License

Book content: [CC BY-NC-ND 4.0](../LICENSE) · safe-lite source code: MIT

## Companion Books

This book is one volume of the "Automotive Electronics Septet" series. The other six volumes are already published:

- **[From Sand to Ruts — An Engineer's Understanding](https://github.com/Lularible/from-sand-to-ruts)** — From Turing machines to the CAN bus, from semiconductor physics to AUTOSAR; a panoramic primer written for automotive electronics engineers.
- **[PTP — From Thought Experiments to a Working Implementation](https://github.com/Lularible/ptp-book)** — Starts from thought experiments about time synchronization, then dismantles the PTP protocol mechanism by mechanism and builds an implementation by hand.
- **[HSM — From Thought Experiments to a Security Cornerstone](https://github.com/Lularible/hsm-book)** — From cave-painting cryptography to hardware security modules, covering the full technical chain of the automotive HSM.
- **[Storage — Building a Reliable Data Home on Unreliable Hardware](https://github.com/Lularible/storage-book)** — An in-depth technical book on the evolution of storage technology and the implementation of filesystems.
- **[UDS — From Diagnosis to a UDS Protocol Implementation](https://github.com/Lularible/uds-book)** — Starting from the meta-questions of diagnostics, straight through the ISO 14229 specification and the AUTOSAR DCM source code, to building a UDS stack by hand.
- **[Automotive Embedded Software Engineering — Engineering Through an Architectural Metaphor](https://github.com/Lularible/swe-book)** — The engineering-methodology volume: architecture principles and quality infrastructure, with a runnable CI-pipeline teaching project, eng-lite.

## Acknowledgements

- Thanks to [@web-l](https://github.com/web-l) for building and maintaining the [mdBook site](https://web-l.github.io/lularible-books/) for this series, which makes it easier for everyone to read.

---

If you find it useful, a ⭐ is the best support. And if you can pass it on to someone who needs it, even better. 🚗💨
