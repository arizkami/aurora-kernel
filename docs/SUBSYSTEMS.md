# Aurora NT/L4 Hybrid Design Overview

## Goals
- Blend NT-style object, process, and I/O manager model with L4/Fiasco-style minimal microkernel primitives.
- Provide capability-based security for inter-process communication (IPC) and resource control.
- Maintain a clear layering to allow incremental replacement or enhancement of policies.

## Layering
1. Hardware / Arch Layer
   - CPU init, low-level interrupt/trap handling, context switch primitives.
2. Core Kernel (NT-style base)
   - Memory manager (physical/virtual), process & thread tables, scheduler, system call dispatch, object naming TBD.
3. Subsystem Managers
   - I/O Manager (devices, drivers, IRPs), FS drivers, Configuration (Hive/NTCore), Performance, WMI.
4. L4 Adaptation Layer (`l4/`)
   - Thread-local TCB extension with capability table.
   - Basic IPC message registers (4x64-bit) and mailbox semantics.
   - Capability insertion/lookup rights filtering.
5. Fiasco Policy Layer (`fiasco/`)
   - Fastpath IPC (bypass heavier syscall path when possible).
   - Pager hook placeholder and future memory mapping control.
   - Blocking sender queue scaffolding.
6. Future Object Layer (Planned)
   - Unified kernel object header tying NT handles <-> L4 capabilities.
   - Reference counting, security descriptors, name namespace.

## Kernel Object Model (Planned)
Each kernel object will embed a small header:
```
struct AURORA_OBJECT_HEADER {
  UINT16 Type;        // object type index
  UINT16 Flags;       // generic flags
  UINT32 RefCount;    // atomic reference count
  VOID*  Security;    // placeholder for security descriptor
};
```
Capabilities will reference objects via this header; NT-style handles will map to capability slots, enabling both handle duplication logic and L4 rights enforcement with a single source of truth.

## IPC Model
- Messages: fixed small register array currently (MR[4]); plan to add flex pages / buffer descriptors for bulk transfer.
- Send: non-blocking if receiver mailbox empty; else sender enqueued (blocking semantics planned) with eventual message state storage.
- Receive: empties mailbox; triggers wake of one queued sender (fastpath reattempt pending implementation).
- Rights: SEND / RECV / MAP / CTRL. Future: GRANT (transfer mapping), STRING/STREAM rights, ASYNC notify.

## Capability Lifecycle (Planned Extensions)
1. Allocation: Insert into per-thread cap table.
2. Revocation: Iterate all tables (needs global registry) or maintain back-pointers.
3. Derivation: Create reduced-rights child caps.
4. Audit: Enumerate capabilities for diagnostics.

## Security Model Roadmap
- Phase 1: Rights mask enforcement (current).
- Phase 2: Object type-specific rights decomposition (e.g., thread control vs. IPC send).
- Phase 3: Policy modules (LSM-style) hooking capability grant/revoke.

## Testing Strategy
- Context switch integrity test (register save/restore, FPU later).
- IPC roundtrip latency microbenchmarks (baseline for optimization).
- Rights enforcement tests (deny send without SEND right, etc.).
- Stress: spawn many threads with random IPC patterns to test queue correctness.

## TODO Summary (Highlights)
- Persist per-sender pending message when mailbox full.
- Implement receive path wake+retry logic.
- Integrate pager for page fault handling w/ mapping rights.
- Introduce object header + handle-capability harmonization.
- Add interrupt delivery + syscall ABI conformance tests.
- Security descriptors and token model.

---
This document will evolve as the hybrid model solidifies. Contributions should reference the layer they are modifying and keep cross-layer coupling minimal.
