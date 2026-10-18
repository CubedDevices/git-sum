# Linux / NetBeans builds

NetBeans stores project metadata under `nbproject/` in the project directory.
git-sum is a single application. Sources stay at the repository root.

| Project | Path | Output |
|---------|------|--------|
| **git-sum** | `build/Linux/git-sum/` | `dist/…/git-sum` |

## Command line

```bash
cd build/Linux/git-sum && make CONF=Debug
```

Or from `build/Linux`:

```bash
make
make CONF=Release
```

`wx-config` and `pkg-config` must be on `PATH`. The project links wxWidgets and libgit2 (`pkg-config --cflags/--libs libgit2`).

## NetBeans

**File → Open Project** and select `build/Linux/git-sum`.
