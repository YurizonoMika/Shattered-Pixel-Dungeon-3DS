# Contributing

Thanks for helping port Shattered Pixel Dungeon to the 3DS.

## Before you start

- Read `README.md` for the architecture and current scope.
- Keep changes consistent with the existing C++17 / citro2d style: no exceptions,
  no RTTI, small per-frame allocations.
- Game logic should stay independent from rendering where practical. Rendering code
  lives in `game_screen.cpp` / `assets.cpp`; rules live in `game.cpp`.

## Building and testing

```bash
bash tools/prepare-assets.sh
bash tools/fetch-fonts.sh
python3 -m venv tools/venv && tools/venv/bin/pip install pillow fonttools
tools/venv/bin/python tools/gen_font.py
make
```

Load the resulting `spd3ds.3dsx` in Azahar or on hardware. Use the `scene.txt` debug
entry (see README) to jump directly to the screen you are working on.

## Pull requests

- Describe what changed and which screen/system it affects.
- Include a screenshot or a short recording for UI changes.
- If you add new Chinese UI strings, rerun `tools/gen_font.py` and commit the
  regenerated `gfx/font.png` and `source/font_glyphs.h`.
- Do not commit ROMs, commercial assets, or anything you do not have the right to
  redistribute.

## Release notes / 发布日志

Write GitHub release notes in Simplified Chinese and English. Keep the published
text in `docs/releases/<tag>.md` and update both `README.md` and `README.zh-CN.md`
when downloads or compatibility change.

GitHub 发布日志须有简体中文和英文两个版本，正文保存在
`docs/releases/<tag>.md`。下载地址或兼容性有变动时，同步更新两份 README。

## License

By contributing you agree that your changes are licensed under GPL-3.0.
