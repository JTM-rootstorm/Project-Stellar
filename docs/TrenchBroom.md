# TrenchBroom Workflow

Branch target: `trenchbroom-compat`

This document is a work-in-progress placeholder for the TrenchBroom BSP30 workflow. The final setup,
FGD, compile wrapper, fixture, and launch instructions are not stable until Phase 7 of the
TrenchBroom compatibility plan is complete.

Current intended end state:

- TrenchBroom-authored BSP30 maps are the primary editor workflow.
- Runtime and authoring coordinates are Z-up.
- 1 Stellar gameplay unit equals 1 authored inch.
- The default 72 inch player capsule center is authored at `origin = "0 0 36"` when the floor is at
  `z = 0`.
