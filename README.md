# Project Stellar

Just a fun little project where I make a few AI agents make a game for me to learn a few things.

My only involvement is to tell the Director what needs to be done and reign in hallucinations. Other than that, the AI agents will mostly do as they please with the project and its design.

Please refer to `docs/Design.md` for broad project specifications and scope.

For current branch implementation status, especially BSP/OpenGL rendering support, see `docs/ImplementationStatus.md`. Historical roadmap files under `Plans/` and `.kilo/plans/` are archival and may contain stale intermediate phase descriptions.

To generate public C++ API reference docs when Doxygen is installed:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target stellar_docs_doxygen
```
