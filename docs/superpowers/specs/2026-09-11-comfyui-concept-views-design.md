# Local concept views for mod assets: ComfyUI to Meshy multi-image

Generate, on this machine, the four images Meshy's multi-image-to-3D wants — **main, left,
back, right** — for weapons, shields and armour, from a written design and an SDXL finetune the
user chooses. Armour is generated **worn on the reference bodies it must fit, 3BA and HIMBO**,
in their shared bind pose. All workflows are driven from Claude Code through comfy-cli and the
official comfy-mcp, and remain usable by hand in the ComfyUI interface.

For weapons this replaces the *input* of stage A in `docs/WEAPON_ASSET_GUIDE.md` — one concept
image from an external tool becomes four consistent views generated locally; everything
downstream of Meshy is unchanged. For armour it is the input of a pipeline that is not written
yet (see Non-goals).

## Goals

- Three ComfyUI workflows, versioned in the repository: **concept** (SDXL), **dress**
  (Qwen-Image-Edit-2511, armour only) and **views** (Qwen-Image-Edit-2511 with two LoRAs).
- One Blender script that renders the reference bodies as plain mannequins.
- Output that meets Meshy's multi-view input contract (below) without manual editing.
- **No custom nodes, and nothing installed into ComfyUI's Python.** All workflows use core
  nodes only.
- The same workflow files run from the CLI, from the MCP, and in the ComfyUI interface.

## Non-goals

- **Making armour wearable in Skyrim.** Removing the body from Meshy's mesh, skinning to 3BA
  and HIMBO including their physics bones, BodySlide projects with `_0`/`_1` weights, ARMO and
  ARMA records, first person, and the permissions for shipping conversions — that is its own
  pipeline with its own spec, as the weapon one was. This spec delivers images. It takes exactly
  two things from that pipeline: the bind pose, and that moving parts must be separable.
- **Automating the Meshy upload.** The API needs a key and costs credits per task; manual
  upload stays until several assets come in a row.
- **Choosing the SDXL finetune.** The user supplies it. A smoke test uses a checkpoint
  already on disk.
- **MV-Adapter.** Documented below as the fallback, not built.
- Upscaler models, background-removal nodes, ControlNet, the Qwen camera-angle custom node.
- Portrait resolution buckets: every image is **1024×1024**, as the user specified — body
  armour included, which then fills about a third of the width. Revisit only if Meshy visibly
  lacks detail.
- Characters and creatures.

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
is not guaranteed. The hardest cases are the edge-on view of a thin blade and, for armour,
keeping the body's proportions in the side views. The first test decides. If views fail,
MV-Adapter in its own environment is an addition — the concept and dress workflows, the
installation and the archive step stay as they are.

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

### Auto Split, and what it does not promise

Meshy's Auto Split finds "natural structural boundaries — like where a neck meets a torso" and
cuts there; **every cut is capped into a closed part**. It runs only on **Standard, untextured
draft** models from Meshy 6 and 7, at 10 credits. Meshy documents it for 3D printing and says
nothing about separating clothing or armour from a body.

So whether a worn armour comes apart from the body under it is **unknown until the first
armour run**, and three questions go to that run: does Auto Split separate armour from skin;
can the parts still be textured, given it only runs on untextured drafts; and are capped
closed parts usable where Skyrim needs open shells. The design does not depend on the answer —
see *Armour is generated on the body* below.

## Architecture

```
 Blender, once per body
   CBBE 3BA / HIMBO body + vanilla head, hands, feet ──► grey mannequin, 1024² ──► mannequins
                                                                                     │
 Workflow 1 · concept                (repeat until one design is right)              │
   SDXL finetune ── prompt, seed ──► 1024×1024, batch of 4 ──► concept images        │
                                                                   │                 │
                                           the user picks one ◄────┘                 │
 Workflow 2 · dress                  armour only                   ▼                 ▼
   Picture 1 = mannequin, Picture 2 = design ──► Qwen-Image-Edit-2511 + Lightning ──► dressed
     3BA   : Picture 2 = the chosen concept
     HIMBO : Picture 2 = the dressed 3BA image
                                                                   │
 Workflow 3 · views                  weapon: the concept · armour: each dressed image
   input ─► Qwen-Image-Edit-2511 fp8 + Lightning LoRA + multiple-angles LoRA
     ├─ "<sks> front view eye-level shot medium shot"      ─► 1536² ─► main
     ├─ "<sks> left side view eye-level shot medium shot"  ─► 1536² ─► left
     ├─ "<sks> back view eye-level shot medium shot"       ─► 1536² ─► back
     └─ "<sks> right side view eye-level shot medium shot" ─► 1536² ─► right

   fetched by job id ─► E:\_isekai_assets\<asset>\ ─► Meshy, by hand
```

**Separate workflows, with a human choice between them.** Concept generation is a creative
loop over many seeds; dressing and views are expensive — a 20B model, and four branches for the
views. In one workflow every rejected design would pay for them. It also means SDXL and Qwen
never have to be loaded at the same time, which matters with 32 GB of system RAM.

**`main` goes through Qwen as well.** Meshy requires the same distance and scale in every
image. The input comes from a different model or step with its own framing; running all four
branches with the same elevation and distance (`eye-level shot`, `medium shot`) guarantees it.
The input is archived beside the views.

**Lanczos resize to 1536², no upscaler model.** A generative upscaler invents detail, and
invented detail differs between views — the opposite of what reconstruction needs. 1024 is
16 pixels short of Meshy's minimum, so all four images are resized; 1536 leaves headroom.

**No background removal and no camera node.** The plain background and diffuse light are
prompted, and so is the absence of specular highlights, which Meshy names as a failure mode
on metal. The camera custom node only builds the LoRA's prompt string; the prompts are fixed
text here. A background-removal node is added only if the test shows it is needed.

**A blade or a shield stands upright with its face to the camera.** The LoRA's azimuths
rotate the camera about the vertical axis, so `left side view` and `right side view` then show
the edge — which is what they must show.

**The asset's name never enters ComfyUI.** Save nodes use fixed prefixes, and outputs are
fetched by job id into the archive folder named after the asset, renamed on the way.

**Images never enter the repository.** They are source material and belong in the asset
archive on `E:`, with Meshy's originals.

### Armour decisions

**Armour is generated on the body, not as an empty shell.** Image models draw worn armour far
more reliably than a hollow one, and Meshy may split the result (above). If it does not, the
body in the mesh is still exactly the reference body in the reference pose — laid over it, the
body can be removed geometrically, which is the armour pipeline's business. Either way the body
in the image is worth having.

**The mannequin is the reference body itself, rendered in the bind pose.** Measured from the
skeleton nodes of each file:

| Body | File | Upper arm below horizontal |
|---|---|---|
| Vanilla female, male | `femalebody_1.nif`, `malebody_1.nif` | 65.6° |
| CBBE 3BA | `CBBE 3BA Ref.nif` | 65.6° |
| HIMBO | shape `HIMBO - Body` in a HIMBO conversion | 65.6° |

One pose for every body: arms hanging about 24° away from the torso. That is **neither a
T-pose (0°) nor an A-pose (45°)**, so Meshy's pose toggle stays off — either setting would move
the arms away from the body the armour must fit.

**Qwen dresses the mannequin; SDXL stays the source of the design.** `TextEncodeQwenImageEditPlus`
takes up to three images (`image1`–`image3`, addressed in the prompt as *Picture 1*, *Picture
2*; confirmed in `comfy_extras/nodes_qwen.py`). Qwen edits the mannequin's own pixels, which
keeps pose and proportions better than the two alternatives considered:

- *SDXL img2img from the render* — the stronger the denoise, the more pose and build drift; the
  weaker, the less armour.
- *SDXL with ControlNet* — holds the pose, but needs another model, and whether SDXL ControlNets
  work cleanly with Illustrious-based finetunes is open.

**One design on both bodies.** 3BA is dressed first, with the chosen concept as Picture 2.
HIMBO is dressed next, with the dressed 3BA image as Picture 2, so the design carries over and
only the body under it changes. Picture 2 is always connected; the workflow has no optional
inputs.

**The whole outfit in one image** — helmet, cuirass, gauntlets, boots — because the boundaries
between them are the ones Auto Split cuts at.

**Moving parts must read as separate pieces.** Both bodies carry physics bones:

| Body | Physics bones |
|---|---|
| CBBE 3BA | `L/R Breast01–03`, `NPC Belly`, `NPC L/R Butt`, `NPC L/R FrontThigh`, `NPC L/R RearThigh`, `NPC L/R Pussy02`, `Clitoral1` |
| HIMBO | `NPC GenitalsBase [GenBase]` — as found in HIMBO conversions; HIMBO's own files may carry more |

A skirt, tasset, loincloth or cape drawn fused into a rigid plate becomes one mesh in Meshy and
cannot swing. The design text names such parts as separate, and the dress prompt keeps them so.
Weighting them to the bones is the armour pipeline's.

## Asset types

Each type has a fixed prompt frame for the concept workflow; a run writes the frame with the
design appended into `2.text`, and the type's negative into `3.text`.

| Type | Positive frame (`<design>` appended) | Negative |
|---|---|---|
| Weapon | single fantasy weapon, entire weapon in frame, upright, flat side facing the camera, orthographic view, centered, plain light grey background, soft diffuse studio lighting, matte finish, no specular highlights | hand, person, character, multiple weapons, cropped, perspective, dramatic lighting, glare, reflections, specular highlights, scenery, text, watermark |
| Shield | single fantasy shield, entire shield in frame, upright, front face to the camera, orthographic view, centered, plain light grey background, soft diffuse studio lighting, matte finish, no specular highlights | hand, person, character, multiple shields, cropped, perspective, dramatic lighting, glare, reflections, specular highlights, scenery, text, watermark |
| Armour | full fantasy armour set on a plain mannequin, helmet, cuirass, gauntlets and boots, full figure in frame, standing, front view, orthographic view, centered, plain light grey background, soft diffuse studio lighting, matte finish, no specular highlights | nude, weapon, multiple figures, cropped, perspective, dramatic lighting, glare, reflections, specular highlights, scenery, text, watermark |

The armour concept's pose does not matter — it is only Picture 2 for the dress step.

Archive layout per asset:

```
E:\_isekai_assets\<asset>\
  concept.png                      the chosen SDXL image
  views\main.png left.png back.png right.png                  weapon, shield
  dressed\3ba.png himbo.png                                   armour
  views\3ba\main.png …   views\himbo\main.png …               armour
```

## The mannequins

`tools/render-mannequin.py`, run headless in Blender with PyNifly:

```
blender --background --python tools/render-mannequin.py -- \
    --body <nif> [--shape <name>] --head <nif> --hands <nif> --feet <nif> --out <png>
```

- Imports the body — one named shape if the file holds several — and the vanilla head, hands and
  feet. All are skinned to the same skeleton in the bind pose, so they meet at neck, wrists and
  ankles without being moved.
- One matte mid-grey material on everything, against a plain light-grey world: the mannequin
  must not be close to the background's colour.
- Workbench renderer — deterministic, no light rig, no sampling noise.
- Orthographic camera in front of the face, at the figure's mid height; the figure fills 90 % of
  the frame's height, centred. **1024×1024.**
- Prints a JSON report and verifies the written PNG — size and that the figure is not cut at
  any border — rather than trusting the render call.

| Input | Where it comes from |
|---|---|
| `CBBE 3BA Ref.nif` | CBBE 3BA's `CalienteTools/BodySlide/ShapeData/CBBE 3BA Reference/`, in the modlist |
| shape `HIMBO - Body` | a HIMBO conversion that carries the full body — Kreiste's Samurai Outfit, `HIMBO KHO - SAM/SAM - Body.nif`, 11,007 vertices; Iron Rose's is a 64-vertex stub — because **HIMBO itself is not installed**; the armour pipeline needs HIMBO's own files |
| `femalehead.nif`, `femalehands_1.nif`, `femalefeet_1.nif`, and the `male…` equivalents | `Skyrim - Meshes0.bsa`, `meshes/actors/character/character assets/`, extracted to `E:\_skyrim_ref\` |

Rendered once per body to `E:\_isekai_assets\_bodies\3ba.png` and `himbo.png`, and reused for
every armour. The body meshes are other mod authors' work and Bethesda's; neither they nor the
renders enter the repository or the mod.

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
91 % full and is kept out of it. The dress workflow needs no model beyond these, and the
mannequin script only Blender 5.2 and PyNifly, both already installed for the weapon pipeline.

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

Stored in **API format** as `tools/comfyui/concept.json`, `tools/comfyui/dress.json` and
`tools/comfyui/views.json`. Runtime overrides address inputs as `<node id>.<input name>`, so
**the node ids below are fixed and part of the interface.** No file contains a path — only
model file names and relative save prefixes.

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
their own. The file's default prompts are the weapon frame and negative from *Asset types*.

| Override | Address |
|---|---|
| Checkpoint | `1.ckpt_name` |
| Prompt — the type's frame plus the design | `2.text` |
| Negative — the type's | `3.text` |
| Seed | `5.seed` |
| Images per run | `4.batch_size` |

### Workflow 2 · dress

| Id | Node | Values |
|---|---|---|
| 1 | `LoadImage` | Picture 1 — the mannequin |
| 2 | `LoadImage` | Picture 2 — the design reference |
| 3 | `UNETLoader` | `qwen_image_edit_2511_fp8mixed.safetensors` |
| 4 | `CLIPLoader` | `qwen_2.5_vl_7b_fp8_scaled.safetensors`, type `qwen_image` |
| 5 | `VAELoader` | `qwen_image_vae.safetensors` |
| 6 | `LoraLoaderModelOnly` | Lightning 4-steps, strength 1.0 |
| 7 | `ModelSamplingAuraFlow` | as in fal's reference workflow |
| 8 | `CFGNorm` | as in fal's reference workflow |
| 9 | `FluxKontextImageScale` | Picture 1 |
| 10 | `FluxKontextImageScale` | Picture 2 |
| 11 | `TextEncodeQwenImageEditPlus` | positive, `image1` ← 9, `image2` ← 10 |
| 12 | `FluxKontextMultiReferenceLatentMethod` | on 11 |
| 13 | `TextEncodeQwenImageEditPlus` | negative, empty, same images |
| 14 | `FluxKontextMultiReferenceLatentMethod` | on 13 |
| 15 | `VAEEncode` | node 9's output — the result keeps the mannequin's size and framing |
| 16 | `KSampler` | `seed`, 4 steps, CFG 1, `euler`, `simple`, denoise 1.0 |
| 17 | `VAEDecode` | |
| 18 | `SaveImage` | `filename_prefix` `isekai/dressed` |

No multiple-angles LoRA here — the camera does not move. The shift, CFGNorm strength and method
values are the same as in the views workflow.

The default positive prompt:

> Dress the mannequin in Picture 1 in the armour shown in Picture 2. Keep the mannequin's pose,
> body proportions, framing and camera exactly as in Picture 1. Keep separate pieces such as
> skirts, tassets and capes separate. Plain light grey background, soft diffuse studio lighting,
> matte finish, no specular highlights.

| Override | Address |
|---|---|
| Mannequin | `1.image` |
| Design reference | `2.image` |
| Prompt, when a design needs a note | `11.prompt` |
| Seed | `16.seed` |

### Workflow 3 · views

The shared part loads the models once; each of the four branches is fal's reference graph
for the multiple-angles LoRA, differing only in its prompt and its save prefix.

| Id | Node | Values |
|---|---|---|
| 1 | `LoadImage` | the chosen concept (weapon, shield) or a dressed image (armour) |
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
| Input image | `1.image` |
| Seed, all four branches | `14.seed`, `24.seed`, `34.seed`, `44.seed` — set together |

**Node provenance.** `TextEncodeQwenImageEditPlus` (`comfy_extras/nodes_qwen.py`),
`FluxKontextImageScale` and `FluxKontextMultiReferenceLatentMethod`
(`comfy_extras/nodes_flux.py`), `CFGNorm` (`comfy_extras/nodes_cfg.py`) and
`ModelSamplingAuraFlow` (`comfy_extras/nodes_model_advanced.py`) are confirmed core nodes in
ComfyUI's source. All three workflow files are validated against the installed ComfyUI before
their first run, which catches a missing node, a wrong input name or a missing model file
before any GPU work.

## Verification

**Before the user's finetune exists — a smoke test of both chains.** Workflow 1 runs on an
Illustrious XL checkpoint linked from `J:`. The anime style is irrelevant here: the test is of
the chains, not of the look.

- **Weapon chain:** a sword concept → views.
- **Armour chain:** both mannequins rendered → an armour concept → dress 3BA → dress HIMBO with
  the 3BA result → views for each.

A mannequin passes when it is 1024×1024, the face points at the camera, head, hands and feet
join the body without a visible gap, the figure is centred and uncut, and the arms hang at the
bind pose.

A dress run passes when, laid over its mannequin at half opacity, head, hands, feet and arms sit
in the same place; the armour follows Picture 2's design; and the background stays plain.

A views run passes when:

1. four PNGs arrive in the archive folder, each **1536×1536**, named for Meshy's slots;
2. every background is plain, with no strong specular highlights;
3. all four recognisably show **the same object**, whole and at the same size in frame;
4. for a blade or shield, `left` and `right` show the **edge** and `back` the reverse; for
   armour, `back` shows a real back — not a mirrored front — and the arms keep the bind pose in
   all four.

All runs are timed, and system memory is watched during the dress and views runs. If it pages,
the remedy is one of ComfyUI's memory flags at start-up, not a change to the design.

**The real tests are Meshy runs**, done by the user: one weapon, and one armour that also
answers the three Auto Split questions. If the weapon's edge views come out unusable, route B —
MV-Adapter in its own environment — is added behind the same concept workflow.

**Known risks.**

- *Armour proportions in the side views.* If the views LoRA distorts the body, the fallback is
  to render the mannequin from all four angles and dress each render with the same Picture 2 —
  the pose stays exact, at the cost of the design being inferred four times.
- *Asymmetric weapons.* Which edge the LoRA calls "left" matters only for an asymmetric weapon;
  a mirrored reconstruction of a symmetric blade is identical. Check it the first time a weapon
  is not symmetric.

## What changes in the repository

- `tools/comfyui/concept.json`, `tools/comfyui/dress.json` and `tools/comfyui/views.json` — new.
- `tools/render-mannequin.py` — new.
- `docs/WEAPON_ASSET_GUIDE.md` — a stage before A for the concept views, the tools in the
  tools table, and any silent failure the smoke test turns up. The armour side is documented
  with the armour pipeline.

Everything else stays local: the installation, the models, `extra_model_paths.yaml`, the MCP
registration, the extracted and modlist meshes, the mannequin renders and every generated image.
The pre-push byte scan covers the new files like any other tracked file.
