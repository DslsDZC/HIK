# HIK
HIK (Hierarchical Isolation Kernel) Reference Document
Author: DslsDZC
Email: dsls.dzc@gmail.com
1. Design Philosophy and Fundamental Document he HIK (Hierarchical Isolation Kernel) is designed to establish a unified architectural paradigm capable of adapting to environments ranging from resource-constrained embedded devices to complex general-purpose computing systems. Its core objective is to achieve a tripartite synthesis of extreme performance, robust security isolation, and dynamic scalability within a single structural framework.
Core Design Principles:
 * Complete Separation of Mechanism and Policy:
   * The Kernel Core (Core-0) provides only atomic, policy-free execution primitives, isolation mechanisms, and a capability system.
   * All system policies and functional implementations are provided by modular services in the Privileged-1 Layer, customized via build-time configuration or runtime dynamic management.
 * Logical Three-Tier Privilege Architecture:
   * Establishment of three logical control flows: Untrusted Applications (Ring 3), Modular Privileged Service Sandboxes (Logical Ring 1), and the Kernel Core/Arbiter (Logical Ring 0).
   * This model utilizes Memory Management Units (MMU) and a software capability system within the highest physical privilege level (e.g., x86 Ring 0) to create strongly isolated "in-kernel processes," unifying isolation and performance.
 * Unified Architecture, Resilient Deployment:
   * The architecture supports two deployment modes:
     * Static Synthesis Mode: For embedded, real-time, or high-security scenarios, all service modules are compiled and linked at build-time into a single, deterministic kernel image.
     * Dynamic Modular Mode: For general-purpose, desktop, or server scenarios, core services are statically compiled while extended services (e.g., drivers, filesystems, protocol stacks) can be securely installed, updated, or uninstalled at runtime via network repositories.
 * Robust Backward Compatibility and API Evolution:
   * The kernel and Privileged-1 services provide a stable User-Mode ABI (Application Binary Interface).
   * Through modular services and user-mode compatibility libraries, the system provides transparent support for legacy APIs while introducing new ones, ensuring long-term ecosystem stability.
2. Three-Tier Model Architecture (Physical Space Direct Mapping)
This section details the core architectural model of HIK, specifically the design of the Privileged-1 layer utilizing direct physical space mapping and allocation constraints.
2.1 Core-0 Layer: Kernel Core and Arbiter
 * Physical Execution Environment: Highest CPU privilege mode (x86 Ring 0, ARMv8-A EL1 or EL2).
 * Logical Role: The system's Trusted Computing Base (TCB), the final arbiter of resource access, and the enforcer of isolation mechanisms.
 * Design Goal: Minimalist and amenable to formal verification. The code size (excluding architecture-specific assembly and auto-generated data) is strictly limited to under 10,000 lines of C code.
Core Responsibilities and Implementation Details:
 * Physical Resource Management and Allocation:
   * Management of global bitmaps for all physical memory frames, CPU time slices, and hardware interrupt lines.
   * Adoption of a direct physical memory allocation strategy, avoiding traditional virtual memory overhead. Each isolation domain (including itself, Privileged-1 services, and Applications) is allocated independent, contiguous physical memory regions isolated via MMU.
   * Core-0's own code and data reside in a fixed physical region marked inaccessible by all other domain page tables.
 * Capability System Kernel:
   * Maintenance of a global, protected capability table. Each entry (capability) is an unforgeable kernel object describing specific access rights (e.g., read, write, execute) to a system resource (e.g., a physical memory range, I/O port, or IPC endpoint).
   * Each domain is associated with a "Capability Space"—an array of indices (handles) pointing to held capabilities. Domains can only request resource operations through these handles.
   * Provision of system calls for passing, deriving (creating permission subsets), and revoking capabilities between domains.
 * Execution Control and Scheduling:
   * Management of all Thread Control Blocks (TCB). Each thread is bound to a specific isolation domain.
   * Implementation of a predictable, real-time friendly scheduler based on static priorities or round-robin policies.
   * Handling of all exceptions and hardware interrupts. Interrupts are first delivered to Core-0, which routes them to the registered Privileged-1 service handlers via protected entry points.
 * Isolation Enforcement:
   * When creating a Privileged-1 domain, Core-0 configures page tables to map only:
     * a) The service's own code and private data segments.
     * b) Shared physical memory or MMIO regions explicitly authorized by capabilities.
   * Service domains cannot modify their own page tables. Unauthorized access triggers exceptions handled by Core-0.
   * Control flow transfers between services must pass through Core-0-defined synchronous call gates, which verify endpoint capabilities and perform stack switching.
2.2 Privileged-1 Layer: Privileged Service Sandboxes
 * Physical Execution Environment: Same as Core-0 (x86 Ring 0, ARMv8-A EL1/EL2).
 * Logical Role: Modular providers of system functionality. Each service is an independent, mutually untrusted "kernel-mode process."
 * Key Design: No traditional virtual address space.
   * Direct Physical Mapping: Services operate directly on allocated contiguous physical memory. Memory access is identity-mapped (VA = PA offset), eliminating virtual memory management overhead and ensuring deterministic latency.
   * Physical Space Isolation: Service regions do not overlap and are isolated via MMU page table permissions.
   * Resource Allocation Constraints: Total physical memory, CPU time, and I/O ports are subject to hard quotas via the capability system.
Core Features:
 * Independent Physical Address Space: Simplifies internal memory management and avoids non-deterministic performance hits.
 * Capability-Based Resource Access: All hardware or shared memory access is verified by Core-0 via the capability system before instruction execution.
 * Fault Isolation and Recovery: Service crashes (illegal instructions, page faults) are confined to their physical space. Core-0 terminates the domain's threads, reclaims resources, and notifies a "Monitor Service" for potential restart.
 * Efficient Communication:
   * With Application: Bulk, zero-copy data exchange via capability-authorized shared physical memory (using lock-free ring buffers).
   * With Core-0: Triggered via syscall for capability verification and routing.
   * Inter-Service: Indirect (Core-0 mediated control flow) or Direct (Zero-copy data transfer via shared memory).
2.3 Application-3 Layer: Application Layer
 * Physical Execution Environment: Low CPU privilege (x86 Ring 3, ARMv8-A EL0).
 * Logical Role: Execution environment for untrusted user code.
 * Core Features:
   * Runs in standard process abstractions with independent virtual address spaces managed by Core-0.
   * All resource requests must go through OS APIs (pointing to Privileged-1 services) and pass Core-0 authorization.
   * No direct memory access exists between applications or between applications and privileged services; communication occurs via kernel-established IPC channels.
3. Analysis of Core Security Mechanisms
3.1 Capability Lifecycle and Secure Transfer
Capabilities are the carriers of all authorization in HIK. Security relies on Core-0’s interpretation rather than storage location.
 * Generation: Created at build-time by the hardware synthesis system or at runtime by Core-0 in response to resource requests.
 * Storage: The "Master Copy" resides in Core-0's protected global table. Domains hold obfuscated indices (handles) to these entries.
 * Transfer and Revocation:
   * Send: Core-0 verifies the sender's ownership and the receiver's eligibility, potentially performing "permission attenuation" (e.g., Read/Write to Read-Only) before granting the receiver a new local handle.
   * Revocation: When a domain is destroyed or a capability revoked, Core-0 removes the global entry, immediately invalidating all associated handles.
3.2 Unified API Access and Secure Communication
 * API Gateway: Privileged-1 services register "Service Endpoint Capabilities." Callers use ipc_call(cap_endpoint, message) to initiate requests.
 * Call Path: Core-0 intercepts the call, validates the capability, and securely switches the context (registers/message) to the target service domain.
 * Shared Memory Establishment: Core-0 arbitrates the process by allocating physical memory and mapping it into the caller’s virtual space and the service’s physical space with specific permissions (e.g., Caller: Write-Only; Service: Read-Only).
4. Build-Time Hardware Synthesis System
This phase converts hardware uncertainty into software determinism. The input is a machine-readable hardware description (e.g., platform.yaml).
Processing Flow:
 * Resolution: Checks for resource conflicts (IRQs, MMIO) and resolves them based on policy.
 * Privileged Code Generation: Generates/selects optimized driver code and links it with service frameworks to create independent Privileged-1 modules.
 * Static Configuration Generation: Produces memory layout tables, interrupt routing tables, and initial capability allocation tables.
 * Final Image Synthesis: Links Core-0, all generated modules, and configuration data into a single, bootable kernel image.
5. Dynamic Device Support and Modular System
To handle hot-pluggable devices (USB, NVMe), HIK provides a controlled expansion mechanism.
 * Dynamic Resource Pool: A portion of resources (MSI-X vectors, PCIe BAR space) is reserved at build-time for a "Hot-Swap Coordination Service."
 * Secure Driver Modules: Signed binary modules containing resource declarations and interface specifications.
 * Workflow: Upon device insertion, Core-0 routes the interrupt to the Coordination Service, which verifies the driver’s signature, requests Core-0 to create a new Privileged-1 sandbox, allocates resources from the dynamic pool, and initializes the driver.
6. Modular Service Architecture and Rolling Updates
6.1 Service Module Format (.hikmod)
Modules include a header (metadata, UUID, versioning), code/data segments, and a cryptographic signature (e.g., SHA-384) for integrity and origin verification.
6.2 Module Manager Service
This Privileged-1 service manages the full lifecycle of modules, including repository interaction, dependency resolution, signature verification, and Rolling Updates (Hot Upgrades). Hot upgrades allow a new service version to start in parallel, migrate states from the old instance, and take over without application downtime.
6.3 API Versioning
HIK supports concurrent execution of multiple API versions (e.g., filesystem.v1 and filesystem.v2). Clients can bind to specific versions, and adapter modules can be used to translate legacy calls to new service interfaces.
8. Resource Management and Protection Mechanisms
8.4 Defense Against Resource Flooding (DoS)
HIK employs multi-layered defense to prevent denial-of-service via resource exhaustion:
 * Build-Time Quotas: Hard, immutable limits on memory, CPU, and I/O for every service.
 * Hierarchical Delegation: Services must "carve out" sub-quotas from their own allocation for clients, ensuring one rogue client cannot exhaust the service's total resources.
 * Adaptive Rate Limiting: Core-0 monitors resource allocation rates and can introduce latency or reject requests from suspicious domains.
 * Flow Control: IPC endpoints utilize credit-based flow control and backpressure to prevent fast producers from overwhelming slow consumers.
 * Atomic Reclamation: Upon crash, Core-0 immediately and atomically reclaims all capabilities and resources to prevent leaks.
9. Performance Optimization and Expected Metrics
 * Syscall/Service Call Latency: Targeted at 20–30 ns due to the absence of privilege/page-table switching in Privileged-1 calls.
 * Interrupt Latency: Targeted at 0.5–1 μs via direct routing to Privileged-1 ISRs.
 * Thread Switching: Targeted at 120–150 ns for intra-domain switches.
11. Architecture Comparison Summary
| Feature | Linux (Monolithic) | seL4 (Microkernel) | HIK (Hierarchical Isolation) |
|---|---|---|---|
| Driver Location | Kernel Space (Ring 0) | User Space (Ring 3) | Privileged Sandbox (Phys Ring 0) |
| TCB Size | Massive | Minimal | Small (Core-0 + Capability Sys) |
| Performance | Optimal | High Overhead (IPC) | Near-Monolithic |
| Isolation | Weak | Strong | Strong (Hardware + Software) |
| Fault Impact | System Crash | Service Crash | Sandbox Crash (Quick Recovery) |
12. Simplified Design for Non-MMU Architectures
For resource-constrained MCUs (e.g., ARM Cortex-M), HIK employs a Physical Space Flat Mapping Model.
 * Static Layout: Domains are assigned fixed, non-overlapping physical regions at build-time.
 * Protection Alternatives: Utilization of MPU (Memory Protection Unit) or pure software validation.
 * Communication Adjustment: Zero-copy shared memory is replaced by Core-0 mediated message copying due to the lack of virtual mapping flexibility.
 * Trade-offs: This mode provides high determinism for hard real-time scenarios but lacks the dynamic loading and high-level fault isolation found in MMU-enabled systems.
Would you like me to generate a specific technical implementation guide for the Core-0 capability table or the hardware description YAML format?

