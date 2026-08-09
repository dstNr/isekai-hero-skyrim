# ImGui VR Helper — vendored client API

Copied verbatim from [`alandtse/imgui-vr-helper`](https://github.com/alandtse/imgui-vr-helper),
`api/`, on 2026-08-09. **Do not edit these files** — re-copy them from upstream instead, so
a diff against upstream stays meaningful.

Vendored rather than pulled in with CMake FetchContent for the same reason
`src/PrismaUI_API.h` is: the build has to work offline and a released archive must not
depend on a URL still resolving in two years.

## Licence

**LGPL-3.0-or-later** (`COPYING.LESSER`, with `COPYING` for the GPL text it refers to).
That is not this project's licence — Isekai Hero itself is MIT, see `/LICENSE`, and its
third-party section names these files.

MIT code may link an LGPL library. What the LGPL asks in return is that the LGPL part
stays replaceable and identifiable, which is why these files sit in their own directory,
unmodified, with their licence texts beside them.

## What is used

- `ImGuiVRHelperAPI.h` / `.cpp` — the SKSE handshake and the versioned interfaces. The
  handshake is deliberately **retryable, not latched**: an early call returns null if the
  helper's messaging listener is not up yet, which is a plugin load-order race, not a
  failure.
- `ImGuiVRHelperTypes.h` — `Frame`, `PanelHandle`, `WorldQuad`.
- `ImGuiVRHelperInput.h` — `InputCombo`, for VR controller bindings.
- `ImGuiVRHelperClientSDK.h` — the client-side convenience layer (`RenderToPanel`,
  `BlitDrawData`, `ResolveImmediateContext`). Used rather than hand-rolling the D3D blit,
  because none of it can be tested from this side.

## Why this matters for us

The helper calls **us** — `RegisterClient` takes an `OnFrameFn`. So in VR we never hook
Present, never read `RE::BSGraphics::Renderer`, and never touch
`renderWindows[0].swapChain`: the device is resolved from the panel's render target view.
That garbage read is the thing that crashes Skyrim VR today. See `docs/VR.md`.
