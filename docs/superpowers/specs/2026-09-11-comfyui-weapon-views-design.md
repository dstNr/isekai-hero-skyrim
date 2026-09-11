# Local concept views for weapons: ComfyUI to Meshy multi-image

Generate, on this machine, the four images Meshy's multi-image-to-3D wants for a weapon —
**main, left, back, right** — from a written design and an SDXL finetune the user chooses.
Both workflows are driven from Claude Code through comfy-cli and the official comfy-mcp, and
remain usable by hand in the ComfyUI interface.

This replaces the *input* of stage A in `docs/WEAPON_ASSET_GUIDE.md` — one concept image
from an external tool becomes four consistent views generated locally. Everything downstream
of Meshy is unchanged.

## Goals

- Two ComfyUI workflows, versioned in the repository: a **concept** workflow (SDXL) and a
  **views** workflow (Qwen-Image-Edit-2511 with two LoRAs).
- Output that meets Meshy's multi-view input contract (below) without manual editing.
- **No custom nodes, and nothing installed into ComfyUI's Python.** Both workflows use core
  nodes only.
- The same workflow files run from the CLI, from the MCP, and in the ComfyUI interface.

## Non-goals

- **Automating the Meshy upload.** The API needs a key and costs credits per task; manual
  upload stays until several weapons come in a row.
- **Choosing the SDXL finetune.** The user supplies it. A smoke test uses a checkpoint
  already on disk.
- **MV-Adapter.** Documented below as the fallback, not built.
- Upscaler models, background-removal nodes, the Qwen camera-angle custom node.
- Portrait resolution buckets: concept images are **1024×1024**, as the user specified.
- Armour and characters.

## Why Qwen-Image-Edit, not MV-Adapter

MV-Adapter is purpose-built for this — it generates all views jointly, with orthographic
cameras — and it cannot share an environment with a current ComfyUI:

- ComfyUI's core requires `transformers>=4.50.3`; ComfyUI-MVAdapter pins
  `transformers==4.46.3`, `diffusers==0.31.0` and `huggingface_hub==0.24.6`. The conflict is
  with ComfyUI itself, not with another node pack.
- Leaving the pins out fails as well. Issue #106 (May 2026) breaks at import
  (`cannot import name 'FLAX_WEIGHTS_NAME'`); issue #107 (May 2026) breaks in
  `LdmPipelineLoader` with an SDXL finetune (`'CLIPTextModel' object has no attribute
  'text_model'`) on Windows and Python 3.13 — exactly this use. Both are unanswered, and the
  repository has been inactive since mid-2025.

Running it would mean a second Python environment outside ComfyUI, on dependencies frozen at
2024. Qwen-Image-Edit-2511 with fal's multiple-angles LoRA runs on core nodes instead.

**The trade-off accepted:** Qwen generates each view separately, so consistency between views
is not guaranteed, and the edge-on view of a thin blade is the hardest case for it. The first
test decides. If the edge views fail, MV-Adapter in its own environment is an addition — the
concept workflow, the installation and the archive step stay as they are.

## Meshy's input contract

From Meshy's multi-view tutorial and API reference:

| Requirement | Value |
|---|---|
| Images | 1 to 4 of the same object, slots **Main, Left, Back, Right** |
| Resolution | at least **1040×1040** |
| Formats | PNG, JPG, JPEG, WebP, up to 20 MB each |
| Background | plain — white, grey or transparent |
| Lighting | even and diffuse |
| Framing | object centred and filling the frame, **the same distance in every image** |
| Named failure modes | several angles in one image, a background close to the object's colour, strong highlights on metal, inconsistent distances |

## Architecture

```
 Workflow 1 · concept                (repeat until one design is right)
   SDXL finetune ── prompt, seed ──► 1024×1024, batch of 4 ──► concept images
                                                                   │
                                           the user picks one ◄────┘
 Workflow 2 · views                                                ▼
   concept ─► Qwen-Image-Edit-2511 fp8 + Lightning LoRA + multiple-angles LoRA
     ├─ "<sks> front view eye-level shot medium shot"      ─► 1536² ─► main
     ├─ "<sks> left side view eye-level shot medium shot"  ─► 1536² ─► left
     ├─ "<sks> back view eye-level shot medium shot"       ─► 1536² ─► back
     └─ "<sks> right side view eye-level shot medium shot" ─► 1536² ─► right

   fetched by job id ─► E:\_isekai_assets\<weapon>\concept\  and  \views\ ─► Meshy, by hand
```

**Two workflows, with a human choice between them.** Concept generation is a creative loop
over many seeds; the views are expensive — a 20B model, four branches. In one workflow every
rejected design would pay for its views. It also means SDXL and Qwen never have to be loaded
at the same time, which matters with 32 GB of system RAM.

**`main` goes through Qwen as well.** Meshy requires the same distance and scale in every
image. The SDXL original comes from a different model with its own framing; running all four
branches with the same elevation and distance (`eye-level shot`, `medium shot`) guarantees
it. The original is archived beside the views as `concept.png`.

**Lanczos resize to 1536², no upscaler model.** A generative upscaler invents detail, and
invented detail differs between views — the opposite of what reconstruction needs. 1024 is
16 pixels short of Meshy's minimum, so all four images are resized; 1536 leaves headroom.

**No background removal and no camera node.** The plain background and diffuse light are
prompted, and so is the absence of specular highlights, which Meshy names as a failure mode
on metal. The camera custom node only builds the LoRA's prompt string; the prompts are fixed
text here. A background-removal node is added only if the test shows it is needed.

**The blade stands vertical with its flat side to the camera.** The LoRA's azimuths rotate
the camera about the vertical axis, so `left side view` and `right side view` then show the
edge — which is what they must show for a sword.

**The weapon's name never enters ComfyUI.** Save nodes use fixed prefixes, and outputs are
fetched by job id into the archive folder named after the weapon, renamed to `main.png`,
`left.png`, `back.png`, `right.png` and `concept.png` on the way.

**Images never enter the repository.** They are source material and belong in the asset
archive on `E:`, with Meshy's originals.

## Installation

```
H:\AI\ComfyUI_windows_portable\              ComfyUI v0.35.0, Windows portable, nvidia build
  run_nvidia_gpu.bat                         starts ComfyUI on 127.0.0.1:8188
  ComfyUI\models\
    checkpoints\       <the user's SDXL finetune>
    diffusion_models\  qwen_image_edit_2511_fp8mixed.safetensors
    text_encoders\     qwen_2.5_vl_7b_fp8_scaled.safetensors
    vae\               qwen_image_vae.safetensors
    loras\             Qwen-Image-Edit-2511-Lightning-4steps-V1.0-bf16.safetensors
                       qwen-image-edit-2511-multiple-angles-lora.safetensors
```

| File | Size | Source |
|---|---|---|
| `ComfyUI_windows_portable_nvidia.7z` (v0.35.0) | 1.91 GB | `Comfy-Org/ComfyUI` releases |
| `qwen_image_edit_2511_fp8mixed.safetensors` | 20.5 GB | `Comfy-Org/Qwen-Image-Edit_ComfyUI` |
| `qwen_2.5_vl_7b_fp8_scaled.safetensors` | 9.38 GB | `Comfy-Org/Qwen-Image_ComfyUI` |
| `qwen_image_vae.safetensors` | 254 MB | `Comfy-Org/Qwen-Image_ComfyUI` |
| `Qwen-Image-Edit-2511-Lightning-4steps-V1.0-bf16.safetensors` | 850 MB | `lightx2v/Qwen-Image-Edit-2511-Lightning` |
| `qwen-image-edit-2511-multiple-angles-lora.safetensors` | 295 MB | `fal/Qwen-Image-Edit-2511-Multiple-Angles-LoRA` |

All from public repositories, no token. About 33 GB in total; `H:` has 455 GB free, `C:` is
91 % full and is kept out of it.

**Precision.** Qwen-Image-Edit-2511 exists as bf16 (40.9 GB, beyond 24 GB of VRAM) and
fp8mixed (20.5 GB). The nvfp4 text encoder (6.11 GB) is not used: FP4 needs RTX 50-series
hardware, which an RTX 4090 is not.

**Licences.** The multiple-angles LoRA is Apache-2.0 per its model card. None of the
generated images ship in the mod — the mod ships Meshy's output, whose licensing is settled
in the weapon pipeline spec — so the image models' licences bear on this tooling rather than
on the release. Each licence is read and recorded when its file is downloaded.

**The old installation on `J:`** (ComfyUI 0.19.3) is left untouched. Its two Illustrious XL
checkpoints are linked **read-only** through `extra_model_paths.yaml` for the smoke test;
nothing is copied.

### Control: CLI and MCP, outside ComfyUI's Python

- **comfy-cli and comfy-mcp are installed as two separate `uv` tools.** comfy-mcp does not
  depend on comfy-cli; it shells out to whatever `COMFY_BIN` names. Neither touches
  ComfyUI's embedded Python — the lesson of the MV-Adapter conflict, applied to our own
  tooling.
- **ComfyUI is started with `run_nvidia_gpu.bat`**, and both tools reach it over HTTP at
  `127.0.0.1:8188`. The MCP's launch and stop tools are not used: they expect ComfyUI to
  run from comfy-cli's own Python.
- **The MCP is registered with scope `local`**, with `COMFY_BIN` set to the absolute path of
  uv's `comfy.exe`. That lands in the user's own Claude Code configuration, not in the
  repository — paths on `H:` and under the home directory do not belong in git. Its tools
  appear only **after the Claude Code session is restarted**.

## The workflows

Stored in **API format** as `tools/comfyui/weapon-concept.json` and
`tools/comfyui/weapon-views.json`. Runtime overrides address inputs as
`<node id>.<input name>`, so **the node ids below are fixed and part of the interface.**
Neither file contains a path — only model file names and relative save prefixes.

### Workflow 1 · concept

| Id | Node | Values |
|---|---|---|
| 1 | `CheckpointLoaderSimple` | `ckpt_name` — the finetune |
| 2 | `CLIPTextEncode` | positive prompt |
| 3 | `CLIPTextEncode` | negative prompt |
| 4 | `EmptyLatentImage` | 1024 × 1024, `batch_size` 4 |
| 5 | `KSampler` | `seed`, 30 steps, CFG 6, `dpmpp_2m`, `karras`, denoise 1.0 |
| 6 | `VAEDecode` | |
| 7 | `SaveImage` | `filename_prefix` `isekai/concept` |

The sampler values are SDXL defaults and are meant to be changed — finetunes often prescribe
their own.

**The positive prompt is a fixed frame with the design appended.** The frame is the
workflow's default text; a run replaces `2.text` with the frame plus the design:

> single fantasy one-handed sword, entire weapon in frame, blade vertical, flat side facing
> the camera, orthographic view, centered, plain light grey background, soft diffuse studio
> lighting, matte finish, no specular highlights, `<design>`

Negative, fixed: *hand, person, character, multiple weapons, cropped, perspective, dramatic
lighting, glare, reflections, specular highlights, scenery, text, watermark*.

| Override | Address |
|---|---|
| Checkpoint | `1.ckpt_name` |
| Prompt | `2.text` |
| Seed | `5.seed` |
| Images per run | `4.batch_size` |

### Workflow 2 · views

The shared part loads the models once; each of the four branches is fal's reference graph
for the multiple-angles LoRA, differing only in its prompt and its save prefix.

| Id | Node | Values |
|---|---|---|
| 1 | `LoadImage` | the chosen concept image |
| 2 | `UNETLoader` | `qwen_image_edit_2511_fp8mixed.safetensors` |
| 3 | `CLIPLoader` | `qwen_2.5_vl_7b_fp8_scaled.safetensors`, type `qwen_image` |
| 4 | `VAELoader` | `qwen_image_vae.safetensors` |
| 5 | `LoraLoaderModelOnly` | Lightning 4-steps, strength 1.0 |
| 6 | `LoraLoaderModelOnly` | multiple-angles, strength 1.0 |
| 7 | `ModelSamplingAuraFlow` | as in fal's reference workflow |
| 8 | `CFGNorm` | as in fal's reference workflow |
| 9 | `FluxKontextImageScale` | scales the input; a square image stays 1024² |
| 10–19 | branch **main** | `<sks> front view eye-level shot medium shot` → `isekai/views/main` |
| 20–29 | branch **left** | `<sks> left side view eye-level shot medium shot` → `isekai/views/left` |
| 30–39 | branch **back** | `<sks> back view eye-level shot medium shot` → `isekai/views/back` |
| 40–49 | branch **right** | `<sks> right side view eye-level shot medium shot` → `isekai/views/right` |

Every branch samples with **4 steps, CFG 1, `euler`, `simple`, denoise 1.0** and an empty
negative prompt — the values in fal's reference workflow, which loads the Lightning LoRA
together with the multiple-angles LoRA. Values the reference sets and this spec does not
list (the AuraFlow shift, the CFGNorm strength, the latent source) are copied from that file,
not chosen. Each branch ends in `ImageScale` to 1536 × 1536 with `lanczos`, then `SaveImage`.

Within a branch, ids follow one pattern — `x0` positive `TextEncodeQwenImageEditPlus`,
`x1` its reference-latent method, `x2` the negative encode, `x3` its method, `x4` `KSampler`,
`x5` `VAEDecode`, `x6` `ImageScale`, `x7` `SaveImage` — so the addresses are predictable:

| Override | Address |
|---|---|
| Concept image | `1.image` |
| Seed, all four branches | `14.seed`, `24.seed`, `34.seed`, `44.seed` — set together |

**Node provenance.** `TextEncodeQwenImageEditPlus` (`comfy_extras/nodes_qwen.py`),
`FluxKontextImageScale` and `FluxKontextMultiReferenceLatentMethod`
(`comfy_extras/nodes_flux.py`), `CFGNorm` (`comfy_extras/nodes_cfg.py`) and `ModelSamplingAuraFlow` (`comfy_extras/nodes_model_advanced.py`) are confirmed core nodes in ComfyUI's source. Both workflow
files are validated against the installed ComfyUI before their first run, which catches a
missing node or model file before any GPU work.

## Verification

**Before the user's finetune exists — a smoke test of the whole chain.** Workflow 1 runs on an
Illustrious XL checkpoint linked from `J:`, with a test design; one image is chosen and fed to
workflow 2. The anime style is irrelevant here: the test is of the chain, not of the look.

A views run passes when:

1. four PNGs arrive in the archive folder, each **1536×1536**, named for Meshy's slots;
2. every background is plain, with no strong specular highlights on the blade;
3. all four recognisably show **the same sword**, at the same size in frame;
4. `left` and `right` show the **edge** of the blade, and `back` its reverse.

Both runs are timed, and system memory is watched during the views run. If it pages, the
remedy is one of ComfyUI's memory flags at start-up, not a change to the design.

**The real test is a Meshy multi-image run** on the four views, done by the user. If the
edge views come out unusable, route B — MV-Adapter in its own environment — is added behind
the same concept workflow.

**A known risk for later designs.** Which edge the LoRA calls "left" matters only for an
asymmetric weapon; a mirrored reconstruction of a symmetric blade is identical. Check it the
first time a weapon is not symmetric.

## What changes in the repository

- `tools/comfyui/weapon-concept.json` and `tools/comfyui/weapon-views.json` — new.
- `docs/WEAPON_ASSET_GUIDE.md` — a stage before A for the concept views, the tools in the
  tools table, and any silent failure the smoke test turns up.

Everything else stays local: the installation, the models, `extra_model_paths.yaml`, the MCP
registration and every generated image. The pre-push byte scan covers the two workflow files
like any other tracked file.
