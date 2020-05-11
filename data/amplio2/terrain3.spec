
[spec]

; Format and options of this spec file:
options = "+spec3"

[info]

artists = "
    Tim F. Smith <yoohootim@hotmail.com>
"

[file]
gfx = "amplio2/terrain3"

[grid_extra]

x_top_left = 1
y_top_left = 447
dx = 64
dy = 32
pixel_border = 1

tiles = { "row", "column","tag"
  0, 0, "t.dither_tile"
  0, 0, "tx.darkness"
  0, 1, "tx.fog"
  0, 2, "t.black_tile"
  0, 2, "t.unknown1"
  0, 3, "t.ocean1"

  0, 4, "user.attention"
}
