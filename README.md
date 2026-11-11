# SHEFF FEA I/O (`sheff-fea-io`)

`sheff-fea-io` is a header-only C++ library in the **Scalable Hierarchical Enrichment Finite-element Framework (SHEFF)** for reading and writing partitioned finite element meshes, element/node groups, field data, local coordinate systems, and time-series metadata in a hybrid JSON + binary file format, supporting both serial and MPI-distributed parallel I/O as well as ParaView visualization.

## Capabilities

- **Mesh & Partition I/O (`fea_io/mesh/`)**:
  - Serial and MPI-parallel mesh writers and readers (`MeshWriter`, `MeshParWriter`, `MeshReader`, `MeshStructs.h`, `IteratorAdapters.h`) storing JSON descriptors (`.m.json`) paired with compact binary connectivity/coordinate buffers (`.m.bin`).
- **Field Data & Group I/O (`fea_io/data/`, `fea_io/group/`)**:
  - Node-, element-, component-, and quadrature-level field storage (`DataWriter`, `DataReader`, `DataStructs.h` -> `.d.json` / `.d.bin`).
  - Element and node group serialization (`GroupWriter`, `GroupParWriter`, `GroupReader`, `GroupStructs.h` -> `.g.json` / `.g.bin`).
- **Simulation Metadata (`fea_io/meta/`)**:
  - Time-step and multi-partition manifest management (`MetaWriter`, `MetaReader`, `MetaStructs.h` -> `.x.json`) linking meshes, groups, and field snapshots across steps.
- **ParaView Reader Plugin (`paraview_plugin/`)**:
  - Optional VTK/ParaView plugin (`vtkFEAIOReader`, enabled via `--make-paraview-reader` / `-DSHEFF_BuildParaViewReader=ON`) for direct interactive visualization of `.x.json` FEA datasets.

## Project Structure

- `include/fea_io/`: Header-only library organized into `common/`, `mesh/`, `data/`, `group/`, and `meta/`.
- `paraview_plugin/`: Optional ParaView reader plugin source and CMake configuration.
- `unit_tests/`: Unit tests for serial and MPI-parallel mesh, group, data, and metadata I/O.
- **CMake Package / Target**: `FeaIo` / `FeaIo::fea_io` (header-only `INTERFACE` library).

## Dependencies

- **Build System**: [`sheff-build-tools`](https://github.com/afrl/sheff-build-tools) (`SHEFF_TOOLS`), CMake 3.17+, C++17/C++20 compiler.
- **Third-Party Libraries**: [`nlohmann/json`](https://github.com/nlohmann/json) (`nlohmann_json::nlohmann_json`).
- **Optional External Libraries**:
  - MPI (`--use-mpi` / `-DSHEFF_USE_MPI=ON`, required for parallel `ParWriter` classes).
  - ParaView built from source (`--make-paraview-reader` / `-DSHEFF_BuildParaViewReader=ON`).

## Building

Because `sheff-fea-io` is header-only, configuring and installing exports the `FeaIo::fea_io` CMake package; unit tests can be built and executed via `make.py`:

```bash
python3 make.py --config --build --third-party
python3 make.py --config --build --use-mpi --make-tests --run-tests
```

## Public Release Information

Distribution Statement A. Approved for public release: distribution is unlimited. Case #: AFRL-2026-4513
