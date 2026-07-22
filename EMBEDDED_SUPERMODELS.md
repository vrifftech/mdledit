# Embedded stock supermodel metadata

MDLedit now contains a metadata-only fallback for the five stock humanoid supermodels in each game:

```text
S_Male01
S_Male02
S_Female01
S_Female02
S_Female03
```

## Lookup precedence

During ASCII-to-binary compilation MDLedit now uses:

1. A valid destination-game binary `<supermodel>.mdl` beside the ASCII model.
2. The embedded stock metadata for the selected destination game.
3. The existing warning/no-supermodel behavior for unknown or custom supermodels.

A neighboring binary remains authoritative so custom or modified supermodels continue to work. The embedded fallback contains only node names, parent relationships, node types, supernode numbers, and total-node counts. It contains no geometry, controller, animation, texture, or MDX data.

## Embedded sets

| Game | Supermodel | Inherits | Local nodes | Total nodes with chain | Maximum local supernode |
|---|---|---:|---:|---:|---:|
| K1 | `S_Male01` | `NULL` | 82 | 82 | 81 |
| K1 | `S_Male02` | `S_Male01` | 81 | 164 | 85 |
| K1 | `S_Female01` | `S_Male02` | 83 | 248 | 87 |
| K1 | `S_Female02` | `S_Female01` | 83 | 332 | 87 |
| K1 | `S_Female03` | `S_Female02` | 83 | 416 | 88 |
| K2 | `S_Male01` | `NULL` | 102 | 102 | 101 |
| K2 | `S_Male02` | `S_Male01` | 109 | 212 | 133 |
| K2 | `S_Female01` | `S_Male02` | 106 | 319 | 260 |
| K2 | `S_Female02` | `S_Female01` | 103 | 423 | 344 |
| K2 | `S_Female03` | `S_Female02` | 101 | 525 | 345 |

## Reusable tables

The `metadata` directory contains:

- `supermodel_summary.csv`: one row per game/supermodel.
- `supermodel_nodes.csv`: every hierarchy node and supernode mapping.
- `supermodel_k1_k2_comparison.csv`: path-based K1/K2 number comparison.
- `supermodel_nodes.json`: nested machine-readable metadata.

Use `node_path` or `relative_path` rather than `node_name` as the unique key. K2 `S_Male02` contains two nodes named `w_Longsword`, one under each hand.

