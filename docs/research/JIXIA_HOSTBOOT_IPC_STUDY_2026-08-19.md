# Hostboot IPC/Message Source Study for M00-08.03

- **Date:** 2026-08-19
- **Branch:** `agent/m00-08-03-research`
- **Reference:** OpenPOWER Hostboot `release-fw1120`, pinned commit
  `22e3c409ab8b439d4c8eb31b644acb498032a487` (local checkout
  `/home/pedroa/workspace/source/hostboot`)
- **Status:** RESEARCH input for `M00-08.03 Message IPC Foundation`
- **License note:** Apache-2.0 source studied for architecture reconstruction only.
  No Hostboot code is copied into Jixia. Facts below are paraphrased behavior with
  file references, not source text.

## 0. Sources inspected

| File | Role |
|---|---|
| `src/include/sys/msg.h` | user ABI: `msg_t`, message types, syscall prototypes |
| `src/include/kernel/msg.H` | kernel `MessageQueue` representation (data only) |
| `src/kernel/syscall.C` | all message syscalls: MSGQ_CREATE/DESTROY/REGISTER_ROOT/RESOLVE_ROOT, MSG_SEND, MSG_SENDRECV, MSG_RESPOND, MSG_WAIT; MM_ALLOC_BLOCK |
| `src/lib/syscall_msg.C` | user-side wrappers, VFS-name registration via messaging itself, `msg_wait_timeout` design contract |
| `src/kernel/msghandler.C` (+ `.H`) | kernel-to-userspace `MessageHandler` deferred-response engine |
| `src/kernel/blockmsghdlr.C` (+ `.H`) | `BlockReadMsgHdlr`/`BlockWriteMsgHdlr` resource-provider response handling |
| `src/kernel/block.H`, `src/kernel/block.C` | VMM block ↔ resource-provider wiring |
| `src/kernel/doorbell.C` | POWER doorbell IPI helpers |
| `src/kernel/futexmgr.C` | futex wait/wake (comparison model) |
| `src/kernel/ipc.C` | multi-node (inter-drawer) IPC transport |
| `src/kernel/task.H` | `TASK_STATE_*` block states |
| `src/usr/testcore/kernel/msgtest.H` | messaging test cases |

Note: `src/kernel/msg.C` / `msg.H` named in the task brief do **not exist** in
`release-fw1120`. The kernel queue type lives in `src/include/kernel/msg.H` and
all operations are implemented inline in the syscall handlers of
`src/kernel/syscall.C`. `src/kernel/ipc.C` is the *remote-node* transport, not the
local queue implementation.

## 1. Method

Each subsystem is reconstructed as: objects and fields, then per-operation
**State / Owner / Valid transitions / Protected by / Publication point /
Lifetime / Observers**. Sections 2 (facts), 3 (inference), 4 (Jixia design
implications) are kept strictly separated.

---

# 2. FACT FROM HOSTBOOT SOURCE

## 2.1 Message representation (`msg_t`, `src/include/sys/msg.h`)

- `msg_t` fields: `uint32_t type`; a 32-bit reserved-bit word whose low bits are
  `__reserved__async` and `__reserved__pseudosync`; `uint64_t data[2]`;
  `void* extra_data` (variable-length payload follows `data[0]` by convention).
- Kernel-reserved types start at `MSG_FIRST_SYS_TYPE = 0x80000000`
  (`MSG_MM_RP_READ/WRITE/PERM`, `MSG_INTR_*`). User sends of kernel types are
  rejected with `-EINVAL`.
- **Flag polarity (counter-intuitive, verified in syscall handlers):**
  - `msg_send` sets `__reserved__async = 0` → asynchronous message.
  - `msg_sendrecv` sets `__reserved__async = 1` → synchronous message.
  - `msg_is_async(m)` returns true when the bit is 0.
  - `msg_sendrecv_noblk` additionally sets `__reserved__pseudosync = 1`.
- Messages are heap objects in the sender's shared-virtual space. `msg_allocate`
  is user-side `contiguous_malloc`; `msg_free` is plain `free`. The kernel
  traverses these pointers directly (single shared address space).

## 2.2 Kernel queue representation (`src/include/kernel/msg.H`)

`class MessageQueue` is a pure data holder with exactly four members:

1. `Spinlock lock` — one lock for the whole queue.
2. `Util::Locked::List<MessagePending, msg_t*> messages` — FIFO of undelivered
   messages, keyed by the `msg_t*`.
3. `Util::Locked::List<MessagePending, msg_t*> responses` — outstanding
   received-but-not-responded synchronous transactions, keyed by `msg_t*`.
4. `Util::Locked::Queue<task_t> waiting` — FIFO of receiver tasks blocked in
   `msg_wait` on this queue.

`MessagePending` = `{ msg_t* key; task_t* task; prev; next }`. The `task` field
is **overloaded by transaction kind** (fact, see 2.6–2.8): blocked client task
(normal sync), client's secondary queue reinterpreted as `task_t*`
(pseudo-sync), or the `MessageHandler*` object (kernel-consumed responses).

## 2.3 Queue lifecycle syscalls

- **MSGQ_CREATE**: returns `new MessageQueue()` — the raw kernel pointer *is*
  the user-visible handle `msg_q_t` (a `void*`).
- **MSGQ_DESTROY**: `delete mq` unconditionally. No check for pending
  messages, pending responses, or tasks in `waiting`. Dangling handles are not
  detected anywhere else (no generation/validation on use).
- **MSGQ_REGISTER_ROOT / RESOLVE_ROOT**: only two kernel-known well-known
  queues exist, `MSGQ_ROOT_VFS` and `MSGQ_ROOT_INTR`, stored in file-static
  pointers in `syscall.C`. Everything else is named via the *VFS server itself*:
  `msg_q_register(name)` in `lib/syscall_msg.C` resolves the root VFS queue and
  sends it a `VFS_MSG_REGISTER_MSGQ` synchronous message whose `extra_data`
  carries the name string. Naming is a userspace service layered on IPC, not a
  kernel registry.
- `msg_intr_q_register(q, ipc_base)` registers the interrupt queue and creates
  `InterruptMsgHdlr` bound to the inter-node IPC MMIO base.

## 2.4 `msg_send` — asynchronous send (`MsgSend`, syscall.C)

Behavior (local case):

1. Validate `q` and `msg` non-NULL (`-EINVAL`), reject kernel-range `type`.
2. Set `__reserved__async = 0`.
3. `mq->lock.lock()`.
4. Pop one task from `mq->waiting` (server blocked in `msg_wait`).
   - **No waiter:** allocate `MessagePending{key=msg, task=sender}`, insert into
     `mq->messages`. Sender keeps running.
   - **Waiter found:** set waiter's syscall return value to the `msg_t*`, push
     waiter onto its scheduler (`scheduler->addTask(waiter)`), then
     `doorbell_broadcast()`. Sender keeps running.
5. `mq->lock.unlock()`, return 0.

Remote-node case: if bits of the handle indicate `MSGQ_TYPE_IPC`, the message is
  routed to `KernelIpc::send` (see 2.13) instead.

| Property | Value |
|---|---|
| State touched | `messages` list, `waiting` queue, waiter task state (via scheduler) |
| Owner | sender owns msg until enqueued; queue owns msg after enqueue |
| Valid transitions | queue: {messages++, waiter--} or {waiter→READY} atomically under lock |
| Protected by | `mq->lock` |
| Publication point | the locked critical section: enqueue + wake decision are one atomic region |
| Lifetime | msg freed by *consumer* (`msg_free` after `msg_wait` handling) |
| Observers | any hart in `msg_wait`/`msg_send`/`msg_respond` on the same queue |

## 2.5 `msg_wait` — blocking receive (`MsgWait`, syscall.C)

1. `mq->lock.lock()`; pop `MessagePending` from `messages`.
2. **Queue empty:** insert calling task into `mq->waiting`; set
   `state = TASK_STATE_BLOCK_MSG`, `state_info = mq`; then
   `scheduler->setNextRunnable()` (caller deschedules *inside* the handler);
   unlock. Task's syscall return value is later filled by the waker.
3. **Message present:** `m = mp->key`;
   - if `m->__reserved__async` (sync transaction): *move* `mp` into
     `mq->responses` — the server now owes a response for `m`;
   - else (async): `delete mp` — transaction complete.
   - Return `m` as the syscall return value; unlock; caller keeps running.

| Property | Value |
|---|---|
| State touched | `messages`, `waiting`, task state, scheduler current task |
| Owner | receiver task owns queue-position until woken or message dequeued |
| Valid transitions | task: RUNNING→BLOCK_MSG (empty) or stays RUNNING (message ready) |
| Protected by | `mq->lock` for queue fields; scheduler lock for run queues |
| Publication point | BLOCK_MSG + `waiting` insertion visible under `mq->lock` before unlock |
| Lifetime | task remains blocked until a matching send/respond fills its return value |
| Observers | senders on the same queue pop it from `waiting` |

Note there is no timeout or cancellation: a blocked `msg_wait` can only be
released by a message arriving on that queue.

## 2.6 `msg_sendrecv` — blocking synchronous call (`MsgSendRecv`, syscall.C)

Arguments: server queue `mq`, message `m`, optional secondary queue `mq2`
(only `msg_sendrecv_noblk` passes one).

1. Set `__reserved__async = 1`; set `__reserved__pseudosync = 1` iff `mq2 != NULL`.
2. Reject kernel-range `type` (`-EINVAL`).
3. Allocate `MessagePending mp{key=m}`. Then, by kind:
   - **Normal sync:** `mp->task = caller`; `caller->state =
     TASK_STATE_BLOCK_MSG`; `caller->state_info = mq`.
   - **Pseudo-sync:** `mp->task = (task_t*)mq2`; caller's syscall return is
     preset to 0 (client continues immediately).
4. `mq->lock.lock()`; pop a server from `mq->waiting`:
   - **No waiter:** `mq->messages.insert(mp)`. For normal sync, immediately
     `caller->cpu->scheduler->setNextRunnable()` (client deschedules now).
     For pseudo-sync the client keeps running.
   - **Waiter found:** waiter's syscall return value := `m`; `mq->responses.insert(mp)`;
     `waiter->cpu = caller->cpu` (server migrates onto client's CPU);
     for pseudo-sync, re-queue caller (`addTask` + `doorbell_broadcast()`);
     then `TaskManager::setCurrentTask(waiter)` — **direct context switch to
     the server inside the syscall**; unlock.

| Property | Value |
|---|---|
| State touched | msg reserved bits, `messages`/`responses`/`waiting`, client task state, waiter `cpu`, scheduler current |
| Owner | queue owns msg + transaction record; server owns the obligation to respond |
| Valid transitions | client RUNNING→BLOCK_MSG (normal) or stays RUNNING (pseudo); waiter→RUNNING |
| Protected by | `mq->lock` (queue mutation + wake decision); scheduler lock (run queue) |
| Publication point | single `mq->lock` critical section that both inserts `mp` and picks the wakeup path |
| Lifetime | `mp` lives until `msg_respond` erases it; msg returned to client (normal) or mq2 (pseudo) |
| Observers | server `msg_wait`/`msg_respond`; client scheduler |

The fast path is a **direct handoff**: client → server without passing through
the scheduler. `waiter->cpu = t->cpu` makes the server inherit the caller's CPU
so the scheduler accounting stays consistent.

## 2.7 `msg_respond` — server reply (`MsgRespond`, syscall.C)

1. `mq->lock.lock()`; `mp = mq->responses.find(m)` (lookup by the `msg_t*`
   the server received).
2. **Not found → `-EBADF`** (“message was not sent synchronously”): this is the
   duplicate-reply and wrong-token rejection path.
3. Found: `waiter = mp->task`; erase `mp`; unlock; `delete mp`. Then by kind:
   - **Kernel-type message** (`m->type >= MSG_FIRST_SYS_TYPE`): `waiter` is a
     `MessageHandler*`; call `((MessageHandler*)waiter)->recvMessage(m)`; if
     that handler switched the current task, re-queue the responder
     (`addTask` + `doorbell_broadcast()`).
   - **Pseudo-sync:** `waiter` is the client's secondary `MessageQueue* mq2`;
     lock `mq2`; deliver `m` there (to a blocked client via return value +
     `addTask`, or into `mq2->messages`), same shape as `msg_send`.
   - **Normal sync:** `waiter` is the blocked client task:
     `waiter->cpu = responder->cpu; TaskManager::setCurrentTask(waiter)` —
     **direct switch back to the client**, with the responder re-queued
     (`addTask(t)` + `doorbell_broadcast()`).

| Property | Value |
|---|---|
| State touched | `responses`, woken client task, scheduler current, `mq2` state (pseudo) |
| Owner | responder owns reply until `responses.find` succeeds (single-use token) |
| Valid transitions | response-record→freed; client BLOCK_MSG→RUNNING |
| Protected by | `mq->lock` for the token lookup/erase; `mq2->lock` for pseudo delivery |
| Publication point | the successful `responses.erase(mp)` under `mq->lock` — reply is linearized exactly once |
| Lifetime | token consumed by this operation; duplicate use returns `-EBADF` |
| Observers | any duplicate responder sees `-EBADF` after consumption |

## 2.8 Asynchronous vs synchronous semantics summary

| Interface | sender blocks? | delivery | reply path | msg ownership |
|---|---|---|---|---|
| `msg_send` | never | queue or immediate wake | none | consumer frees |
| `msg_sendrecv` | until respond | direct switch or queue | response record in `responses` | client frees after reply |
| `msg_sendrecv_noblk` | never | queue or wake | reply relayed onto `q2` | client frees after `msg_wait(q2)` |
| `msg_wait` | until message | pops `messages` | — | caller frees |
| `msg_respond` | never | wakes/relays client | consumes token | responder fills, client frees |

## 2.9 Scheduler wakeups and CPU migration

All wake paths use exactly two scheduler calls: `scheduler->addTask(t)` (make
READY on `t->cpu`'s queue) and `scheduler->setNextRunnable()` (deschedule
caller, pick next), plus `doorbell_broadcast()` after remote-queue wakeups.
Direct handoffs additionally use `TaskManager::setCurrentTask(target)` and set
`target->cpu = current->cpu`. The wake decision (pop waiter vs enqueue
message) is always taken under the destination queue's lock, so a message can
never be enqueued while its intended waiter is mid-enqueue — the lost-wakeup
class is prevented by construction (see doc 2, scenario 13).

## 2.10 Task block states (`src/include/kernel/task.H`)

- `TASK_STATE_BLOCK_MSG 'M'` — blocked in `msg_wait` or `msg_sendrecv`;
  `state_info` = the `MessageQueue*`.
- `TASK_STATE_BLOCK_USRSPACE 'u'` — blocked on a kernel `MessageHandler`
  pending response; `state_info` = the handler key (e.g. virtual address).
- `TASK_STATE_BLOCK_FUTEX 'f'` — `state_info` = kernel address of futex.
- `TASK_STATE_BLOCK_SLEEP 's'`, `TASK_STATE_BLOCK_JOIN 'j'`, plus R/r/E.
- The Jixia `task.h` enum already mirrors these character values exactly.

## 2.11 Kernel → userspace `MessageHandler` flow (`msghandler.C/.H`)

Purpose: the kernel itself is a *synchronous client* of a userspace service
(e.g. the VMM Resource Provider, interrupt distribution).

`MessageHandler::sendMessage(type, key, data, task)`:

1. Allocate `MessageHandler_Pending{key, task}`; if `task != NULL`, set
   `task->state = TASK_STATE_BLOCK_USRSPACE`, `state_info = key`.
2. If no request is already outstanding for this key (`!iv_pending.find(key)`),
   synthesize a kernel `msg_t` with `__reserved__async = 1` and a
   `MessagePending` whose `task` field is the `MessageHandler*` itself, and
   deliver it on the handler's bound queue `iv_msgq` exactly like
   `msg_sendrecv`'s enqueue side (waiter wake or `messages` insert).
3. If `task` was the current task: switch to the woken server directly or
   `setNextRunnable()`; if the woken server is on another CPU: re-queue current
   and `setCurrentTask(server)` + `doorbell_broadcast()`.
4. Insert the pending record (deduplication: many blocked tasks may share one
   outstanding kernel message per key).

`MessageHandler::recvMessage(msg)` (called from `MsgRespond` when a
kernel-type response arrives):

1. Reject non-kernel types (`-EINVAL`).
2. Lock the subsystem spinlock `iv_lock`; iterate **all** pending records
   matching the response key; for each, run the virtual `handleResponse()`.
3. `handleResponse` returns SUCCESS (resume task), `UNHANDLED_RC` (resume or
   kill by rc), or `CONTINUE_DEFER` (keep blocked).
4. Resume path: first unpinned deferred task becomes the *current* task
   directly; others go through `addTask` + `doorbell_broadcast()`. Kill path
   collects tasks and calls `TaskManager::endTask(..., TASK_STATUS_CRASHED)`
   **outside** the spinlock; `delete i_msg` frees the synthetic message.

| Property | Value |
|---|---|
| State touched | subsystem pending list, blocked task states, scheduler, `iv_msgq` |
| Owner | `MessageHandler` subclass owns the transaction; subsystem lock owns pending list |
| Valid transitions | task RUNNING→BLOCK_USRSPACE→(RUNNING|ENDED); one kernel msg per key |
| Protected by | `iv_lock` (subsystem) + queue lock; documented deadlock avoidance: never printk-backtrace while holding the VMM lock |
| Publication point | `iv_pending.insert(mhp)` after the queue critical section; response linearized by key lookup under `iv_lock` |
| Lifetime | pending record freed on matching response; synthetic msg freed by `recvMessage` |
| Observers | all blocked tasks sharing the key wake together |

## 2.12 VMM Resource Provider message flow (`block.C`, `blockmsghdlr.C`, `vmmmgr.C`)

FACT chain: `MmAllocBlock` syscall takes a **userspace message queue** `mq` and
creates a paging `Block` whose read handler is
`BlockReadMsgHdlr(VmmManager::getLock(), i_msgQ, this)`; on a page fault the
block calls `iv_readMsgHdlr->sendMessage(MSG_MM_RP_READ, vaddr, page_target,
faulting_task)`. `data[0]` of the request is the faulting virtual address,
`data[1]` the fill destination. The provider (e.g. `pnorrp.C` VFS RP)
`msg_wait`s on its queue, reads pflash/PNOR, and `msg_respond`s with
`data[1] = rc`. `BlockReadMsgHdlr::handleResponse` then calls
`iv_block->attachSPTE(key)` to publish the mapping and returns SUCCESS, which
resumes the faulting task. `BlockWriteMsgHdlr` additionally tracks
per-task write-message counts (`CONTINUE_DEFER` until the last write of a task
completes) and frees written-back pages via a va→pa list.

So the page-fault path is: fault → kernel blocks task (`BLOCK_USRSPACE`) →
kernel-issued sync message → userspace RP fills → `msg_respond` →
`handleResponse` → attach translation → resume task. The kernel never copies
user buffers here; it passes its *own* kernel addresses in `data[1]`.

## 2.13 Doorbell and remote-node IPC

- `doorbell_send(pir)` = `msgsnd` instruction to one thread; `doorbell_clear()`
  = `msgclr`; `doorbell_broadcast()` is **entirely commented out (no-op)** in
  this release (RTC 152189) — cross-CPU wakeup relies on the scheduler queues
  being polled at the next interrupt/timeslice boundary.
- `kernel_execute_hyp_doorbell()` (syscall.C) runs the doorbell work-item
  stack, forwards incoming inter-node IPC to the master CPU's interrupt
  service via `InterruptMsgHdlr::sendIpcMsg(pir)`, executes deferred work,
  and asserts the current task did not change (interrupt context must not
  reschedule). POWER ISA note in source: `msgsync()` before consuming data
  written by the doorbell-sender (memory-ordering contract).
- `KernelIpc::send` (ipc.C): remote queues are encoded in the 64-bit handle
  (destination node in bits 29:31, `MSGQ_TYPE_IPC` tag). The sender CAS-locks a
  **shared per-node mailbox slot** (`msg_queue_id`), copies the fixed-size
  `msg_t` payload, `lwsync()`, publishes the queue id, then doorbells the
  remote PIR and frees the caller's message. `msg_send`'s libc wrapper retries
  on `-EAGAIN` (mailbox busy). Single-slot mailbox: one in-flight inter-node
  message per node pair until consumed.

## 2.14 Futex manager (comparison)

`FutexManager::_wait`: under one global lock, re-check `*addr != val` →
`EWOULDBLOCK` (userspace retries), else insert waiter, set
`TASK_STATE_BLOCK_FUTEX`, unlock, `setNextRunnable()`. The kernel translates
the user virtual address with `VmmManager::findKernelAddress` first (it never
dereferences the raw user pointer). `_wake`: find/erase waiters, `addTask`,
`doorbell_broadcast()` (no-op), optional requeue to a second futex.

## 2.15 Test-side behavioral contract (`msgtest.H`, `syscall_msg.C`)

- Sync round trip is exercised client→server→client with `msg_sendrecv` +
  `msg_respond` and in-place response mutation of the same `msg_t`.
- `msg_wait_timeout` documents, in comments, the *fundamental* constraint that
  **there is no other way to unblock a `msg_wait` than a message on that same
  queue**, that tasks cannot be killed while blocked, and that its timeout is
  therefore implemented by a helper task sending a timeout message into the
  queue, with a TOCTOU race the design must render benign.

---

# 3. INFERENCE (not stated in source; derived from it)

1. **The raw-pointer ABI is sound only because Hostboot has one shared virtual
   address space with no isolation between “user” tasks and kernel.** Every
   handle (`msg_q_t`) is a raw kernel pointer; every message is a sender-heap
   object the kernel dereferences and mutates (`TASK_SETRTN(waiter, m)` hands
   the same pointer to another task). This is a *cooperative firmware image*
   model, not a protection boundary.
2. **The single per-queue lock is the entire correctness argument.** All three
   lists plus the wake decision are serialized; the lost-wakeup hazard is
   eliminated because a sender can never observe “empty queue” between a
   receiver's emptiness check and its `waiting` insertion — those two steps are
   one critical section.
3. **Direct handoff (`setCurrentTask`) is an optimization that couples IPC to
   the scheduler.** It transfers CPU affinity (`waiter->cpu = t->cpu`) to keep
   run-queue accounting consistent; removing it would cost one scheduling
   latency but not correctness.
4. **`responses` keyed by `msg_t*` is a capability token**, but an unforgeable
   one only under the single-address-space trust model. In an isolated U-mode
   it would be guessable/forgeable (any pointer value).
5. **`MsgQDestroy` is unsafe by design** (delete regardless of waiters/
   transactions). Hostboot tolerates this because destruction is only used in
   orderly teardown paths in practice.
6. **The no-op `doorbell_broadcast` means fw1120 IPC wakeup is only eventually
   visible**: a woken task on another CPU runs when that CPU next takes an
   interrupt/timeslice, not immediately. Hostboot accepts this latency.
7. **Deduplicated kernel messages (`iv_pending` keyed by vaddr) make the kernel
   a batching client**: N tasks faulting on N pages of one block each generate
   one RP request per distinct key, and one response wakes all of them.
8. **There is no IPC timeout/cancellation primitive at all**; the timeout
   service is built *on top* of messaging by self-sending a message. This is a
   deliberate minimalism: cancel/timeout would require removing a task from
   `waiting` and invalidating its pending token — extra states Hostboot chose
   not to carry.
9. **Interrupt-context discipline**: `kernel_execute_hyp_doorbell` asserts the
   task did not change and defers IPC delivery into the master CPU's userspace
   interrupt queue rather than delivering inline — wakeups from interrupt
   context are funneled through a queue so they never reschedule directly.
10. **Message memory ownership flows sender → queue → receiver (async) or
    sender → queue/server → back to sender (sync)**; the freeing side is always
    the party that last consumed the message. This convention is enforced
    socially (docs/comments), not by type.

# 4. JIXIA DESIGN IMPLICATION

1. **Keep the one-lock-per-endpoint discipline** (queue lists + wake decision
   in one critical section) — it is the cheapest complete lost-wakeup proof.
2. **Reject the raw-pointer ABI**: Jixia U-mode services run in separate
   Sv39 address spaces, so `msg_q_t`-as-pointer, kernel-dereferenced `msg_t`,
   and `responses` keyed by user pointers are all unusable. Jixia needs typed,
   kernel-validated handles (index+generation) and register-resident payloads.
3. **Keep the three-part queue shape** (pending messages, outstanding
   transactions, waiting receivers) but make the transaction record
   kernel-internal and give it an explicit identity (reply token) that is not a
   pointer.
4. **Separate mechanism from policy**: Hostboot's naming (VFS server over IPC)
   is the right layering for Jixia too — the kernel provides only well-known
   endpoint(s); name resolution is a future userspace service (Root Component
   Registry milestone), *not* kernel string tables.
5. **Do not adopt direct handoff in M00-08.03.** Jixia's scheduler already has
   a publish-inside-lock fail-closed `add_task` contract; direct
   `set_current_task` handoff would add a second wake protocol to audit.
   Requeue + normal scheduling is enough at single-hart acceptance scale.
6. **Reply tokens must be single-use and validated** — the `-EBADF` duplicate/
   wrong-token rejection is correct behavior to keep; the token itself must be
   unforgeable from U-mode (64-bit kernel-generated id, not a user-chosen
   value).
7. **Destruction must be explicit and safe** (refuse or drain), because Jixia
   cannot rely on Hostboot's orderly-telemetry-only destruction usage.
8. **Kernel-as-client (MessageHandler) is the architectural bridge to the VMM
   Resource Provider**, but Jixia M00-08.03 does not need it yet; the M00-07
   FlashProvider materializes inline. Keep the *pattern* (block + pending-key
   dedup + handleResponse resume) as the future design shape.
9. **No timeout in the base mechanism for M00-08.03** matches both Hostboot
   minimalism and Jixia's milestone scoping; a later `recv`-with-deadline can
   reuse the existing per-hart delay queue rather than a helper task.
10. **Wakeup visibility contract for future SMP**: Hostboot's `msgsync()` note
    and no-op broadcast show the two halves (queue publication vs remote hart
    notification) must both be specified. Jixia M00-08.03 publishes under the
    endpoint lock and accepts polling visibility on other harts (no IPI), but
    the design must not *depend* on immediate remote visibility for safety.
11. **Interrupt-context rule for Jixia**: kernel code that delivers IPC from a
    trap/timer context must enqueue-only and let the scheduler boundary do the
    wake; never reschedule from inside the trap path (Jixia's M00-08.02 timer
    path already follows this).
12. **Payload discipline**: `data[2] + extra_data` shows even Hostboot's fast
    path is 16 register-sized words; Jixia's register-only ABI (6×64-bit) is a
    stricter but sufficient subset for service-call IPC, with buffer transfer
    deferred to the user-copy milestone.

---

## Appendix A — Second-pass addendum (2026-08-19, after the M00-08.02 closure directive; M00-08.02 has since landed on `main` as `de4df0e` with CI run `32219284629`)

### A.1 Coverage map against the M00-08.03 task list

| Required item | Covered in |
|---|---|
| MessageQueue representation and ownership | 2.2, 2.3 |
| `msg_send` | 2.4 |
| `msg_wait` | 2.5 |
| `msg_sendrecv` | 2.6 |
| `msg_sendrecv_noblk` | 2.6 (pseudo-sync path), 2.7, 2.8 |
| `msg_respond` | 2.7 |
| synchronous vs asynchronous behavior | 2.1 (flag polarity), 2.8 |
| waiting sender / receiver semantics | 2.5 (receiver), A.2 (sender) |
| pending reply / response ownership | 2.6, 2.7, 2.11 (pending tables per operation) |
| task BLOCK_* states | 2.10 |
| scheduler interaction / wakeup paths | 2.9 |
| MessageHandler / BlockMsgHdlr | 2.11, 2.12 |
| kernel-to-userspace message delivery | 2.11 |
| Resource Provider / VMM message flow | 2.12 |
| doorbell/IPI relevance | 2.13 |
| futex relationship | 2.14 |
| tests as behavioral contract | 2.15 |

### A.2 FACT — waiting *sender* semantics (synthesis of 2.6/2.7/2.11)

Hostboot never blocks a sender on queue capacity. The only blocked-sender
kind is a normal-synchronous client (`msg_sendrecv`), and it is not parked in
the `waiting` queue (that FIFO is exclusively for blocked *receivers*).
Instead the blocked client is referenced by its `MessagePending` record:

- before the server receives: the record sits in `messages` with
  `mp->task = client`;
- after the server's `msg_wait` moves the record into `responses`:
  `mp->task = client` identifies whom `msg_respond` must resume;
- for pseudo-sync: `mp->task = mq2` (the reply relay queue) instead;
- for kernel-consumed types: `mp->task = MessageHandler*`.

So: **waiting receivers are tracked in a dedicated FIFO; waiting senders are
tracked inside the transaction record itself.** Jixia's Transact record keeps
exactly this split (receivers in `waiting`, blocked callers in `Transact.client`).

### A.3 FACT — kernel-initiated termination of a blocked task exists

`MessageHandler::recvMessage` collects deferred tasks whose response rc is
unhandled and calls `TaskManager::endTask(task, ..., TASK_STATUS_CRASHED)` —
i.e. Hostboot *can* end a BLOCK_USRSPACE task from the kernel side (it ends
the task; it does not cancel an in-progress `msg_wait` — see 2.15 for the
no-cancel constraint on the user-side wait). Exit/termination therefore
crosses IPC state in exactly two places: the blocked task's queue/transaction
membership, and any reply obligations it owns.

### A.4 INFERENCE (second pass)

11. Because the waiting-sender identity rides inside `MessagePending`, a
    queue that is destroyed while sync messages are outstanding loses both
    the messages and the identities of the blocked clients — reinforcing that
    Hostboot destruction is only safe on drained queues.
12. `TaskManager::endTask` from `recvMessage` runs with the subsystem lock
    released specifically because ending a task re-enters scheduler/queue
    code — evidence that task teardown and IPC locks must be ordered, not
    nested arbitrarily (Jixia lock order §3 of the concurrency model).

### A.5 JIXIA DESIGN IMPLICATION (second pass)

13. Jixia task-exit cleanup must handle **both** blocked-sender
    (Transact.client) and blocked-receiver (waiting FIFO) membership, and must
    void reply rights the exiting task owns — the two membership kinds have
    different linearization requirements (see concurrency model §2.14–2.16).
