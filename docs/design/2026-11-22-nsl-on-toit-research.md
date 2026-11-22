# nsl on Toit — two research notes (2026-11-22)

**Status:** research only, no decision. Follow-up in oevm: davidg238/rete#530. **Correction:** oevm re-enters the
interpreter from C in **14** places, not the 7 cited below (rete#530 lists them).

Not a decision; nothing changed in rete, ztoit or Toit. Inferences marked [inf]. Spot-checked by the main session:
entry_points.h:21-30, find_method (interpreter_run.cc:89-97), 105 opcodes, Stack initial 64 words, 1.5x grow-by-copy
(interpreter.cc:245-286), OEVM's 7 C→interpreter re-entries — all as cited.

**Headlines**
- **B (nsl → Toit bytecode):** feasible, but the bytecode isn't the hard part — linking Toit's core lib (VM calls 9
  entry points by name) and rebuilding ~190 oevm primitives is, on every route. Own emitter (b) 12–20 wk buys nothing
  over feeding Toit's IR (a) 6–9 wk; transpile (c) 3–5 wk first. Patching/mirrors/debugger lost without VM changes.
- **Fibers:** Toit's tasks are cheap because a stack is a small movable heap object (64 words, grows 1.5× by copy,
  GC shrinks it) and nothing points into it — blocks are stack offsets, C never re-enters the interpreter. OEVM built
  this model too, then deleted it at Milestone A for the reactor; the residual pain was NLR-capable escaping closures
  and C re-entry. Reactor judged right for its goals.

---

# B — compiling nsl straight to Toit VM bytecode: feasibility

Sources read: Toit `~/workspaceToit/toit-v2.0.0/src` (paths below relative to it), nsl `~/workspace/rete/nodus/nsl`
(prefixed `nsl/`). **[inf]** marks a claim inferred, not read in code.

## 0. Answer in one paragraph

Technically feasible, but the hard part is not bytecode. A Toit program image is not self-contained bytecode: the
VM calls back into **Toit core library methods** by fixed name (`__entry__main`, `lookup-failure_`, …), expects core
classes with fixed field layouts (`Task`, `Lambda`, `Exception_`, String/List/Map), and its primitives assume them.
So any route must link nsl code **with Toit's core library**. That makes (a) "inject nsl as IR into the Toit
compiler and let it link core" the only bytecode route that is not a VM re-port, and (c) transpile-to-source the
cheapest. (b) "own emitter + snapshot writer" has to re-create core's ABI by hand: the largest and most fragile.
Under all three, the work that costs the most is the same: porting `kernel.nsl`'s native layer (≈190 oevm prims,
`nodus/oevm/src/prim_names.def`) onto Toit primitives and libraries. The live patching and debugger cannot be done
without VM changes.

## 1. What a Toit program image contains

### 1.1 Bytecodes — 105 opcodes (`bytecodes.h:88-204`, `grep -c '^  BYTECODE('` = 105)

Stack machine, variable-length (1–6 bytes), formats `bytecodes.h:47-80`.

| Family | Opcodes (cite) | Count |
|---|---|---|
| Locals / outer / fields | LOAD_LOCAL[_WIDE,_0.._5], POP_LOAD_LOCAL, STORE_LOCAL[_POP], LOAD_OUTER, STORE_OUTER, LOAD/STORE_FIELD[_WIDE,_LOCAL,_POP] (:89-109) | 20 |
| Literals / constants | LOAD_LITERAL[_WIDE], LOAD_NULL, LOAD_SMI_0/1, LOAD_SMIS_0, LOAD_SMI_U8/U16/U32, LOAD_METHOD (:111-120) | 10 |
| Globals | LOAD_GLOBAL_VAR[_WIDE,_LAZY,_LAZY_WIDE,_DYNAMIC], STORE_GLOBAL_VAR[_WIDE,_DYNAMIC] (:122-129) | 8 |
| Blocks / stack | LOAD_BLOCK, LOAD_OUTER_BLOCK, POP, POP_1 (:130-134) | 4 |
| Alloc / type checks | ALLOCATE[_WIDE], IS_CLASS/IS_INTERFACE/AS_CLASS/AS_INTERFACE[_WIDE], AS_LOCAL (:136-147) | 11 |
| Static / block / lambda calls | INVOKE_STATIC[_TAIL], INVOKE_BLOCK, INVOKE_LAMBDA_TAIL, INVOKE_INITIALIZER_TAIL (:149-153) | 5 |
| Virtual calls | INVOKE_VIRTUAL[_WIDE,_GET,_SET] (:155-158) | 4 |
| Fast-path virtual ops | INVOKE_EQ/LT/GT/LTE/GTE, BIT_OR/XOR/AND/SHL/SHR/USHR, ADD/SUB/MUL/DIV/MOD, AT, AT_PUT, SIZE (:160-178) | 19 |
| Branches | BRANCH[_IF_TRUE,_IF_FALSE,_IF_NOT_NULL], BRANCH_BACK[…] (:180-187) | 8 |
| Control / unwind | PRIMITIVE, THROW, RETURN, RETURN_NULL, NON_LOCAL_RETURN[_WIDE], NON_LOCAL_BRANCH, IDENTICAL, LINK, UNLINK, UNWIND, HALT (:188-199) | 12 |
| Intrinsics | INTRINSIC_SMI_REPEAT, _ARRAY_DO, _HASH_FIND, _HASH_DO (:201-204) | 4 |

No breakpoint, no "send by name", no reflective opcode.

### 1.2 Method header — 4 bytes, then bytecode (`objects.h:800-893`)

| Byte | Field |
|---|---|
| 0 | arity (7 bits, `ARITY_MASK`) + `STATICALLY_RESOLVED_INSTANCE_BIT` (`objects.h:879,885-886`) |
| 1 | kind (2 bits: METHOD, FIELD_ACCESSOR, LAMBDA, BLOCK, `:893`) + max stack height/4 (6 bits) |
| 2-3 | value: **selector offset** for methods (`NO_SELECTOR`=0xFFFF), captured count for lambdas (`:817-824,889`) |

A method's identity everywhere is its byte offset in the single `bytecodes` array (`LOAD_METHOD` pushes a u32 offset,
`interpreter_run.cc:461`; `Method(program->bytecodes, id)`).

### 1.3 Program tables (`program.h:310-321`, serialized by `Program::write`, `program.cc:28-57`)

| Table | Content |
|---|---|
| `class_bits` (u16/class) | instance size + TypeTag per class id (`program.h:315,327`) |
| `global_variables`, `literals` | object tables; literals are null/bool/smi/double/large int/string/byte array (`compiler/program_builder.h:48-62`); equal strings are one object (`program_builder.cc:97-101`) |
| roots | null/true/false, empty array, OOM error, SDK version+info (`program.h:29-36`) |
| builtin class ids | 23 ids the VM needs: string, array, byte_array…, smi, task, exception… (`program.h:68-91`) |
| invoke_bytecode_offsets | dispatch offsets for INVOKE_EQ..INVOKE_SIZE fallback (`compiler/backend.cc:263-275`) |
| entry points | 9 Toit methods the VM calls by role: `__entry__main/spawn/task`, `lookup-failure_`, `as-check-failure_`, `primitive-lookup-failure_`, `too-few-code-arguments-failure_`, `program-failure_`, `run-global-initializer__` (`entry_points.h:21-30`) |
| `class_check_ids`, `interface_check_offsets` | id ranges for `is`/`as` checks (`program.h:313-314`) |
| `dispatch_table` (i32) | global selector-row-displacement table (below) |
| `bytecodes` (u8) | all methods, headers inline |

### 1.4 Dispatch: `find_method` (`interpreter_run.cc:89-97`)

`index = class_id(receiver) + selector_offset; m = dispatch_table[index]; valid iff m != -1 && m.selector_offset == offset`.
One global, row-displaced table (the compressed vtable layout from the literature). Class ids are assigned depth-first
so each class's subclasses form one id range (`dispatch_table.cc:300-326`). The selector is
`Selector<PlainShape>` = name + shape (`dispatch_table.h:34`). Shape = total arity **including receiver**, block-arg
count, sorted named-arg names, setter flag (`compiler/shape.h:66-110,249-280`). `INVOKE_VIRTUAL` carries the stack
offset of the receiver + a u16 selector offset (`interpreter_run.cc:703-713`); a miss pushes receiver + offset and calls
`lookup-failure_` (core `lib/core/exceptions.toit:159-162`, throws `LOOKUP_FAILED`). **Selector names are not in the
image**: only the host-side source map has offset→name (`backend.cc:187-189`).

### 1.5 Blocks vs lambdas

| | Block | Lambda |
|---|---|---|
| Value | a Smi-tagged pointer **into the stack** (`LOAD_BLOCK`, `interpreter_run.cc:518-521`) | heap `Lambda` instance: field 0 method id, field 1 captured value or Array (`interpreter_run.cc:1048-1056`; `lib/core/objects.toit:118-122`) |
| Escape | cannot outlive the frame (compiler-enforced) **[inf: enforcement in resolver/type check, not read]** | free |
| Captures | read/write the defining frame via LOAD_OUTER/STORE_OUTER | copied at creation; mutated captured locals are boxed (`compiler/lambda.cc:190`, `BoxVisitor`) |
| Call | `INVOKE_BLOCK n` (`:659-675`) | `Lambda.call`…`call a b c d` → `__invoke-lambda__` → `INVOKE_LAMBDA_TAIL` (`objects.toit:127-152`, `byte_gen.cc:738-746`); **max 4 args** |
| NLR | `NON_LOCAL_RETURN` / `NON_LOCAL_BRANCH` | none |

Creation of a lambda is a call to core `lambda__ method arguments arg-count` (`objects.toit:172-182`).

### 1.6 Exceptions

`THROW` unwinds to the nearest `LINK` frame (`interpreter_run.cc:1167-1174,1258-1300`); LINK/UNLINK/UNWIND implement
`try/finally` only. `catch` is library code in core over try/finally **[inf: not read in full]**. Any object can be
thrown; core wraps it in `Exception_ value trace` (`exceptions.toit:150-153`).

### 1.7 Snapshot → image → flash

| Step | Where | What |
|---|---|---|
| Program → snapshot | `snapshot.cc:947-980`, `program.cc:28-57` | two-pass (collect back-refs, emit) object-graph serialization with a header; source map not included |
| Snapshot → bundle | `snapshot_bundle.cc:30-108` | `ar` archive: magic `toit`, `snapshot`, `source-map`, `sdk-version`, `uuid` = SHA-256(sdk-version, snapshot) shaped as UUID5 |
| Bundle → image | `tools/snapshot_to_image.toit:43-198` (Toit, re-parses the snapshot) | builds the in-memory `ProgramImage`, emits chunks of 1 relocation-mask word + 32 words (`snapshot.cc:1318-1347`); header carries `system-uuid` (= UUID5 of the SDK version, `:195-198`), snapshot uuid, image `id` |
| Flash install | `ImageOutputStream::write` (`snapshot.cc:1361-1376`) | adds the flash base address to every word whose mask bit is set; `set_program_id` (`:1378`) |

The image is tied to the exact VM/SDK version via the system-uuid (in ztoit this is the `tools/sdk-id.sh` match, #24).

## 2. Where an external front end could plug in

Pipeline (`compiler/compiler.cc`): parse → `resolve()` (`:2369`, AST → `ir::Program`) → `sort_classes` →
`check_types_and_deprecations` + definite-assignment (`:2377-2379`) → `construct_program` (`:2148-2190`):
drop abstract, **add_lambda_boxes, add_monitor_locks, stub methods + switch to PlainShapes, interface stubs,
apply_mixins** → `optimize` (TypeOracle) → `tree_shake` → assign field/global ids → `Backend::emit` (`backend.cc:176-280`:
DispatchTable::build, ProgramBuilder, ByteGen per method) → SnapshotGenerator (`:2486`). At -O2 the whole thing is
**run twice, re-resolving from source** with propagated types (`:2433-2456`).

| Boundary | Clean? | Notes |
|---|---|---|
| `Backend::emit(ir::Program*)` (`backend.h:24`) | API is clean, input is not | expects post-lowering IR: PlainShapes, stubs added, lambdas boxed, field indexes and global ids assigned, classes sorted DFS with `is_instantiated` set |
| `construct_program(ir::Program*, …)` (`compiler.cc:2148`) | **the practical seam** | takes resolved IR; does all lowering, optimization, shaking. Input invariants = what `Resolver` produces (ResolutionShape on methods, Typecheck nodes, references to core `Lambda`/`lambda__`/box class) |
| `Pipeline::post_resolve` hook (`compiler.cc:325,1226`) | yes, a virtual | an override could append nsl `ir::Class`/`ir::Method` to the resolved core program; it re-runs on the -O2 second resolve |
| ProgramBuilder (`program_builder.h:48-112`) | C++ API, `friend` of `Method` | a lower seam: you'd re-do ByteGen and DispatchTable yourself |

IR vocabulary: 62 node kinds (`ir.h:41-102`) — Class/Field/Method*/Code/Block/Sequence/TryFinally/If/While/LoopBranch,
FieldLoad/Store, CallVirtual/CallStatic/CallConstructor/CallBlock/CallBuiltin/Lambda, Typecheck, Return,
Reference*/Assignment*, Literal*, PrimitiveInvocation. `ir::Program` = classes, methods, globals, tree roots, entry
points, literal types, lookup_failure, as_check_failure, lambda_box (`ir.h:224-277`). There is no serialized IR: it is
an in-memory C++ zone graph — an external front end must be C++ linked into `toitc` (or feed it JSON via a new reader).

Assumptions an nsl front end must satisfy:

| Assumption | nsl status |
|---|---|
| Whole program, closed world, tree-shaken from entry points | matches: nsl is closed-world (row 26: an unwritten selector is never in the image) |
| Static class hierarchy, single inheritance (+ Toit mixins) | matches: static binding (row 1), one delegate (row 2) |
| Every send's selector = name + arity known at compile time | matches: no `perform:` (row 26) |
| Shape distinguishes **block** args from value args; a block param must receive a literal block | conflict: nsl closures are untyped values → map all non-inlined nsl closures to lambdas |
| Named args sorted in the shape | unused by nsl; keyword selector `at:put:` = name `"at:put:"`, arity 3, no names **[inf: Symbol accepts any string]** |
| Core library present (entry points, Lambda, Task, Exception_) | must link Toit core |

## 3. nsl features on Toit VM

| Feature | Mark | How |
|---|---|---|
| Unary/binary/keyword sends | **direct** | CallVirtual with selector `"at:put:"`/arity; `+ - < = at: at:put: size` hit INVOKE_ADD… fast paths if names match Toit's (`backend.cc:263-275`) |
| Explicit `self`/`super` sends, private/protected (row 25) | **direct / lowering** | super → CallStatic; private → static; protected (self-only) is a compile check |
| Implicit-receiver send (`foo` walking outer chain then inheritance, `nodus/oevm/docs/op_send_implicit.md`) | **needs lowering** | closed world: per site, find the first lexical level whose possible runtime classes define the selector locally → emit `outer` field loads + CallVirtual; where only some subclasses define it, an IS_CLASS guard picks the level. No runtime walk needed. `nsl/tools/ns_compile_oevm.py:1786-1807` today emits a runtime walk |
| Nested classes, `outer` slot 0 | **direct** | ordinary field 0, filled at construction (`docs/compiler.md` "outer (slot 0)") |
| Modules as objects (`Kernel usingPlatform:`) | **direct** | a module is a class instance; no Toit globals needed |
| Class references as values (`platform Array`, `Array new: 3`) | **needs lowering** | Toit classes are not runtime values: one singleton "class object" per nsl class, held in a lazy global or the enclosing object |
| Class side / metaclass (`Meta_<vt>`) | **needs lowering** | the class-object singleton above is an instance of a generated `Meta_X` class whose superclass is `Meta_<super>`; this fixes nsl's "inherited `self new` builds the defining class" limitation for free |
| Non-inlined blocks/closures | **needs lowering** | Toit lambdas (heap, boxes for mutated captures, `lambda.cc`); `value…` → add `value/value:…` methods to `Lambda` (IR route) or a wrapper class; >4 args needs more `call` overloads |
| Inlined control flow (`ifTrue:`, `whileTrue:`, `to:do:`, row 19) | **direct** | ir::If/While/LoopBranch; `^` inside them → ir::Return |
| `^` from escaping closure | n/a | already a compile error in nsl (`ns_compile_oevm.py:1386`) |
| Literals: ints, floats, strings | **direct** (ints: semantics differ) | Toit int is wrapping int64 **[inf]**; nsl is ±(2^64−1) with overflow trap (row 29) |
| Symbols | **needs lowering** | no Symbol class or literal kind; equal strings share one object (`program_builder.cc:97-101`), so `#foo` as a string would be `==` to `'foo'` — wrong by row 10. Use a `Symbol` class wrapping a string, one per lazy global |
| `{…}` dynarray, Record/List/Map/Bytes values (row 28) | **needs lowering** | nsl kernel classes compiled as ordinary classes, or mapped to Toit Array_/List/Map/ByteArray where layouts allow |
| Primitives (≈190, `prim_names.def`; "universal" prims on any receiver) | **needs lowering — the big item** | each becomes a method on the right class calling a Toit primitive (`#primitive.<module>.<name>`, PRIMITIVE bytecode, module+index, `interpreter_run.cc:1091-1095`) or Toit library code. oevm-only ones (space/channels, journal, pulse, relay, mirror) need Toit code or new VM primitive modules |
| Exceptions: `error:` Record, `on:do:` by predicate, `ensure:`, `raise` (row 24) | **needs lowering** | throw the Record; `on:do:` = try/catch + predicate test + rethrow; `ensure:` = TryFinally. Toit has the needed unwind (LINK/UNWIND); terminate-only semantics match |
| Reactor, no fibers (row 22), supervisor crash boundary | **needs lowering** | one Toit task running the nsl reactor loop; uncaught error caught at the loop and handed to the supervisor. Toit tasks exist but nsl code never sees them |
| `doesNotUnderstand:` | **needs lowering** | replace the `lookup-failure_` entry point; it receives receiver + selector **offset**; emit an offset→name table as a literal so it can build a Message. Only possible via the IR route (or own writer) |
| Computed send `Message sendTo:` (row 26) | **needs lowering** | no dynamic-selector opcode: generate a per-selector dispatch method (switch on name → static CallVirtual) over the closed selector set |
| Mirrors / reflection (rows 15, 26) | **impossible without VM work** | no class objects, no vtable/frame introspection in the image; Toit's stack trace is encoded bytes decoded on the host |
| Live method patch (`nsl/tools/patch_method.py`, row 31) | **impossible without VM work** | methods, dispatch table, literals all in the read-only flash image; method id = byte offset; `find_method` has no override hook (`interpreter_run.cc:89-97`). Would need a RAM override table checked in `find_method` (hot path) |
| Debugger (breakpoints by blob offset, row 16) | **impossible without VM work** | no breakpoint bytecode in the 105; no single-step hook found **[inf: searched opcode list only]** |
| Image map (`.oimg.map.json`) | **direct-ish** | Toit's source map (`snapshot_bundle.cc:37`) already maps method ids→source lines and offsets→selector names |

## 4. Options ranked by effort (cheapest first)

Common to all three: the kernel/platform port — `nsl/lib/kernel.nsl` (5 460 lines) + `supervisor.nsl` (429) are
nsl and come along, but the ≈190 oevm prims underneath must be re-expressed on Toit (≈1.5–3 k lines Toit/C,
**4–6 weeks**, risk: semantic drift in channels/space/timers). Not counted below.

| | (c) transpile to Toit source | (a) nsl → Toit IR, reuse Toit back end | (b) own emitter + snapshot writer |
|---|---|---|---|
| What you write | Python: nsl JSON AST (existing `ns-parse.js`) → `.toit`; a Toit runtime shim (class objects, Symbol, Record, closures, `on:do:`) | C++ in a `toitc` fork: JSON-AST reader → `ir::Class/Method/...` appended in `post_resolve`; same Toit shim | Python: stack-height-tracking emitter (cf. `emitter.cc` 589 + `byte_gen.cc` 1 102 lines), row-displacement dispatch builder (cf. `dispatch_table.cc` 534), class ids/bits, literal + snapshot writer (cf. `snapshot.cc` ≈600 relevant), **plus** a hand-made core: entry points, Task, Lambda, Exception_, builtin classes |
| Size | 2–3 k Python + 1–2 k Toit | 3–5 k C++ + 1–2 k Toit | 5–8 k Python + re-implemented core (Toit core is 10 659 lines in `lib/core`) |
| Time | **3–5 weeks** | **6–9 weeks** | **12–20 weeks** |
| Keeps DNU, Symbol identity, closed-world implicit send | DNU no (Toit source can't hook `lookup-failure_`), rest yes | yes | yes |
| Toit optimizer/tree shaking | yes | yes | no (own) |
| Main risk | Toit's resolver rejects or warns on legal nsl (dynamic sends on `any`, name mangling `at:put:` → `at-put a b`, reserved words); source-level indirection for class objects costs RAM | undocumented IR invariants that only `Resolver` establishes (ResolutionShape, Typecheck, `is_instantiated`, sort order); re-run on the -O2 second resolve; a C++ fork of `toitc` to carry across SDK bumps | VM ABI coupling: Task/Lambda field order, entry-point set, builtin class ids, snapshot format and system-uuid all change per SDK version (ztoit #24); every Toit primitive assumes core's object layouts |

Recommendation implied by the evidence: (c) first, to measure what nsl-on-Toit costs in RAM/flash and speed; move to
(a) only if DNU, Symbol identity or the lambda overhead earn it. (b) buys nothing that (a) does not, at two to three
times the cost. No route keeps live patching or the debugger without a VM fork (RAM override in `find_method` +
a breakpoint opcode).

Estimates are judgement **[inf]**, scaled from the sizes of the code they replace; nothing was prototyped.


---

# Many cheap tasks: Toit's mechanism vs why OEVM chose the reactor

Toit paths are relative to `~/workspaceToit/toit-v2.0.0/`. OEVM paths are relative to `~/workspace/rete/`.
**[inf]** marks an inference or arithmetic, not a line read from the code.

## 0. The headline

OEVM's fiber trouble was not that the Toit model failed. OEVM **built** the Toit model: a flat
loop, a switch that swaps a heap `Stack`, base-relative links and growable stacks. All of it was
green on 2026-07-01 (`nodus/oevm/docs/archive/fiber_derecursify_core_design.md:175-187`).
Milestone A **deleted it three days later**, as a product decision (`review/WHY.md:164-203`).
The real pain came earlier, in R7: fibers bolted onto a **C-recursive** interpreter
(`fiber_implementation.md:17-50`). The cost that remained came from the *language*: blocks that
can both escape and do a non-local return (NLR) across fiber stacks. Toit's VM never pays that,
because the Toit language rules it out.

## 1. Toit's mechanism

### 1.1 What a task is

| Item | Fact | Cite |
|---|---|---|
| Task | A Toit `Task_` instance whose field 0 is its `Stack` | `src/objects.h:1644-1650` |
| Stack | An ordinary heap object (`STACK_TAG`) in the process heap. Grows downward. Header: length, top, try_top, pending_stack_check_method, guard zone | `src/objects.h:955-1035` |
| Initial size | **64 words** | `src/objects.h:990`, `src/heap.cc:257-271` |
| Max size | One heap page minus overhead: `(max_allocation_size - HEADER)/WORD` | `src/objects_inline.h:35-37`, `src/heap.h:57` |
| Page size | 4 KB on 32-bit, 32 KB on 64-bit | `src/top.h:179-190` |
| Frame | **2 words**: return `bcp` plus `frame_marker`. No frame pointer; locals are addressed from sp | `src/interpreter.h:49`, `src/interpreter.cc:90-101` |
| Per-method need | The compiler records `max_height` per method and `global_max_stack_height` | `src/interpreter_run.cc:156`, `src/program.h:307` |

**Per-task floor [inf]** (32-bit, release build). Stack header: 5 words plus a 4-word guard zone,
about 39 B, aligned (`objects.h:1026-1035`). The stack object is then 39 + 64·4 ≈ **296 B**. The
`Task_` instance has about 12 fields plus a header, roughly 52 B (counted from `lib/core/task.toit`;
the count is approximate). Add the task's lambda. The floor is about **350–400 B per idle task**.
On ESP32 the maximum stack is about 990 words, roughly 3.9 KB.

### 1.2 Growth: copy into a new object

| Step | Cite |
|---|---|
| Every call does one check, `sp - target.max_height() < watermark_` | `src/interpreter_run.cc:155-172` |
| New length = `max(1.5·len, len - headroom + needed)`, capped at max | `src/interpreter.cc:245-252` |
| Allocate a new `Stack`, with up to 3 GC retries. Failure raises a catchable stack-overflow or OOM exception, not an abort | `src/interpreter.cc:254-279` |
| `store_stack` → `copy_to` (one `memcpy` of the used part to the same offset from the *end*) → `set_stack` → `load_stack`. The old stack becomes garbage | `src/interpreter.cc:281-286`, `src/objects_runtime.cc:132-149` |
| Fix-ups after the copy: only the scalars `top` and `try_top` | `src/objects_runtime.cc:137-138` |

### 1.3 Why copying is safe: nothing on the stack points into the stack

- **Blocks are Smi offsets from the stack base**, not pointers: `to_block`/`from_block` =
  `base_ - p + BLOCK_SALT` (`src/interpreter.h:201-207`). `LOAD_BLOCK` pushes one
  (`src/interpreter_run.cc:518-520`). The unwinder uses them for throw, finally and NLR targets
  (`:1172`, `:1217-1248`). The base is the *end* of the object and `copy_to` keeps base-relative
  layout, so the offsets stay valid.
- **Return addresses point into the immutable program bytecodes.** GC skips them
  (`src/objects.cc:268-273,305-309`).
- **The interpreter keeps the absolute pointers (`limit_`, `base_`, `sp_`, `try_sp_`) only while a
  task is loaded** (`src/interpreter.h:159-163`). They are written back to `top`/`try_top` as
  offsets (`src/objects_runtime.cc:151-175`). Every GC goes through `store_stack` → GC →
  `load_stack` (`src/interpreter.cc:107-109`). Every primitive call spills `sp_`, calls, and then
  reloads `sp = sp_` (`src/interpreter_run.cc:1107-1109`).
- **No C re-entry into the interpreter.** Primitives that need to call a block don't recurse.
  `hash_find` returns `kCallBlockThenRestartBytecode`, the interpreter pushes the block call, and
  then re-executes the bytecode (`src/interpreter.h:96-101`, `src/interpreter_run.cc:1600,1694`).
  So no C frame ever holds a pointer into a Toit stack across a switch.

### 1.4 Switching, yielding, preemption

| What | How | Cite |
|---|---|---|
| Task switch | Primitive `task_transfer`: `store_stack()`, `set_task(to)`, `load_stack()`. The interpreter resumes on the new sp via the reload at `:1109`. **The cost is two scalar spills, a pointer swap and four pointer derivations.** No C stack and no register file | `src/primitive_core.cc:2114-2128` |
| Task creation | `task_new`: allocate Task and 64-word Stack, then hand-build a fake `task_transfer` frame on it | `src/primitive_core.cc:2092-2112`, `src/interpreter.cc:90-101` |
| Scheduling policy | Written **in Toit**: a ring of running tasks, `yield` = transfer to `next-running_`. Tasks are **cooperative** inside a process | `lib/core/task.toit:52-54,62,340-356` |
| Blocking | Monitors: `await: cond` evaluates a *block* (a stack slot) on the waiting task's own stack | `lib/core/monitor_impl_.toit:28-52` |
| Termination | `task-transfer-to_ next true` detaches the stack, so the stack becomes garbage at once | `lib/core/task.toit:297`, `src/primitive_core.cc:2123` |
| Preemption | Processes only, on a scheduler tick. `preempt()` sets `watermark_ = PREEMPTION_MARKER`, so the **stack-overflow compare doubles as the preemption poll** at no extra cost | `src/interpreter.cc:45-47`, `src/interpreter_run.cc:156-185`, `src/scheduler.h:208-213` |

### 1.5 GC and shrinking

- A `Stack` is scanned like any object. Its used slots are precise roots, with the bytecode
  range skipped (`src/objects.cc:262-307`). The interpreter writes without a barrier and adds
  the stack to the remembered set on store (`src/objects_runtime.cc:170-174`).
- **Shrink happens in place during mark-sweep.** The trigger: more than half is free (`top > len/2`)
  and at least 8 words can be cut. The stack is `memmove`d toward the header, keeping
  `global_max_stack_height + RESERVED_STACK_FOR_CALLS` of headroom. The tail becomes free words
  (`src/objects.cc:276-301`). Only mark-sweep asks for it (`third_party/dartino/mark_sweep.h:62`).
  The scavenger may also *move* a stack, which is safe for the same reason as growth **[inf]**.

### 1.6 Processes vs tasks

| | Task | Process |
|---|---|---|
| Heap | Shares the process `ObjectHeap` | Its own `ObjectHeap` (`src/process.h:122,320`) |
| Communication | Shared objects plus monitors | Messages **encoded and copied** (`src/primitive_core.cc:2130-2151`) |
| Scheduling | Cooperative, in Toit code | Preemptive: one interpreter per scheduler thread, `max_threads = num_cores` (`src/scheduler.cc:62`) |
| Cost | About 300–400 B **[inf]** | At least one heap page plus process structures **[inf]** |

## 2. Why OEVM abandoned fibers and chose the reactor

### 2.1 Timeline (from the record)

| Date | State | Cite |
|---|---|---|
| 06-15 | Phase 4 fibers from the first commit: `struct Fiber{oop *stack;...}`, **recursive** interpreter | `review/2026-07-04-pre-fiber-baseline.md:20-35` |
| ~06-29 | **R7**: POSIX ucontext with a **256 KB malloc'd C stack per fiber**. Zephyr gets a "hijack" that shares one C stack | `fiber_implementation.md:17-28,68-69` |
| 06-30 | Hijack bug: `ABABAB` came out as `ABBBAA`. One C `run_interpreter` activation cached `saved_stack_ptr`/`prev_ctx` for the *entering* fiber and restored the wrong one. "Fundamentally incompatible" with C recursion per send | `fiber_implementation.md:29-50` |
| 06-30 | Decision: **Option C, the Toit model** (flat loop, switch = swap heap Stack) | `fiber_implementation.md:52-66,76-153` |
| 06-30–07-01 | Flat loop done (`sum:50000` under a 512 KB ulimit). Stack-swap fibers. ucontext and the conservative C-stack scan deleted | `fiber_derecursify_core_design.md:142-213` |
| 07-01–07-02 | Deferred lift: heap `Context`, relativize `home_ctx`, growable stacks (2× growth, cap 262 144 words) | `fiber_derecursify_core_design.md:209-219`; `git show 185fc8a0` |
| 07-03–07-04 | **Milestone A deletes fibers, NLR and growable stacks.** Keeps the flat loop and the heap Context | `nodus/oevm/docs/MILESTONE_A.md:7-61`, `review/2026-07-04-context-decision.md` |
| 07-06 | Reactor: Space drain, FIFO, run-to-completion | commit `57e37606` |

### 2.2 Where it hurt, concretely

1. **The C recursion (R7).** It needed per-fiber C stacks: 256 KB each on POSIX, and impossible
   on a 2–8 KB Zephyr thread (`review/WHY.md:74-76`). The single-stack hijack corrupted
   scheduling. *The Toit model fixed this.*
2. **Closures that both escape and do NLR.** Making `^` safe needed `home_stack`/`home_offset`/
   `home_generation`, a cross-fiber check, a liveness generation tag
   (`fiber_l1_stack_context_design.md:128-146`) and "reify-on-escape". The reify step existed only
   because "fibers made cross-stack home links possible" (`review/2026-07-04-context-decision.md:47-51`).
3. **Growth with residual C pointers.** The growable-stack code needed `stack_ensure` "so
   callers can rebase any live C-local raw pointers". It also needed rebase preludes for `args`,
   a straddle guard, and grow-recovery reloads after a primitive's *nested* `run_interpreter`.
   That came to 164 lines deleted (`git show 185fc8a0`). C still re-enters the interpreter today:
   `nodus/oevm/src/dispatch.c:554,626,2091,2139` and `nodus/oevm/src/vm.c:178,221,241`.
4. **The "balloon".** In WHY.md's words, "two days of fibers + NLR + reify-on-escape + growable
   stacks produced a cascade of interacting complexity" (`review/WHY.md:47-55`). The measured
   delta for deleting fibers and NLR was **−3 921 B text and −676 B data+bss**
   (`MILESTONE_A.md:143-147`). Source dropped from 10 652 to 9 844 lines
   (`review/2026-07-04-context-decision.md:62-68`).
5. **What the reactor was chosen for** (`review/WHY.md:164-203`):
   - It fits Zephyr's grain.
   - It is correct across deep sleep: "a suspended fiber's stack cannot survive power-down".
   - Robustness is cheap: one stack in one known state gives a simple crash boundary, watchdog
     and GC rooting.
   - RAM: **one** 4096-word (16 KB) stack paid once. A fiber model "would multiply it by the live
     task count". Note that this assumes fixed-size per-fiber stacks, the pre-growth design.
   - Measured result: **45 % RAM** on the nRF52840 DK.

## 3. Root differences

| Design choice | Toit | OEVM (today) | Matters? |
|---|---|---|---|
| **Block vs closure split** | **Blocks** live only on the stack (Smi offsets) and can't be stored, so NLR is safe. **Lambdas** are heap objects and can't NLR | One closure kind that can escape. NLR was tried, then made a compile error inside escaping closures | **Most**: this removed cross-stack lexical links, so no reify or generation tags were needed |
| **C re-entry into the interpreter** | **Never.** Restartable bytecodes (`kCallBlockThenRestart…`) | Yes: `send()`, DNU, BoundClass init, `on:do:` predicate/handler, one per reaction (`vm_protected_fire`) | **Co-equal**: it is the precondition for moving or swapping stacks mid-call |
| Interpreter state across calls | `sp` local, re-derived after every GC, primitive and switch | `current_frame`, `regs`, `args` as C locals/globals with absolute addresses, which needed rebase preludes | Follows from the row above |
| Frame size | 2 words (bcp + marker), stack bytecode | 5-word header (`vtable, prev_frame, saved_pc, home_ctx, closure`) plus a register window (`nodus/oevm/src/oevm.h:174-183`) | Moderate: shallow stacks are smaller in Toit |
| Stack-overflow check | One compare per call against a compiler-known `max_height`; also the preemption poll | Hard cap, abort to the crash boundary | Small |
| Growth cost | `memcpy` into a new object; old one is garbage (generational GC, pages) | Was realloc-copy into a non-moving fixed arena; fragmentation is OEVM's live OOM cause (`nodus/oevm/docs/vm.md` §Memory) | **High on a non-moving arena [inf]**: many stacks of varying size fragment it |
| Shrink | In-place `memmove` at mark-sweep | None | Needed to make idle tasks cheap |
| Scheduler | In the language (Toit ring list) | The reactor (nsl kernel) | Neutral |
| Deep sleep | Tasks don't survive it either; Toit reboots into `main` **[inf]** | The reactor's explicit state rehydrates | OEVM's real advantage |
| Isolation | Per-process heaps; tasks share one | One heap, policy isolation | Orthogonal |

**What matters most.** First, the *language* rule that blocks are stack-only and non-escaping.
Second, the *VM* rule of zero C re-entry. Toit has both, so a stack holds no pointers into itself
and no C frame holds pointers into it. OEVM got the second mostly right on 07-01. Its NLR-capable
escaping closures and the remaining `run_interpreter` re-entries meant growth needed fix-up code.
With NLR now gone, the first obstacle **no longer exists in OEVM [inf]**.

## 4. What adopting Toit's tasks would take, against what reactors bought

### 4.1 Work (mostly already done once; see `git show 185fc8a0` and the 07-01 branches)

| Step | Size [inf] |
|---|---|
| Remove the remaining C re-entries: make `send()`-from-C, DNU, BoundClass init and `on:do:` push frames in the loop, with restartable ops for primitives that call blocks | Large: the crash boundary (`setjmp`, Milestone E) would become an in-loop unwinder |
| A per-task `Stack` starting at 64–128 words, with relative links (`frame_abi.h` already makes `prev_frame` and `home_ctx` relative) | Small: restore from history |
| Grow by copying into a new object, shrink at GC | Medium: needs care in the non-moving arena; the safe-point compaction at depth 1 (rete#477) would have to treat parked stacks as movable |
| `task_transfer` primitive, a scheduler and monitors in nsl | Small |
| Watchdog, supervisor and debugger: per-task rather than per-reaction | Medium |

### 4.2 Costs and gains

| | Reactor (current) | Toit-style tasks |
|---|---|---|
| RAM, N waiting activities | One 16 KB stack plus a state object per activity (tens of bytes) | About 300–500 B per task while parked shallow **[inf]**. The 16 KB could hold about 30–50 idle tasks |
| Sequential "do, wait 10 ms, read" | A state machine or numbered steps (`docs/spikes/2026-10-13-sequence-without-fibers/`) | Straight-line code |
| Deep sleep | Free: state is explicit | Lost: the code must checkpoint explicitly anyway |
| Crash containment | One reaction, one known stack state | Per-task unwind; partly-run tasks hold locks/monitors |
| GC / compaction | Safe point at depth 1 is trivial | Must walk every parked stack; growth adds fragmentation pressure |
| Zephyr fit | Matches the workqueue/callback grain | A second scheduler above Zephyr's |
| VM text | Baseline | About +4 KB (the Milestone A deletion delta) **[inf]** |

**Verdict [inf].** For OEVM's goals the reactor was the right call: a 256 KB nRF class, deep-sleep
nodes, let-it-crash, and one fixed, non-moving 128 KB arena. Most of the "fibers are expensive"
cost was the C recursion and NLR, and neither applies any more. The remaining cost of adding
tasks is the C re-entry removal plus a fragmentation-tolerant allocation of many small stacks.
That cost is real, but it is no longer large. Toit's ergonomics are worth it on an ESP32 with a
moving generational heap. On OEVM, compiling `wait:` into numbered states (spike shape A) gives
most of the ergonomics without putting suspension back.
