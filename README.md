# Planar Terrains

Planar Terrains is an experimental Unreal Engine terrain-rendering project focused on GPU-driven, tiled terrain rendering and GDAL-based terrain preprocessing.

The project is built around a custom UDLOD-style terrain plugin that loads preprocessed terrain tiles, manages runtime terrain attachments such as height and albedo data, and renders terrain through custom Unreal rendering code, shader stages, and a terrain mesh vertex factory. Many of the ideas behind the algorithm design—i.e., the shader code—as well as the general code structure have been adapted from the official [bevy_terrain](https://github.com/kurtkuehnert/bevy_terrain) including my own [unofficial fork](https://github.com/ezappala/bevy_terrain).

> This is an active research/prototype project. APIs, asset formats, shader interfaces, and setup steps may change as the renderer evolves.

## What this project does

Planar Terrains explores a terrain pipeline where large source datasets are converted into tiled runtime data and rendered through a custom Unreal Engine terrain component.

At a high level, the project includes:

- A custom `UTerrain` mesh component.
- Runtime terrain config and settings structures.
- Tile atlas and attachment management.
- Custom scene proxy and scene view extension rendering code.
- Custom shader stages for terrain preparation, tile refinement, vertex generation, fragment shading, depth copy, picking, and debug/probe work.
- GDAL-backed preprocessing for heightmaps and optional albedo textures.
- Helper extensions for math, frustum logic, buffers, arrays, and GDAL data typing.

The goal is to make large terrain datasets usable inside Unreal while keeping the runtime renderer flexible enough for GPU-oriented LOD, tile refinement, debug visualization, and future terrain rendering research.

## Repository layout

```text
.
├── Config/
├── Content/
├── Plugins/
│   ├── UDLODTerrain/
│   │   ├── Shaders/
│   │   └── Source/
│   │       ├── DebugViewer/
│   │       └── UDLODTerrain/
│   ├── UDLODPreprocessor/
│   │   ├── Scripts/
│   │   └── Source/
│   │       ├── Tests/
│   │       └── UDLODPreprocessor/
│   ├── UDLODExt/
│   ├── UnrealGDAL/
│   └── SubsystemBrowserPlugin/
├── Scripts/
├── Source/
│   └── PlanarTerrains/
└── PlanarTerrains.uproject
```

The main project module lives in `Source/PlanarTerrains`.

The terrain runtime lives primarily in `Plugins/UDLODTerrain`.

The preprocessing pipeline lives in `Plugins/UDLODPreprocessor`.

Shared helper types and utilities live in `Plugins/UDLODExt`.

## Main modules

### `PlanarTerrains`

The root Unreal project module. It wires the project against the terrain, preprocessing, GDAL, rendering, mesh, material, JSON, and editor/debug dependencies used by the prototype.

### `UDLODTerrain`

The main runtime terrain plugin.

Important areas include:

- `terrain.h` / `terrain.cpp`
  The custom terrain mesh component.

- `terrain_config.h`
  Runtime terrain metadata loaded from generated terrain config files.

- `terrain_settings.h`
  Runtime terrain settings and preprocessing settings exposed to Unreal.

- `terrain_scene_proxy.h` / `terrain_scene_proxy.cpp`
  Unreal render-thread bridge for the terrain component.

- `terrain_scene_view_extension.h` / `terrain_scene_view_extension.cpp`
  View-extension based rendering and GPU dispatch integration.

- `terrain_mesh_vertex_factory.h` / `terrain_mesh_vertex_factory.cpp`
  Custom terrain vertex factory integration.

- `terrain_tile_atlas.h`, `gpu_terrain_tile_atlas.h`, and attachment-related headers
  Runtime texture/attachment atlas management.

- `terrain_shaders.h` / `terrain_shaders.cpp`
  Shader registration and binding code.

### `UDLODPreprocessor`

The preprocessing plugin responsible for converting source geospatial data into runtime terrain data.

It supports terrain input settings such as:

- Heightmap source path.
- Optional albedo source path.
- Output terrain path.
- Temporary output path.
- NoData handling.
- GDAL data type overrides.
- Fill radius.
- Attachment labels.
- Texture size.
- Border size.
- Mip count.
- Attachment formats.

The default preprocessing settings expect tiled terrain output with a height attachment and optional albedo attachment.

### `UDLODExt`

Shared extension utilities used by the terrain and preprocessing plugins.

This includes support code for affine transforms, fixed-size arrays, buffer helpers, frustum utilities, and GDAL data type helpers.

## Shader pipeline

`UDLODTerrain/Shaders` contains the custom shader code used by the runtime renderer.

Notable shader files include:

```text
attachments.ush
bindings.ush
common.ush
depth_copy.usf
fragment.usf
functions.ush
math.ush
picking.usf
prepare_prepass.usf
probe_samples.usf
refine_tiles.usf
terrain_mesh_vertex_factory.ush
vertex.usf
```

These files define the shared terrain shader bindings, math helpers, vertex factory behavior, prepass/refinement passes, vertex and fragment stages, picking support, and related utility passes.

## Requirements

This project currently targets:

- Unreal Engine 5.7
- A C++23-capable toolchain
- GDAL / UnrealGDAL support
- Unreal Engine rendering modules such as `RenderCore`, `Renderer`, `RHI`, and `RHICore`
- Unreal plugins used by the project, including:
  - `UDLODTerrain`
  - `UDLODPreprocessor`
  - `UDLODExt`
  - `UnrealGDAL`
  - `GeometryProcessing`
  - `CustomMeshComponent`
  - `ModelingToolsEditorMode`
  - `SubsystemBrowserPlugin`

On Windows, use a Visual Studio toolchain compatible with your Unreal Engine 5.7 installation.

## Getting started

Clone the repository:

```bash
git clone --recurse-submodules https://github.com/ezappala/planar-terrains.git
cd planar-terrains
```

If you already cloned without submodules, initialize them with:

```bash
git submodule update --init --recursive
```

Open the project in Unreal Engine:

```text
PlanarTerrains.uproject
```

When prompted, let Unreal rebuild missing binaries.

## Building from the command line

Example build command:

```bat
"C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\Build.bat" PlanarTerrainsEditor Win64 Development -Project="C:\path\to\PlanarTerrains\PlanarTerrains.uproject"
```

## Preprocessing terrain data

Terrain data is expected to be preprocessed before runtime rendering.

The preprocessing settings include:

- `heightmap_src_path`
- `albedo_src_path`
- `terrain_path`
- `temp_path`
- `heightmap_no_data`
- `albedo_no_data`
- `heightmap_data_type`
- `albedo_data_type`
- `fill_radius`
- `heightmap_create_mask`
- `albedo_create_mask`
- `heightmap_lod_count`
- `albedo_lod_count`
- `texture_size`
- `border_size`
- `mip_level_count`
- `heightmap_attachment_format`
- `albedo_attachment_format`

By default, the project expects a height attachment labeled `height` and an albedo attachment labeled `albedo`.

Generated terrain configs are loaded through `FTerrainConfig`, which stores terrain metadata such as:

- Terrain path.
- LOD count.
- Minimum and maximum height.
- Attachment configuration.
- Tile coordinates.
- Side length.
- Face count.

## Runtime terrain usage

The runtime terrain object is exposed as a custom Unreal mesh component:

```cpp
UTerrain
```

`UTerrain` owns the terrain config, terrain settings, material reference, atlas state, and render resources. It creates a custom scene proxy and participates in Unreal's primitive rendering flow.

The intended flow is:

1. Preprocess source terrain data into a terrain output directory.
2. Load the generated terrain config.
3. Create or configure a `UTerrain` component.
4. Assign terrain settings and material.
5. Let the terrain renderer manage tile data, shader dispatches, and draw integration.

## Development notes

This project uses strict modern C++ conventions in its module rules:

- C++23
- Explicit or shared PCHs
- Latest Unreal include order
- IWYU support enabled

The renderer is still in active development. Expect shader bindings, render resource layouts, and terrain data formats to change.

## Troubleshooting

### Unreal asks to rebuild modules

This is expected after a fresh clone or after switching Unreal Engine versions. Let Unreal rebuild the project, or build manually with Unreal Build Tool.

### Build fails because a plugin is missing

Make sure all project plugins and submodules are present. Run:

```bash
git submodule update --init --recursive
```

Also confirm that GDAL and UnrealGDAL are available to the project.

### Build paths are wrong

The provided build script contains local machine paths. Edit `Scripts/run_build_tool.bat` for your Unreal Engine install and local checkout path.

### Terrain renders flat or invisible

Check that:

- The terrain config was generated successfully.
- The height attachment exists and is listed in the config.
- The runtime terrain settings include the expected attachment labels.
- The terrain material is assigned.
- The shader bindings match the expected terrain attachment layout.
- The tile atlas size is large enough for the active terrain dataset.
- Render resources are being initialized after the terrain config and settings are assigned.

## Current status

Planar Terrains is currently a prototype/research codebase rather than a packaged plugin.

Use it as:

- A terrain rendering experiment.
- A reference for Unreal custom rendering integration.
- A sandbox for UDLOD-style terrain work.
- A base for GPU-driven terrain preprocessing/rendering research.

It is not yet packaged as a stable marketplace-ready Unreal plugin.

## License

This repository is licensed under the MIT license. Refer to the LICENSE file for details.

## Author

Created by [Eloise Zappala](https://github.com/ezappala).
