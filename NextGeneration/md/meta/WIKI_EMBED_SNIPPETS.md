# Wiki Embed Snippets

## Relative paths

| Source file | Image prefix |
|-------------|--------------|
| `md/wiki/*.md` | `../../assets/…` |
| `README.md` (root) | `assets/…` |

## Heroes (Home — Issues 28–30)

```html
<p align="center">
  <img src="../../assets/heroes/main_hero_amouranthrtx.png" width="32%" />
  <img src="../../assets/heroes/main_hero_field_die.png" width="32%" />
  <img src="../../assets/heroes/main_hero_friends.png" width="32%" />
</p>
```

## Triptych template

Replace `{group}` with `stack`, `physics`, or `start` and `{page}` with topic folder:

```html
<p align="center">
  <img src="../../assets/wiki/{group}/{page}/01_....png" width="32%" />
  <img src="../../assets/wiki/{group}/{page}/02_....png" width="32%" />
  <img src="../../assets/wiki/{group}/{page}/03_....png" width="32%" />
</p>
```

## GitHub raw (example — Field Die)

```html
<img src="https://raw.githubusercontent.com/ZacharyGeurts/NextGeneration/main/assets/wiki/stack/field_die/01_amouranth.png" width="32%" />
```

Cast map: [PAGE_CASTS.md](PAGE_CASTS.md) · Structure: [STRUCTURE.md](../../STRUCTURE.md)