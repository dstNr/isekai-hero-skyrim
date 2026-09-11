# ComfyUI Concept Views Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Generate Meshy-ready main/left/back/right views for weapons and armour locally, with three ComfyUI workflows and a mannequin render script, per `docs/superpowers/specs/2026-09-11-comfyui-concept-views-design.md`.

**Architecture:** ComfyUI v0.35.0 portable on `H:` runs core nodes only. SDXL makes the concept, Qwen-Image-Edit-2511 dresses a rendered 3BA/HIMBO mannequin (armour) and turns one image into four views with fal's multiple-angles LoRA. comfy-cli and comfy-mcp drive it over HTTP from outside ComfyUI's Python; Blender with PyNifly renders the mannequins.

**Tech Stack:** ComfyUI portable (nvidia), Qwen-Image-Edit-2511 fp8mixed, Lightning and multiple-angles LoRAs, comfy-cli 1.20.0, comfy-mcp 0.10.0, uv 0.12.10, Blender 5.2 + PyNifly, Python stdlib.

## Global Constraints

- Every artifact in git is English; chat with the user is German.
- Never bump the mod version (`CMakeLists.txt`, `package.ps1`, tags, `CHANGELOG.md` headings).
- **No custom nodes, and nothing installed into ComfyUI's Python.**
- The installation and all models live under `H:\AI\`; `C:` is kept out of the ~33 GB.
- The old install on `J:` is only **read**, through `extra_model_paths.yaml`; nothing on `J:` is written or copied.
- Generated images, mannequin renders, body meshes and scratch scripts never enter the repository. They live under `E:\_isekai_assets\`; vanilla meshes under `E:\_skyrim_ref\`.
- Nothing is staged under `C:\Users`. Scratch scripts for this plan go to `E:\_isekai_assets\_comfy\`.
- Workflow files contain no path — only model file names and relative save prefixes. Node ids are the interface and are exactly those in the spec.
- No committed file may contain the author's real name. Before every commit run the name check (Task 2, Step 7).
- The MCP is registered with scope `local`, never in `.mcp.json`.
- ComfyUI listens on `127.0.0.1:8188` only.
- Blender run from Git Bash needs Windows-style paths (`E:/…`) for its arguments; `/e/…` is not converted inside quoted paths with brackets or apostrophes.

---

### Task 1: ComfyUI portable and the models on H:

**Files:** none in the repository. Local: `H:\AI\ComfyUI_windows_portable\`, `…\ComfyUI\extra_model_paths.yaml`.

**Interfaces:**
- Produces: ComfyUI serving `http://127.0.0.1:8188`, with the six files below visible in its model lists and both `J:` checkpoints listed under `checkpoints`.

- [ ] **Step 1: Check free space**

Run: `df -h /h /e`
Expected: `H:` at least 40 GB available.

- [ ] **Step 2: Write the download script** to `E:\_isekai_assets\_comfy\download.sh`

```bash
set -euo pipefail
D=/h/AI/_downloads
mkdir -p "$D"
curl -L --fail -C - -o "$D/ComfyUI_windows_portable_nvidia.7z" \
  https://github.com/Comfy-Org/ComfyUI/releases/download/v0.35.0/ComfyUI_windows_portable_nvidia.7z
"/c/Program Files/7-Zip/7z.exe" x -y -o"H:\\AI" "H:\\AI\\_downloads\\ComfyUI_windows_portable_nvidia.7z" > /dev/null
M=/h/AI/ComfyUI_windows_portable/ComfyUI/models
get() { mkdir -p "$M/$1"; curl -L --fail -C - -o "$M/$1/$(basename "$2")" "$2"; }
get diffusion_models https://huggingface.co/Comfy-Org/Qwen-Image-Edit_ComfyUI/resolve/main/split_files/diffusion_models/qwen_image_edit_2511_fp8mixed.safetensors
get text_encoders    https://huggingface.co/Comfy-Org/Qwen-Image_ComfyUI/resolve/main/split_files/text_encoders/qwen_2.5_vl_7b_fp8_scaled.safetensors
get vae              https://huggingface.co/Comfy-Org/Qwen-Image_ComfyUI/resolve/main/split_files/vae/qwen_image_vae.safetensors
get loras            https://huggingface.co/lightx2v/Qwen-Image-Edit-2511-Lightning/resolve/main/Qwen-Image-Edit-2511-Lightning-4steps-V1.0-bf16.safetensors
get loras            https://huggingface.co/fal/Qwen-Image-Edit-2511-Multiple-Angles-LoRA/resolve/main/qwen-image-edit-2511-multiple-angles-lora.safetensors
echo DOWNLOADS_DONE
```

- [ ] **Step 3: Run it in the background**

Run: `bash /e/_isekai_assets/_comfy/download.sh` with `run_in_background`.
Expected on completion: last line `DOWNLOADS_DONE`. If a transfer breaks, run the script again — `-C -` resumes, and `7z x -y` re-extracts harmlessly.

- [ ] **Step 4: Verify every file by its exact byte size**

```bash
python - <<'EOF'
import os
M = r"H:\AI\ComfyUI_windows_portable\ComfyUI\models"
want = {
    r"H:\AI\_downloads\ComfyUI_windows_portable_nvidia.7z": 1910039517,
    M + r"\diffusion_models\qwen_image_edit_2511_fp8mixed.safetensors": 20533762817,
    M + r"\text_encoders\qwen_2.5_vl_7b_fp8_scaled.safetensors": 9384670680,
    M + r"\vae\qwen_image_vae.safetensors": 253806246,
    M + r"\loras\Qwen-Image-Edit-2511-Lightning-4steps-V1.0-bf16.safetensors": 849608296,
    M + r"\loras\qwen-image-edit-2511-multiple-angles-lora.safetensors": 295140688,
}
bad = {p: (os.path.getsize(p) if os.path.exists(p) else None, n) for p, n in want.items()
       if not os.path.exists(p) or os.path.getsize(p) != n}
print("SIZES OK" if not bad else bad)
EOF
```

Expected: `SIZES OK`.

- [ ] **Step 5: Link the J: checkpoints read-only**

Write `H:\AI\ComfyUI_windows_portable\ComfyUI\extra_model_paths.yaml`:

```yaml
# The old installation on J: is only read from; downloads go to this installation.
j_old_install:
  base_path: J:/SD/ComfyUI/ComfyUI_windows_portable/ComfyUI/
  checkpoints: models/checkpoints/
```

- [ ] **Step 6: Start ComfyUI**

Run in the background (no browser, same as `run_nvidia_gpu.bat` otherwise):
`cd /h/AI/ComfyUI_windows_portable && ./python_embeded/python.exe -s ComfyUI/main.py --listen 127.0.0.1 --port 8188`

Wait for it with Monitor, until `curl -s -o /dev/null -w "%{http_code}" http://127.0.0.1:8188/system_stats` prints `200`.

- [ ] **Step 7: Verify nodes and models**

```bash
python - <<'EOF'
import json, urllib.request
get = lambda p: json.load(urllib.request.urlopen("http://127.0.0.1:8188" + p))
nodes = get("/object_info")
need = ["TextEncodeQwenImageEditPlus", "FluxKontextImageScale", "FluxKontextMultiReferenceLatentMethod",
        "CFGNorm", "ModelSamplingAuraFlow", "UNETLoader", "CLIPLoader", "VAELoader",
        "LoraLoaderModelOnly", "VAEEncode", "ImageScale", "CheckpointLoaderSimple"]
print("missing nodes:", [n for n in need if n not in nodes])
methods = nodes["FluxKontextMultiReferenceLatentMethod"]["input"]["required"]["reference_latents_method"]
print("index_timestep_zero offered:", "index_timestep_zero" in json.dumps(methods))
for folder in ["checkpoints", "diffusion_models", "text_encoders", "vae", "loras"]:
    print(folder, get("/models/" + folder))
EOF
```

Expected: `missing nodes: []`, `index_timestep_zero offered: True`, `checkpoints` lists `ilustmix_v111.safetensors` and `prefectIllustriousXL_v70.safetensors`, and each other folder lists its file(s) from Step 4.

- [ ] **Step 8: Remove the archive**

Run: `rm /h/AI/_downloads/ComfyUI_windows_portable_nvidia.7z && rmdir /h/AI/_downloads`

No commit — nothing in the repository changed.

---

### Task 2: comfy-cli, comfy-mcp and the run helper

**Files:** none in the repository. Local: `E:\_isekai_assets\_comfy\comfy_job.py`; the user's Claude Code config (MCP, scope local).

**Interfaces:**
- Consumes: ComfyUI on `127.0.0.1:8188` (Task 1).
- Produces:
  - `comfy` on PATH (comfy-cli 1.20.0) with the default workspace set to `H:\AI\ComfyUI_windows_portable\ComfyUI`.
  - `python E:/_isekai_assets/_comfy/comfy_job.py set <workflow.json> <out.json> <id.input=JSON>...` — writes a copy with inputs replaced; fails on an unknown address.
  - `python E:/_isekai_assets/_comfy/comfy_job.py stage <image> <name>` — copies an image into ComfyUI's `input` folder under `<name>`.
  - `python E:/_isekai_assets/_comfy/comfy_job.py submit <workflow.json>` — submits through `comfy run` and prints the `prompt_id`.
  - `python E:/_isekai_assets/_comfy/comfy_job.py collect <prompt_id> <dir> [<node>=<name>]...` — waits for the job, copies its images to `<dir>` as `<name>.png` (or `<name>_<n>.png` for batches), prints a JSON report with each file's pixel size, the run's seconds and the lowest free RAM seen.
  - Name check command (Step 7).

- [ ] **Step 1: Install both tools as separate uv tools**

Run: `uv tool install comfy-cli==1.20.0 && uv tool install comfy-mcp==0.10.0 && comfy --version`
Expected: `1.20.0` in the version output.

- [ ] **Step 2: Point comfy-cli at the installation**

Run: `comfy --skip-prompt tracking disable && comfy --skip-prompt set-default "H:\AI\ComfyUI_windows_portable\ComfyUI"`
Expected: the default workspace is reported as that path. If `set-default` refuses the portable folder, continue — `run` only needs the server on `127.0.0.1:8188` — and record the refusal for the guide (Task 7).

- [ ] **Step 3: Write the helper** to `E:\_isekai_assets\_comfy\comfy_job.py`

```python
"""Scratch helper for the smoke tests: override inputs, stage images, submit, collect a job."""
import json
import shutil
import struct
import subprocess
import sys
import time
import urllib.request
from pathlib import Path

COMFY = Path(r"H:\AI\ComfyUI_windows_portable\ComfyUI")
URL = "http://127.0.0.1:8188"


def get(path):
    with urllib.request.urlopen(URL + path) as r:
        return json.load(r)


def png_size(path):
    with open(path, "rb") as f:
        head = f.read(24)
    return list(struct.unpack(">II", head[16:24]))


cmd, args = sys.argv[1], sys.argv[2:]
if cmd == "set":
    wf = json.loads(Path(args[0]).read_text(encoding="utf-8"))
    for a in args[2:]:
        addr, value = a.split("=", 1)
        node, name = addr.split(".", 1)
        assert node in wf and name in wf[node]["inputs"], "unknown address " + addr
        wf[node]["inputs"][name] = json.loads(value)
    Path(args[1]).write_text(json.dumps(wf, indent=2), encoding="utf-8")
elif cmd == "stage":
    shutil.copy(args[0], COMFY / "input" / args[1])
elif cmd == "submit":
    out = subprocess.run(["comfy", "--json", "--where", "local", "run", "--workflow", args[0]],
                         capture_output=True, text=True, check=True).stdout
    print(json.loads(out)["data"]["prompt_id"])
elif cmd == "collect":
    pid, dest = args[0], Path(args[1])
    names = dict(a.split("=", 1) for a in args[2:])
    ram_low = None
    while True:
        free = get("/system_stats")["system"]["ram_free"]
        ram_low = free if ram_low is None else min(ram_low, free)
        job = get("/history/" + pid).get(pid)
        if job and job.get("status", {}).get("completed") is not None:
            break
        time.sleep(5)
    status = job["status"]
    if status.get("status_str") != "success":
        print(json.dumps(status["messages"], indent=2))
        sys.exit(1)
    stamps = {m[0]: m[1].get("timestamp") for m in status["messages"]}
    dest.mkdir(parents=True, exist_ok=True)
    files = {}
    for node, out in job["outputs"].items():
        images = out.get("images", [])
        for i, img in enumerate(images):
            name = names.get(node, node) + ("" if len(images) == 1 else "_%d" % (i + 1)) + ".png"
            shutil.copy(COMFY / "output" / img["subfolder"] / img["filename"], dest / name)
            files[name] = png_size(dest / name)
    print(json.dumps({
        "files": files,
        "seconds": round((stamps["execution_success"] - stamps["execution_start"]) / 1000, 1),
        "ram_free_low_gb": round(ram_low / 2**30, 1),
    }, indent=2))
```

- [ ] **Step 4: Prove the CLI reaches the server**

Run: `comfy --json --where local system-stats`
Expected: a JSON envelope whose `data` names the RTX 4090. If the verb is missing in 1.20.0, `curl -s http://127.0.0.1:8188/system_stats` must still answer and the `submit` path is proven in Task 3 instead.

- [ ] **Step 5: Register the MCP, scope local**

```bash
BIN="$(uv tool dir --bin)"
claude mcp add comfy-mcp --scope local -e COMFY_BIN="$BIN/comfy.exe" -- "$BIN/comfy-mcp.exe"
claude mcp list | grep comfy-mcp
```

Expected: `comfy-mcp` listed. Its tools appear only after a Claude Code restart, which is Task 7's last step.

- [ ] **Step 6: Confirm nothing landed in the repository**

Run: `git status --short`
Expected: no output — no `.mcp.json`, no workflow copies.

- [ ] **Step 7: The name check, run before every commit of this plan**

```bash
for w in $(python -c "import os; print(os.path.basename(os.environ['USERPROFILE']))"); do
  git grep -niI -- "$w" -- tools docs && echo "NAME FOUND: $w"
done; echo NAME_CHECK_DONE
```

Expected: only `NAME_CHECK_DONE`.

No commit.

---

### Task 3: The concept workflow and the weapon concept smoke test

**Files:**
- Create: `tools/comfyui/concept.json`
- Local: `E:\_isekai_assets\_comfy\build_workflows.py`, output in `E:\_isekai_assets\_smoketest\sword\`

**Interfaces:**
- Consumes: `comfy_job.py set|submit|collect` (Task 2).
- Produces: `tools/comfyui/concept.json` with overrides `1.ckpt_name`, `2.text`, `3.text`, `4.batch_size`, `5.seed`; `build_workflows.py <name> <out>` with a `BUILDERS` dict later tasks extend.

- [ ] **Step 1: Write the builder** to `E:\_isekai_assets\_comfy\build_workflows.py`

```python
"""Builds the API-format workflows. The committed JSON is the source; this only writes it once."""
import json
import sys

WEAPON_FRAME = ("single fantasy weapon, entire weapon in frame, upright, flat side facing the camera, "
                "orthographic view, centered, plain light grey background, soft diffuse studio lighting, "
                "matte finish, no specular highlights")
WEAPON_NEGATIVE = ("hand, person, character, multiple weapons, cropped, perspective, dramatic lighting, "
                   "glare, reflections, specular highlights, scenery, text, watermark")


def node(class_type, title, **inputs):
    return {"class_type": class_type, "inputs": inputs, "_meta": {"title": title}}


def concept():
    return {
        "1": node("CheckpointLoaderSimple", "Checkpoint", ckpt_name="prefectIllustriousXL_v70.safetensors"),
        "2": node("CLIPTextEncode", "Positive", text=WEAPON_FRAME, clip=["1", 1]),
        "3": node("CLIPTextEncode", "Negative", text=WEAPON_NEGATIVE, clip=["1", 1]),
        "4": node("EmptyLatentImage", "Latent", width=1024, height=1024, batch_size=4),
        "5": node("KSampler", "Sampler", seed=0, steps=30, cfg=6.0, sampler_name="dpmpp_2m",
                  scheduler="karras", denoise=1.0, model=["1", 0], positive=["2", 0],
                  negative=["3", 0], latent_image=["4", 0]),
        "6": node("VAEDecode", "Decode", samples=["5", 0], vae=["1", 2]),
        "7": node("SaveImage", "Save", filename_prefix="isekai/concept", images=["6", 0]),
    }


BUILDERS = {"concept": concept}

if __name__ == "__main__":
    name, out = sys.argv[1], sys.argv[2]
    with open(out, "w", encoding="utf-8", newline="\n") as f:
        json.dump(BUILDERS[name](), f, indent=2)
        f.write("\n")
```

- [ ] **Step 2: Build the file**

Run: `mkdir -p tools/comfyui && python E:/_isekai_assets/_comfy/build_workflows.py concept tools/comfyui/concept.json`

- [ ] **Step 3: Run the sword concept on Illustrious**

```bash
J=E:/_isekai_assets/_comfy
python $J/comfy_job.py set tools/comfyui/concept.json $J/run-concept.json \
  '2.text="single fantasy weapon, entire weapon in frame, upright, flat side facing the camera, orthographic view, centered, plain light grey background, soft diffuse studio lighting, matte finish, no specular highlights, one-handed longsword, steel blade with a faint ember-orange glow along the fuller, blackened crossguard, leather-wrapped grip, round pommel"' \
  '5.seed=1101'
PID=$(python $J/comfy_job.py submit $J/run-concept.json)
python $J/comfy_job.py collect $PID E:/_isekai_assets/_smoketest/sword 7=concept
```

Expected: `files` holds `concept_1.png` … `concept_4.png`, each `[1024, 1024]`; `seconds` printed. A validation error instead (unknown node, bad input name, missing model) arrives before any sampling — fix the builder, rebuild, rerun.

- [ ] **Step 4: Look at the four images**

Read the four PNGs. Pass: a single whole sword, upright, flat side to the camera, plain light background. Record the timing. Copy the best one to `E:\_isekai_assets\_smoketest\sword\concept.png`.

- [ ] **Step 5: Name check (Task 2, Step 7), then commit**

```bash
git add tools/comfyui/concept.json
git commit -m "feat(tools): ComfyUI concept workflow for asset designs"
```

---

### Task 4: The views workflow and the weapon views smoke test

**Files:**
- Create: `tools/comfyui/views.json`
- Modify (local): `E:\_isekai_assets\_comfy\build_workflows.py`

**Interfaces:**
- Consumes: `E:\_isekai_assets\_smoketest\sword\concept.png` (Task 3), `comfy_job.py` (Task 2).
- Produces: `tools/comfyui/views.json` with overrides `1.image`; `14.seed`, `24.seed`, `34.seed`, `44.seed`; save nodes `17`, `27`, `37`, `47` for main, left, back, right. Builder helpers for Task 6: `node(class_type, title, **inputs)`, `qwen_models(wf, loras)`, `edit_sample(wf, ids, prompt, images, latent, prefix)` with `ids` = (positive, its method, negative, its method, sampler, decode, scale or `None`, save), `finish(wf)`, and the constant `LIGHTNING`.

- [ ] **Step 1: Add the views builder** — insert above `BUILDERS` in `build_workflows.py`

```python
QWEN_UNET = "qwen_image_edit_2511_fp8mixed.safetensors"
QWEN_CLIP = "qwen_2.5_vl_7b_fp8_scaled.safetensors"
QWEN_VAE = "qwen_image_vae.safetensors"
LIGHTNING = "Qwen-Image-Edit-2511-Lightning-4steps-V1.0-bf16.safetensors"
ANGLES = "qwen-image-edit-2511-multiple-angles-lora.safetensors"
# From fal's reference workflow for the multiple-angles LoRA; copied, not chosen.
SHIFT = 3.1
CFG_NORM = 1.0
METHOD = "index_timestep_zero"


def qwen_models(wf, loras):
    """The model chain at the ids in wf["_ids"]: UNET, CLIP, VAE, the LoRAs in order, AuraFlow, CFGNorm."""
    ids = wf["_ids"]
    wf[ids["unet"]] = node("UNETLoader", "Qwen Image Edit 2511", unet_name=QWEN_UNET, weight_dtype="default")
    wf[ids["clip"]] = node("CLIPLoader", "Qwen 2.5 VL", clip_name=QWEN_CLIP, type="qwen_image", device="default")
    wf[ids["vae"]] = node("VAELoader", "Qwen VAE", vae_name=QWEN_VAE)
    prev = ids["unet"]
    for lora_id, lora in zip(ids["loras"], loras):
        wf[lora_id] = node("LoraLoaderModelOnly", lora, lora_name=lora, strength_model=1.0, model=[prev, 0])
        prev = lora_id
    wf[ids["aura"]] = node("ModelSamplingAuraFlow", "Shift", shift=SHIFT, model=[prev, 0])
    wf[ids["norm"]] = node("CFGNorm", "CFGNorm", strength=CFG_NORM, model=[ids["aura"], 0])


def edit_sample(wf, ids, prompt, images, latent, prefix):
    """One fal-style edit. ids = (positive, its method, negative, its method, sampler, decode,
    scale or None, save)."""
    pos, pos_m, neg, neg_m, sampler, decode, scale, save = ids
    m = wf["_ids"]
    refs = {"image%d" % (i + 1): [img, 0] for i, img in enumerate(images)}
    common = dict(clip=[m["clip"], 0], vae=[m["vae"], 0], **refs)
    wf[pos] = node("TextEncodeQwenImageEditPlus", "Positive", prompt=prompt, **common)
    wf[pos_m] = node("FluxKontextMultiReferenceLatentMethod", "Positive method",
                     reference_latents_method=METHOD, conditioning=[pos, 0])
    wf[neg] = node("TextEncodeQwenImageEditPlus", "Negative", prompt="", **common)
    wf[neg_m] = node("FluxKontextMultiReferenceLatentMethod", "Negative method",
                     reference_latents_method=METHOD, conditioning=[neg, 0])
    wf[sampler] = node("KSampler", "Sampler", seed=0, steps=4, cfg=1.0, sampler_name="euler",
                       scheduler="simple", denoise=1.0, model=[m["norm"], 0],
                       positive=[pos_m, 0], negative=[neg_m, 0], latent_image=[latent, 0])
    wf[decode] = node("VAEDecode", "Decode", samples=[sampler, 0], vae=[m["vae"], 0])
    last = decode
    if scale:
        wf[scale] = node("ImageScale", "Lanczos 1536", upscale_method="lanczos", width=1536,
                         height=1536, crop="disabled", image=[last, 0])
        last = scale
    wf[save] = node("SaveImage", "Save", filename_prefix=prefix, images=[last, 0])


def finish(wf):
    del wf["_ids"]
    return dict(sorted(wf.items(), key=lambda kv: int(kv[0])))


VIEWS = [(10, "main", "front view"), (20, "left", "left side view"),
         (30, "back", "back view"), (40, "right", "right side view")]


def views():
    wf = {"_ids": {"unet": "2", "clip": "3", "vae": "4", "loras": ["5", "6"], "aura": "7", "norm": "8"}}
    wf["1"] = node("LoadImage", "Input", image="isekai_views_input.png")
    qwen_models(wf, [LIGHTNING, ANGLES])
    wf["9"] = node("FluxKontextImageScale", "Scale input", image=["1", 0])
    wf["50"] = node("VAEEncode", "Latent from input", pixels=["9", 0], vae=["4", 0])
    for base, slot, view in VIEWS:
        edit_sample(wf, tuple(str(base + i) for i in range(8)),
                    "<sks> %s eye-level shot medium shot" % view, ["9"], "50", "isekai/views/" + slot)
    return finish(wf)
```

And change the registry line to: `BUILDERS = {"concept": concept, "views": views}`

- [ ] **Step 2: Build and check the ids**

```bash
python E:/_isekai_assets/_comfy/build_workflows.py views tools/comfyui/views.json
python -c "import json; w=json.load(open('tools/comfyui/views.json')); print(sorted(w, key=int)); print([w[i]['inputs']['filename_prefix'] for i in ('17','27','37','47')]); print(w['14']['class_type'], w['50']['class_type'])"
```

Expected: ids `1`–`9`, `10`–`17`, `20`–`27`, `30`–`37`, `40`–`47`, `50`; prefixes `isekai/views/main|left|back|right`; `KSampler VAEEncode`.

- [ ] **Step 3: Run the sword views**

```bash
J=E:/_isekai_assets/_comfy
python $J/comfy_job.py stage E:/_isekai_assets/_smoketest/sword/concept.png isekai_views_input.png
python $J/comfy_job.py set tools/comfyui/views.json $J/run-views.json \
  '14.seed=2202' '24.seed=2202' '34.seed=2202' '44.seed=2202'
PID=$(python $J/comfy_job.py submit $J/run-views.json)
python $J/comfy_job.py collect $PID E:/_isekai_assets/_smoketest/sword/views 17=main 27=left 37=back 47=right
```

Expected: `main.png`, `left.png`, `back.png`, `right.png`, each `[1536, 1536]`; `seconds` and `ram_free_low_gb` printed. If `ram_free_low_gb` is near 0 and the run crawled, restart ComfyUI with `--reserve-vram 2` (Task 1, Step 6) and rerun; record it.

- [ ] **Step 4: Judge against the spec's pass criteria**

Read the four PNGs. Pass: plain backgrounds, no strong highlights; the same sword at the same size in all four; `left` and `right` show the edge, `back` the reverse. Record pass or fail per criterion, the timing and the RAM low for Task 7. A fail on the edge views is a finding for the user, not a reason to change the design.

- [ ] **Step 5: Name check (Task 2, Step 7), then commit**

```bash
git add tools/comfyui/views.json
git commit -m "feat(tools): ComfyUI views workflow, four Meshy slots from one image"
```

---

### Task 5: The mannequin render script

**Files:**
- Create: `tools/render-mannequin.py`
- Local output: `E:\_isekai_assets\_bodies\3ba.png`, `E:\_isekai_assets\_bodies\himbo.png`

**Interfaces:**
- Consumes: `E:\_skyrim_ref\femalehead.nif`, `femalehands_1.nif`, `femalefeet_1.nif`, `malehead.nif`, `malehands_1.nif`, `malefeet_1.nif` (already extracted); the 3BA and HIMBO meshes in the modlist.
- Produces: `blender --background --python tools/render-mannequin.py -- --body <nif> [--shape <name>] --head <nif> --hands <nif> --feet <nif> --out <png>`; prints one line `MANNEQUIN_REPORT {json}`; the two PNGs.

- [ ] **Step 1: Write the script** to `tools/render-mannequin.py`

```python
"""Render a Skyrim reference body as a plain grey mannequin, the Picture 1 of the dress workflow.

The armour is generated worn on this exact body in its bind pose, so the mannequin must be the
reference body itself: the body NIF, plus vanilla head, hands and feet. Everything is skinned to
the same skeleton, so the parts meet at neck, wrists and ankles without being moved.

Run it headless:

    blender --background --python tools/render-mannequin.py -- \
        --body "<CBBE 3BA Ref.nif>" --head <femalehead.nif> --hands <femalehands_1.nif> \
        --feet <femalefeet_1.nif> --out <3ba.png>

The meshes are other mod authors' work and Bethesda's; they and the render stay outside the
repository.
"""

import argparse
import json
import math
import os
import sys

import bpy
import numpy as np

SIZE = 1024
FILL = 0.9       # figure height as a share of the frame
FIGURE = 0.35    # matte mid grey; Meshy fails on a background close to the object's colour
BACKGROUND = 0.8


def parse_args():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    p = argparse.ArgumentParser()
    p.add_argument("--body", required=True)
    p.add_argument("--shape", default=None, help="keep only this shape from the body NIF")
    p.add_argument("--head", required=True)
    p.add_argument("--hands", required=True)
    p.add_argument("--feet", required=True)
    p.add_argument("--out", required=True)
    return p.parse_args(argv)


def import_meshes(path, shape=None):
    before = set(bpy.data.objects.keys())
    bpy.ops.import_scene.pynifly(filepath=os.path.abspath(path))
    fresh = [bpy.data.objects[n] for n in bpy.data.objects.keys() if n not in before]
    meshes = [o for o in fresh if o.type == "MESH" and not o.name.startswith("bhk")]
    if shape:
        meshes = [o for o in meshes if o.get("pynNodeName", o.name) == shape]
    assert meshes, "no mesh %sin %s" % ("named %r " % shape if shape else "", path)
    return meshes


def world_verts(ob):
    me = ob.data
    n = len(me.vertices)
    co = np.empty(n * 3, dtype=np.float64)
    me.vertices.foreach_get("co", co)
    co = co.reshape(n, 3)
    m = np.array(ob.matrix_world.to_4x4())
    return (np.hstack([co, np.ones((n, 1))]) @ m.T)[:, :3]


def main():
    args = parse_args()
    out = os.path.abspath(args.out)
    for o in list(bpy.data.objects):
        bpy.data.objects.remove(o, do_unlink=True)

    parts = {
        "body": import_meshes(args.body, args.shape),
        "head": import_meshes(args.head),
        "hands": import_meshes(args.hands),
        "feet": import_meshes(args.feet),
    }
    keep = [o for group in parts.values() for o in group]

    # The rest mesh is the bind pose: detach from the imported armatures, drop their modifiers.
    for ob in keep:
        mw = ob.matrix_world.copy()
        ob.parent = None
        ob.matrix_world = mw
        for mod in list(ob.modifiers):
            ob.modifiers.remove(mod)
    for o in list(bpy.data.objects):
        if o not in keep:
            bpy.data.objects.remove(o, do_unlink=True)

    allv = np.vstack([world_verts(o) for o in keep])
    lo, hi = allv.min(axis=0), allv.max(axis=0)
    centre = (lo + hi) / 2.0
    height, width = hi[2] - lo[2], hi[0] - lo[0]
    assert width < height, "figure wider than tall — the arms are not in the bind pose"

    # Toes reach further from the ankle than the heel does: that side is the front.
    feet = np.vstack([world_verts(o) for o in parts["feet"]])
    ankle = feet[feet[:, 2] > feet[:, 2].max() - 0.2 * np.ptp(feet[:, 2])]
    ankle_y = ankle[:, 1].mean()
    forward = 1.0 if feet[:, 1].max() - ankle_y > ankle_y - feet[:, 1].min() else -1.0

    scene = bpy.context.scene
    cam_data = bpy.data.cameras.new("Mannequin")
    cam_data.type = "ORTHO"
    cam_data.ortho_scale = height / FILL
    distance = np.ptp(allv[:, 1]) + 200.0
    cam_data.clip_end = distance * 4.0
    cam = bpy.data.objects.new("Mannequin", cam_data)
    scene.collection.objects.link(cam)
    cam.location = (centre[0], centre[1] + forward * distance, centre[2])
    # A camera looks down -Z; +90° about X turns that to +Y, a further 180° about Z to -Y.
    cam.rotation_euler = (math.pi / 2.0, 0.0, math.pi if forward > 0 else 0.0)
    scene.camera = cam

    if scene.world is None:
        scene.world = bpy.data.worlds.new("Mannequin")
    scene.world.color = (BACKGROUND,) * 3
    shading = scene.display.shading
    shading.light = "STUDIO"
    shading.color_type = "SINGLE"
    shading.single_color = (FIGURE,) * 3
    scene.view_settings.view_transform = "Standard"

    r = scene.render
    r.engine = "BLENDER_WORKBENCH"
    r.resolution_x = r.resolution_y = SIZE
    r.resolution_percentage = 100
    r.film_transparent = False
    r.image_settings.file_format = "PNG"
    r.image_settings.color_mode = "RGB"
    r.filepath = out
    bpy.ops.render.render(write_still=True)

    # --- verify the written file, never the render call --------------------------
    img = bpy.data.images.load(out)
    w, h = img.size
    px = np.array(img.pixels[:], dtype=np.float32).reshape(h, w, 4)[:, :, :3]
    figure = np.abs(px - px[0, 0]).max(axis=2) > 0.02
    report = {
        "out": os.path.basename(out),
        "size": [w, h],
        "forward": "+Y" if forward > 0 else "-Y",
        "height": round(float(height), 2),
        "width": round(float(width), 2),
        "meshes": {k: [o.name for o in v] for k, v in parts.items()},
        "figure_share": round(float(figure.mean()), 3),
        "border_clear": bool(not (figure[0].any() or figure[-1].any()
                                  or figure[:, 0].any() or figure[:, -1].any())),
    }
    assert [w, h] == [SIZE, SIZE], "wrong size %s" % [w, h]
    assert figure.any(), "nothing rendered — background only"
    assert report["border_clear"], "figure cut at the frame's border"
    print("MANNEQUIN_REPORT " + json.dumps(report))


main()
```

- [ ] **Step 2: Render the 3BA mannequin**

```bash
BL="/c/Program Files/Blender Foundation/Blender 5.2/blender.exe"
R=E:/_skyrim_ref
mkdir -p /e/_isekai_assets/_bodies
"$BL" --background --python tools/render-mannequin.py -- \
  --body "E:/Modlists/NYA/mods/CBBE 3BA 2/CalienteTools/BodySlide/ShapeData/CBBE 3BA Reference/CBBE 3BA Ref.nif" \
  --head $R/femalehead.nif --hands $R/femalehands_1.nif --feet $R/femalefeet_1.nif \
  --out E:/_isekai_assets/_bodies/3ba.png 2>&1 | grep -E "MANNEQUIN_REPORT|Error|Traceback|assert"
```

Expected: one `MANNEQUIN_REPORT` with `"size": [1024, 1024]`, `"border_clear": true`, `figure_share` roughly 0.08–0.25, and `meshes.body` holding `3BA Ref`. On a traceback: read it, fix the script, rerun — do not loosen an assertion to pass.

- [ ] **Step 3: Render the HIMBO mannequin**

```bash
BL="/c/Program Files/Blender Foundation/Blender 5.2/blender.exe"
R=E:/_skyrim_ref
"$BL" --background --python tools/render-mannequin.py -- \
  --body "E:/Modlists/NYA/mods/Kreiste's Samurai Outfit (BHUNP - CBBE - HIMBO)/CalienteTools/BodySlide/ShapeData/HIMBO KHO - SAM/SAM - Body.nif" \
  --shape "HIMBO - Body" \
  --head $R/malehead.nif --hands $R/malehands_1.nif --feet $R/malefeet_1.nif \
  --out E:/_isekai_assets/_bodies/himbo.png 2>&1 | grep -E "MANNEQUIN_REPORT|Error|Traceback|assert"
```

Expected: as Step 2, with `meshes.body` holding only `HIMBO - Body`.

- [ ] **Step 4: Look at both renders**

Read both PNGs. Pass: the face points at the camera; head, hands and feet join the body without a visible gap; the figure is centred; the arms hang slightly away from the torso. If the figure faces away, the forward test is wrong — fix it in the script, not by hand.

- [ ] **Step 5: Name check (Task 2, Step 7), then commit**

```bash
git add tools/render-mannequin.py
git commit -m "feat(tools): render reference bodies as mannequins for armour concepts"
```

---

### Task 6: The dress workflow and the armour smoke test

**Files:**
- Create: `tools/comfyui/dress.json`
- Modify (local): `E:\_isekai_assets\_comfy\build_workflows.py`
- Local output: `E:\_isekai_assets\_smoketest\armour\`

**Interfaces:**
- Consumes: `node`, `qwen_models`, `edit_sample`, `finish`, `LIGHTNING` (Task 4); `concept.json` (Task 3); `views.json` (Task 4); `3ba.png`, `himbo.png` (Task 5).
- Produces: `tools/comfyui/dress.json` with overrides `1.image`, `2.image`, `11.prompt`, `16.seed`; save node `18`.

- [ ] **Step 1: Add the dress builder** — insert above `BUILDERS`

```python
DRESS_PROMPT = (
    "Dress the mannequin in Picture 1 in the armour shown in Picture 2. Keep the mannequin's pose, "
    "body proportions, framing and camera exactly as in Picture 1. Keep separate pieces such as "
    "skirts, tassets and capes separate. Plain light grey background, soft diffuse studio lighting, "
    "matte finish, no specular highlights.")


def dress():
    wf = {"_ids": {"unet": "3", "clip": "4", "vae": "5", "loras": ["6"], "aura": "7", "norm": "8"}}
    wf["1"] = node("LoadImage", "Picture 1 - mannequin", image="isekai_dress_mannequin.png")
    wf["2"] = node("LoadImage", "Picture 2 - design", image="isekai_dress_design.png")
    qwen_models(wf, [LIGHTNING])
    wf["9"] = node("FluxKontextImageScale", "Scale mannequin", image=["1", 0])
    wf["10"] = node("FluxKontextImageScale", "Scale design", image=["2", 0])
    wf["15"] = node("VAEEncode", "Latent from mannequin", pixels=["9", 0], vae=["5", 0])
    edit_sample(wf, ("11", "12", "13", "14", "16", "17", None, "18"),
                DRESS_PROMPT, ["9", "10"], "15", "isekai/dressed")
    return finish(wf)
```

And change the registry line to: `BUILDERS = {"concept": concept, "views": views, "dress": dress}`

- [ ] **Step 2: Build and check the ids against the spec**

```bash
python E:/_isekai_assets/_comfy/build_workflows.py dress tools/comfyui/dress.json
python - <<'EOF'
import json
w = json.load(open("tools/comfyui/dress.json"))
want = {"1": "LoadImage", "2": "LoadImage", "3": "UNETLoader", "4": "CLIPLoader", "5": "VAELoader",
        "6": "LoraLoaderModelOnly", "7": "ModelSamplingAuraFlow", "8": "CFGNorm",
        "9": "FluxKontextImageScale", "10": "FluxKontextImageScale",
        "11": "TextEncodeQwenImageEditPlus", "12": "FluxKontextMultiReferenceLatentMethod",
        "13": "TextEncodeQwenImageEditPlus", "14": "FluxKontextMultiReferenceLatentMethod",
        "15": "VAEEncode", "16": "KSampler", "17": "VAEDecode", "18": "SaveImage"}
got = {k: v["class_type"] for k, v in w.items()}
assert got == want, got
assert w["16"]["inputs"]["latent_image"] == ["15", 0]
assert w["17"]["inputs"]["samples"] == ["16", 0] and w["18"]["inputs"]["images"] == ["17", 0]
assert w["11"]["inputs"]["image2"] == ["10", 0] and w["13"]["inputs"]["image2"] == ["10", 0]
print("DRESS IDS OK")
EOF
```

Expected: `DRESS IDS OK`.

- [ ] **Step 3: Generate the armour concept**

```bash
J=E:/_isekai_assets/_comfy
A=E:/_isekai_assets/_smoketest/armour
python $J/comfy_job.py set tools/comfyui/concept.json $J/run-armour-concept.json \
  '2.text="full fantasy armour set on a plain mannequin, helmet, cuirass, gauntlets and boots, full figure in frame, standing, front view, orthographic view, centered, plain light grey background, soft diffuse studio lighting, matte finish, no specular highlights, heavy knight armour, dark steel plates with gold trim, layered pauldrons, a separate red cloth tabard hanging to the knees, plated gauntlets and sabatons, closed helmet"' \
  '3.text="nude, weapon, multiple figures, cropped, perspective, dramatic lighting, glare, reflections, specular highlights, scenery, text, watermark"' \
  '5.seed=3303'
PID=$(python $J/comfy_job.py submit $J/run-armour-concept.json)
python $J/comfy_job.py collect $PID $A 7=concept
```

Expected: `concept_1.png` … `concept_4.png` at `[1024, 1024]`. Read them; copy the best to `$A/concept.png`.

- [ ] **Step 4: Dress 3BA**

```bash
J=E:/_isekai_assets/_comfy
A=E:/_isekai_assets/_smoketest/armour
python $J/comfy_job.py stage E:/_isekai_assets/_bodies/3ba.png isekai_dress_mannequin.png
python $J/comfy_job.py stage $A/concept.png isekai_dress_design.png
python $J/comfy_job.py set tools/comfyui/dress.json $J/run-dress.json '16.seed=4404'
PID=$(python $J/comfy_job.py submit $J/run-dress.json)
python $J/comfy_job.py collect $PID $A/dressed 18=3ba
```

Expected: `3ba.png` at `[1024, 1024]`, seconds and RAM low printed.

- [ ] **Step 5: Dress HIMBO with the 3BA result as the design**

```bash
J=E:/_isekai_assets/_comfy
A=E:/_isekai_assets/_smoketest/armour
python $J/comfy_job.py stage E:/_isekai_assets/_bodies/himbo.png isekai_dress_mannequin.png
python $J/comfy_job.py stage $A/dressed/3ba.png isekai_dress_design.png
PID=$(python $J/comfy_job.py submit $J/run-dress.json)
python $J/comfy_job.py collect $PID $A/dressed 18=himbo
```

Expected: `himbo.png` at `[1024, 1024]`.

- [ ] **Step 6: Judge the dress runs**

Build an overlay of each result on its mannequin at half opacity and read it:

```bash
python -c "
from PIL import Image
for b in ('3ba', 'himbo'):
    m = Image.open('E:/_isekai_assets/_bodies/%s.png' % b).convert('RGB')
    d = Image.open('E:/_isekai_assets/_smoketest/armour/dressed/%s.png' % b).convert('RGB').resize(m.size)
    Image.blend(m, d, 0.5).save('E:/_isekai_assets/_smoketest/armour/dressed/%s_overlay.png' % b)
"
```

Pass: head, hands, feet and arms sit in the same place as the mannequin's; the armour follows the concept (3BA) and the 3BA armour (HIMBO); the tabard reads as a separate cloth piece; plain background. Record pass or fail per criterion.

- [ ] **Step 7: Views for both bodies**

```bash
J=E:/_isekai_assets/_comfy
A=E:/_isekai_assets/_smoketest/armour
for b in 3ba himbo; do
  python $J/comfy_job.py stage $A/dressed/$b.png isekai_views_input.png
  PID=$(python $J/comfy_job.py submit $J/run-views.json)
  python $J/comfy_job.py collect $PID $A/views/$b 17=main 27=left 37=back 47=right
done
```

Expected: eight PNGs at `[1536, 1536]`. Read them. Pass: the whole figure in frame in all four; `back` a real back, not a mirrored front; arms at the bind pose in all four; the same armour in all four. Record results, timings, RAM lows.

- [ ] **Step 8: Name check (Task 2, Step 7), then commit**

```bash
git add tools/comfyui/dress.json
git commit -m "feat(tools): ComfyUI dress workflow, armour onto the reference mannequin"
```

---

### Task 7: Documentation, memory, and the MCP after a restart

**Files:**
- Modify: `docs/WEAPON_ASSET_GUIDE.md` (route table line 25–37, tools table line 68–83, failures table line 46–59, stage A at `## A. Generate in Meshy`)
- Modify: `docs/superpowers/specs/2026-09-11-comfyui-concept-views-design.md` (status line under the title)
- Modify: `CHANGELOG.md` (`[Unreleased]` → Added)
- Local: memory file `comfyui-local-install.md` and its `MEMORY.md` line

**Interfaces:**
- Consumes: the recorded results, timings, RAM lows and any silent failures from Tasks 1–6.

- [ ] **Step 1: Route table** — insert above the `| A | Generate |` row:

```markdown
| 0 | Concept views | Claude, the user picks | ComfyUI: `tools/comfyui/concept.json`, `views.json` | four 1536² views for Meshy's slots |
```

- [ ] **Step 2: Stage 0 section** — insert above `## A. Generate in Meshy`:

````markdown
## 0. Concept views in ComfyUI

The design is generated locally, and one chosen image becomes the four views Meshy's
multi-image-to-3D takes. The decisions behind every value are in
`docs/superpowers/specs/2026-09-11-comfyui-concept-views-design.md`; this is the run.

1. **Start ComfyUI**: `H:\AI\ComfyUI_windows_portable\run_nvidia_gpu.bat`. Everything below
   reaches it at `127.0.0.1:8188`.
2. **Concept**: run `tools/comfyui/concept.json` with `1.ckpt_name` (the finetune), `2.text` —
   the weapon frame from the spec with the design appended — and `5.seed`. Four images per run;
   repeat with new seeds until one is right.
3. **Views**: put the chosen image into `1.image` of `tools/comfyui/views.json`, set `14.seed`,
   `24.seed`, `34.seed`, `44.seed` to one value, and run. Save nodes `17`, `27`, `37`, `47` are
   main, left, back and right.
4. **Archive** to `E:\_isekai_assets\<weapon>\`: `concept.png`, `views\main.png`, `left.png`,
   `back.png`, `right.png`. The weapon's name never goes into ComfyUI — outputs are fetched by
   job id and renamed on the way.

Over the MCP: `set_workflow_slot` writes the overrides, `run_workflow` runs the file, and
`fetch_outputs` collects the images by `prompt_id`. From a terminal: `comfy --json --where local
run --workflow <copy with overrides>`, then the images from `ComfyUI\output\isekai\`.

Measured on the smoke test (RTX 4090, 32 GB RAM): concept <seconds> s for four images, views
<seconds> s for four views, lowest free RAM during views <GB> GB.
````

Replace the three `<…>` values with the numbers recorded in Tasks 3 and 4 before saving — they are measurements, and the section must not be committed with the markers in it.

- [ ] **Step 3: Stage A's first sentence** — replace `Image-to-3D from the concept image.` with:

```markdown
Multi-image-to-3D from the four views of stage 0, in the slots Main, Left, Back and Right.
```

Then read the three settings that follow and confirm each still exists in Meshy's multi-image mode; where one does not, say so in that bullet.

- [ ] **Step 4: Tools table** — add these rows below the Higgsfield row:

```markdown
| ComfyUI **v0.35.0**, Windows portable, nvidia | `Comfy-Org/ComfyUI` releases, unpacked to `H:\AI\` — GPL-3.0 |
| Qwen-Image-Edit-2511 fp8mixed, Qwen 2.5 VL 7B fp8, Qwen VAE | `Comfy-Org` repackages on Hugging Face — Apache-2.0 |
| Lightning 4-steps LoRA, multiple-angles LoRA | `lightx2v`, `fal` on Hugging Face — Apache-2.0 |
| comfy-cli **1.20.0**, comfy-mcp **0.10.0** | two separate `uv tool install`s — GPL-3.0, AGPL-3.0; used locally, not distributed |
```

- [ ] **Step 5: Silent failures** — for every failure met in Tasks 1–6 that produced no error or a misleading success, add a row to the failures table in the same `| What | What you get | Where |` form, with `0` as its stage. If none occurred, add nothing.

- [ ] **Step 6: Spec status** — below the spec's title insert:

```markdown
**Status:** built 2026-09-11. Weapon chain smoke-tested; armour chain smoke-tested on 3BA and
HIMBO. The Meshy runs — one weapon, one armour with the three Auto Split questions — are the
user's and still open.
```

Adjust the two "smoke-tested" clauses to what Tasks 4 and 6 actually recorded (for example "views passed except the edge views").

- [ ] **Step 7: CHANGELOG** — under `[Unreleased]` → `### Added`:

```markdown
- `tools/comfyui/` concept, dress and views workflows and `tools/render-mannequin.py`: local
  concept views for Meshy's multi-image-to-3D, armour dressed on 3BA and HIMBO mannequins.
```

- [ ] **Step 8: Name check (Task 2, Step 7), `node tools/check.mjs`, then commit**

```bash
node tools/check.mjs
git add docs/WEAPON_ASSET_GUIDE.md docs/superpowers/specs/2026-09-11-comfyui-concept-views-design.md CHANGELOG.md docs/superpowers/plans/2026-09-11-comfyui-concept-views.md
git commit -m "docs: stage 0, concept views in ComfyUI, and the smoke-test results"
```

Expected from `check.mjs`: all checks pass.

- [ ] **Step 9: Memory** — write `comfyui-local-install.md` in the project memory folder (type `project`): ComfyUI v0.35.0 portable at `H:\AI\ComfyUI_windows_portable`, models there, `J:` linked read-only, comfy-cli and comfy-mcp as uv tools, MCP scope local, mannequins at `E:\_isekai_assets\_bodies\`, HIMBO body taken from Kreiste's Samurai conversion until HIMBO is installed; link `[[weapon-asset-pipeline]]`. Add its line to `MEMORY.md`.

- [ ] **Step 10: The MCP, after a restart** — tell the user (German) that Claude Code must be restarted for comfy-mcp's tools to appear. After the restart, with ComfyUI running: call `server_info`, then `set_workflow_slot` on a copy of `tools/comfyui/views.json` outside the repository setting `14.seed`, then `run_workflow` with `wait=False` and `fetch_outputs` for its `prompt_id`. If a tool name or argument differs from Step 2's paragraph, correct that paragraph and commit `docs: stage 0 MCP tool names as installed`.
