# Generated BSP30 fixture outputs

Compiled BSP files are not checked in. Default CTest generates deterministic BSP30 fixtures in:

```text
build/tests/fixtures/trenchbroom/compiled/
```

Use `tools/bsp/compile_trenchbroom_bsp30.sh` with the sibling `src/*.map` files to populate this directory manually when an external BSP30 compiler is available.
