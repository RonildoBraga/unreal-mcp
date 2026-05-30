# Example: Megascans Kit Inventory

Sample workflow showing the **asset-registry tools in Sprint 1** doing the
job that originally drove their creation: producing a categorized
inventory of a freshly-downloaded UE sample project's mesh/material/texture
content, without manually browsing the Content Browser.

This is the actual workflow used in the Lauder project on 2026-05-30 to
inventory the **Goddess Temple Megascans Sample** (a Quixel showcase project)
in preparation for migrating selected assets into the active game project.
The full run took **~4 MCP calls and ~3 seconds of actual query time**, vs.
the hours of manual Content Browser browsing that would otherwise be needed.

## Setup

1. UE editor open on the project you want to inventory (the sample, in this
   case — `D:/Developer/Fab/GoddessTempleMegascansSam/`).
2. Plugin installed in that project's `Plugins/UnrealMCP/`.
3. MCP client (Claude) connected to the Python server.

## Prompt to the assistant

> Run the kit inventory: how many StaticMesh assets are in this project,
> categorized by top-level folder, and what's the same for Material and
> Texture2D?

## Expected MCP calls

The assistant runs these in roughly this order:

```
find_assets_by_class(class_name="StaticMesh", search_path="/Game", recursive=True)
find_assets_by_class(class_name="Material",   search_path="/Game", recursive=True)
find_assets_by_class(class_name="Texture2D",  search_path="/Game", recursive=True)
find_assets_by_class(class_name="NiagaraSystem", search_path="/Game", recursive=True)
```

Each returns a JSON shape like:

```json
{
  "assets": [
    {
      "name": "<asset name>",
      "package_path": "/Game/.../folder",
      "package_name": "/Game/.../folder/asset_name",
      "object_path": "/Game/.../folder/asset_name.asset_name",
      "class_path": "/Script/Engine.StaticMesh",
      "class_name": "StaticMesh"
    },
    ...
  ],
  "count": 265
}
```

## Categorization

Group the `package_path` by top-level folder (e.g. `/Game/Megascans/3D_Plants`
becomes the "Megascans / 3D_Plants" bucket). The assistant can do this with
`jq` or Python.

## Example output for Goddess Temple

```
StaticMesh: 265 total
  /Game/Megascans/3D_Plants           121   (vines, moss, leaves)
  /Game/Megascans/3D_Assets           119   (temple stones, columns)
  /Game/CustomAssets/Quarry            10   (custom stone props)
  /Game/CustomAssets/CommentaryBox      4   (info-panel UI; skip)
  /Game/CustomAssets/Candles            3   (hero props — high priority!)
  ...

Material: 24 total
  /Game/Masters/01_Masters             10   (M_BlendMaster, M_SSSMaster — key)
  /Game/Masters/01_Masters/FX           6   (candle, fog, embers)
  /Game/CustomAssets/...                5   (UI; skip)
  ...

Texture2D: 483 total
  /Game/Megascans/3D_Assets           228   (PBR sets for the meshes)
  /Game/Megascans/3D_Plants            79
  /Game/Megascans/Atlases              64
  /Game/Megascans/Surfaces             20   (RVT blend source layers — key)
  ...

NiagaraSystem: 0
  (sample uses material+mesh planes for effects — UE4-era technique)
```

## What this enables

The categorization directly produces a **migration shopping list**: which
folders to migrate wholesale, which to cherry-pick from, which to skip. The
manual alternative is opening each folder in the Content Browser, scrolling
through assets, and making mental tallies — slow and error-prone for a
500-asset project.

See the Lauder project's `project_lauder_v0_scope.md` Phase 7.2 entry for
how this inventory fed into the actual asset migration plan.
