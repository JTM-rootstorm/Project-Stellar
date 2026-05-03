# Phase 4 — BSP30 Import and Validation Lockdown

Branch target: `trenchbroom-compat`

## Goal

Lock the map compatibility target to BSP30 for the TrenchBroom workflow while preserving existing importer robustness and display-free validation.

## Policy

- Active TrenchBroom authoring target: BSP30.
- Existing BSP29 support may remain as a legacy importer path unless it blocks BSP30 work.
- New TrenchBroom fixtures, docs, and compile wrappers must produce and validate BSP30.
- Source/VBSP remains unsupported.

## Tasks

### 4.1 Audit current BSP version handling

Search:

```bash
git grep -n 'BSP29\|BSP30\|version == 29\|version == 30\|bsp version\|BspVersion' -- include src tests docs tools ':!build*/**'
```

Document:

- where BSP version is parsed;
- how unsupported versions fail;
- whether tests currently generate BSP29, BSP30, or both;
- whether any code path treats BSP29 as default.

### 4.2 Define BSP30 fixture policy

New fixtures under this branch should be BSP30. Recommended layout:

```text
tests/fixtures/bsp30/
  minimal_zup_room.bsp
  entity_matrix_zup.bsp
  scripted_interaction_zup.bsp
```

If binary fixture size is a concern, keep small deterministic generated fixtures and add `.map` sources separately under `tests/fixtures/trenchbroom/`.

### 4.3 Add or update version-specific tests

Required tests:

- valid BSP30 minimal room imports;
- BSP30 entity lump preserves dotted keys and alias keys;
- BSP30 texture names trigger deterministic developer material fallback;
- invalid unsupported version fails with actionable diagnostic;
- Source/VBSP-like version does not get treated as BSP30;
- BSP30 with malformed lumps still fails safely.

If BSP29 support remains:

- keep a small legacy BSP29 import test;
- make clear it is not the TrenchBroom target.

### 4.4 Ensure validation commands report target clearly

Update diagnostics/docs so `stellar-client --validate-map` and `stellar-server --validate-config --map` state BSP30 support clearly.

Do not make every BSP29 map fail unless the branch explicitly decides to remove BSP29 support. The safer branch policy is:

- importer can read BSP29/BSP30 if already implemented;
- TrenchBroom compatibility workflow targets BSP30;
- compile wrapper rejects non-BSP30 outputs.

### 4.5 Z-up + BSP30 interaction checks

Confirm importer does not do hidden Y/Z swapping.

Required assertions:

- entity `origin = "0 0 36"` imports as `{0, 0, 36}`;
- floor is at `z = 0`;
- ceiling is at `z = 96`;
- collision triangles preserve Z-up normals;
- player starts and collision are consistent.

## Files likely changed

- `src/assets/bsp/**`
- `include/stellar/assets/**`
- `tests/assets/**`
- `tests/fixtures/**`
- `docs/BspAuthoring.md`
- `docs/Design.md`

## Validation

Focused:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)
ctest --test-dir build -R '^(bsp_validation|bsp_importer|bsp_authoring_smoke|client_map_validation_smoke|client_cli_map_validation|dedicated_server)' --output-on-failure
```

Broad:

```bash
ctest --test-dir build --output-on-failure
```

Audit:

```bash
git grep -n -i 'BSP29/BSP30-style\|BSP29-style\|classic BSP29' -- docs tools Plans ':!Plans/Archived/**' ':!build*/**'
```

Active docs should say BSP30 for TrenchBroom workflow.

## Acceptance criteria

- [ ] BSP30 minimal Z-up fixture imports.
- [ ] BSP30 validation runs display-free.
- [ ] TrenchBroom workflow docs target BSP30.
- [ ] Compile-wrapper requirements say non-BSP30 output is a failure.
- [ ] Source/VBSP remains unsupported.
- [ ] No hidden axis conversion exists in importer.

## Parallelization

Safe workstreams:

- Agent A: version handling and tests.
- Agent B: generated BSP30 fixtures.
- Agent C: validation diagnostics/docs.
- Agent D: negative malformed/unsupported tests.

Do not edit the same fixture generator concurrently.
