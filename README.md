# xmbsort

PSP homebrew that puts the games and `CAT_` category folders on the XMB in the order you choose.

The XMB lists games newest first, by file date. xmbsort reads an ordered list from `config.txt` and rewrites the dates of your games and category folders to match it.

## How it works

- It scans `ISO/` and `PSP/GAME/` on the device it is started from (`ms0:` memory stick or `ef0:` internal storage).
- Every `CAT_<Name>` folder is a category. Folders with the same name under `ISO/` and `PSP/GAME/` are treated as one category; names are case-insensitive.
- Everything outside a `CAT_` folder belongs to the uncategorized group, written `*` in the config.
- Items are stamped with descending timestamps starting at 2035-01-01, 2 seconds apart (the FAT32 resolution). Within each category the items are stamped first, then the category folders themselves.

> **Note:** xmbsort overwrites the modification and creation dates of every game file, game folder and `CAT_` folder it finds. The original dates are not kept.

## Install

Requires custom firmware.

1. Copy `EBOOT.PBP` and `config.txt` to `PSP/GAME/xmbsort/` on the PSP. The folder may also sit inside a category, e.g. `PSP/GAME/CAT_Tools/xmbsort/`.
2. Start xmbsort from the Game menu.

`config.txt` must be in the same folder as `EBOOT.PBP`.

## Usage

Run xmbsort once after editing `config.txt`, and again whenever you add games.

If `config.txt` is missing, xmbsort writes one that lists the categories it found and changes nothing else. Edit it and run again.

On each run it prints the item count per category and a final `N timestamps set, M failed` line, then exits after 3 seconds. If there were warnings it waits for a button press instead.

## config.txt

```
# Lines starting with # are comments.

PSP:
Lumines
Wipeout Pure.iso

PS1:

Emulators:

*:
```

- `Name:` starts the section for the `CAT_Name` folders. Writing `CAT_Name:` works too.
- `*:` is the section for uncategorized items.
- The order of the sections is the order of the categories on the XMB.
- The lines under a section are the items of that category, in the order they should appear.
- Item names are matched case-insensitively and the `.iso`, `.cso`, `.zso` or `.pbp` extension is optional. For entries in `PSP/GAME/`, use the folder name.
- Items you do not list come after the listed ones, sorted A–Z. A section with no lines is sorted entirely A–Z.
- Categories that exist on the device but are missing from the config go last, sorted A–Z, with a warning.

## Warnings

| Message | Meaning |
| --- | --- |
| `no CAT_ folder` | The section names a category that has no folder on the device. |
| `<name>: already used above` | The category appears twice in the config; the second section is ignored. |
| `not found: <item>` | A listed item is not in that category. |
| `not in config.txt` | The category exists on the device but has no section. |
| `line N: missing category name` | A line holds only `:`. |
| `line N: item before any category` | An item line comes before the first section. |
| `cannot open <dir>` | A category folder could not be read. |
| `chstat <code>: <path>` | Setting the date of that file or folder failed. |
| `cannot write <path>` | The default `config.txt` could not be created. |

## Building

The build runs inside the [`pspdev/pspdev`](https://hub.docker.com/r/pspdev/pspdev) Docker image, so Docker is the only requirement.

```sh
make
```

This produces `EBOOT.PBP` in the repository root.

| Target | Action |
| --- | --- |
| `make` | Build `PARAM.SFO` and `EBOOT.PBP` |
| `make eboot` | Build only `EBOOT.PBP` |
| `make sfo` | Regenerate `source/xmb/PARAM.SFO` |
| `make clean` | Remove build artifacts and the root `EBOOT.PBP` |
| `make shell` | Open a shell in the toolchain container |
| `make pull` | Pull or update the toolchain image |

`IMAGE`, `PLATFORM` and `DOCKER` can be overridden, for example `make DOCKER=podman`.

The source is in `source/src/`; the XMB icon is in `source/xmb/`.

## License

GPLv3. See [LICENSE](LICENSE).
