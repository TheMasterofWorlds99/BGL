# BGL Notes

Hard-won lessons from developing BGL. Each entry is a gotcha that cost real
debugging time — read these before fighting the same dragons.

## Slang → SPIR-V

- **Entry points are renamed to `"main"`.** Slang finds your entry point by
  its Slang name (`CSMain`, `VSMain`), but the emitted SPIR-V names it `"main"`.
  Pipelines must use `.pName = "main"` — using `"CSMain"` gives
  `VK_ERROR_INVALID_SHADER_NV` and a cryptic "Failed to create pipeline!"
  (`include/renderer.hpp`).
- **`mul(M, v)` treats the vector as a ROW** (HLSL heritage). It emits
  `OpVectorTimesMatrix` (v × M), which with glm's column-major data is the
  *transposed* transform. A transposed perspective matrix = garbage clip
  coords = grey screen, code looks perfect. Use **`mul(v, M)`** to get
  `OpMatrixTimesVector` (M × v), the math you actually want. This is the
  single most expensive lesson in this file.

  Why it transposes: SPIR-V matrices are column-major, so `M[j][i]` reads
  the element at column j, row i. `OpVectorTimesMatrix(v, M)` computes
  `result[i] = Σⱼ v[j]·M[j][i]` — the dot of v with row i, i.e. `Mᵀ·v`.
  `OpMatrixTimesVector(M, v)` computes `result[i] = Σⱼ M[i][j]·v[j]` — the
  dot of v with column i, i.e. `M·v` (the standard). For rotations, Mᵀ ≈
  M⁻¹ so things look subtly mirrored; for a perspective projection, Mᵀ is
  not a valid projection and every vertex clips → nothing renders.

  How to verify without running: `slangc -entry VSMain -o out.spv ...` then
  `spirv-dis out.spv | grep OpMatrixTimesVector` — you want `OpMatrixTimesVector`.

  Also affects normals: if you ever transform a normal with a matrix in
  Slang, the same row-vector rule applies — transpose the matrix or
  pre-compute a proper normal matrix on the CPU.
- `column_major` on the matrix declaration does **not** change this — verify
  with `spirv-dis` if unsure.

## GLM + Vulkan

- glm matrices are **column-major** — matches Vulkan/SPIR-V and Slang's
  std140 push constant layout, so pushing `glm::mat4` directly works.
- Use **`glm::perspectiveRH_ZO`** for Vulkan's [0,1] depth range. Avoids the
  `GLM_FORCE_DEPTH_ZERO_TO_ONE` macro, which must be defined before any glm
  include (headers already include glm).
- **Push constants cap at 128 bytes.** `mat4` viewProj (64) fits; full MVP
  (192) does not. Push `view * proj`, skip the model matrix (identity at
  origin), or split.
- `glm::lookAt` and `glm::perspective` need `<glm/gtc/matrix_transform.hpp>`
  — no BGL header pulls it in.

## Input system

- **`tickTimer()` must run BEFORE `glfwPollEvents()`.** Events are stamped
  with the current tick; if the tick advances after polling, every
  "just pressed" query compares against a stale frame and misses. If a demo
  forgets to tick, `tickCount` stays 0 and `0 == 0` makes every key report
  "pressed" forever — it compiles, runs, and lies to you.
- **Frame-stamp sentinels:** `keyDownFrame`/`keyUpFrame` default to 0 ("no
  event") and `tickTimer` makes the first real frame 1, so:
  - "just pressed" = `keyDownFrame == tickCount`
  - "held" = `keyDownFrame > keyUpFrame`
- GLFW key callbacks can deliver `GLFW_KEY_UNKNOWN` (-1) — guard before
  indexing `inputStates`.
- Scroll has **no poll** — callback only. Cursor has **no event** — poll only.

## ImGui (dear imgui v1.91.x)

- Define **`IMGUI_IMPL_VULKAN_USE_VOLK`** (not `NO_PROTOTYPES` + a manual
  `LoadFunctions` lambda) — the backend then uses volk directly, and BGL's
  `volkInitialize`/`volkLoadInstance` already ran in `createVulkanInstance`.
  (Make sure the imgui target also links `volk`, or it silently picks up the
  *system* volk.h.)
- **`DescriptorPoolSize > 0`** — the backend creates its own descriptor pool;
  you never hand-write one. Destroyed in `ImGui_ImplVulkan_Shutdown()`.
- **Font texture uploads automatically** on first `NewFrame()`; the backend
  creates its own command pool for it.
- **`UseDynamicRendering = true`** + `PipelineRenderingCreateInfo` whose color
  format matches the swapchain exactly (and depth format matches the render
  pass) — a mismatch renders nothing.
- **`ImGui_ImplGlfw_InitForVulkan(window, false)`** — don't let imgui install
  callbacks or it overwrites BGL's input system. Forward events instead.
- `RenderDrawData` must be called **inside** the render pass, before
  `vkCmdEndRendering`.

## Meshes / OBJ

- **Flat-shaded OBJ exports inflate vertices ~3×.** If every face has its own
  normal, deduping on (pos, normal, uv) never merges. The loader dedups by
  **position only** and averages normals — keeps decimated meshes at their
  real size (2,844 verts instead of 16,636).
- Decimation matters ×instance count: 208k tris × 100k instances ≈ 20B
  tris/frame. Aim ≤ ~500 tris for heavy instancing.
- **A flipped viewport reverses winding.** The 3D demo flips y; meshes that
  render there may be back-face-culled in a normal-viewport demo. `CullBackFace`
  is read at pipeline creation time only.

## Camera

- Orbit: `position = target - cameraForward(camera) * radius`; drag updates
  yaw/pitch; **clamp pitch to ±89°** — `cameraRight` degenerates at the poles
  (cross product collapses to NaN).
- Camera holds view data only; fov/aspect/near/far live in the user's
  projection. That's by design.

## Process

- **The engine is data structs + free functions.** Structs are dumb
  (`Window`, `GlobalTimer`, `Camera`, `Input`); operations are free functions
  (`tickTimer`, `beginFrame`, `cameraForward`). Keep it that way.
- Headers declare, `.cpp` files define (post-split). One home per symbol —
  no `inline` in `.cpp` files, no duplicate definitions.
- **Deleted files may resurrect** from an open editor tab writing stale
  buffers (lost `queues.hpp`/`main.cpp` several times). Before deleting a
  resurrected file, check `git diff`: if it matches HEAD it's the real file
  and must stay (a declarations header like `queues.hpp` is needed by the
  build); if it shows modified with old inline bodies, it's the stale tab.
  Close tabs, check `git status` before committing.
- Debug locally with validation layers: release builds compile out
  `DebugEnabled` (NDEBUG) — build with `-DCMAKE_BUILD_TYPE=Debug` to get
  validation output.
