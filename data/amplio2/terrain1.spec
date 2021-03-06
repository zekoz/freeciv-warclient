
[spec]

; Format and options of this spec file:
options = "+spec3"

[info]

artists = "
    Hogne Håskjold
    Daniel Speyer
    Yautja
    CapTVK
    GriffonSpade
    Gyubal Wahazar
"

[file]
gfx = "amplio2/terrain1"

[grid_main]

x_top_left = 1
y_top_left = 1
dx = 96
dy = 48
pixel_border = 1

tiles = { "row", "column", "tag"

; terrain
 0,  0, "t.desert1"

 1,  0, "t.plains1"

 2,  0, "t.grassland1"

 3,  0, "t.forest1"

 4,  0, "t.hills1"

 5,  0, "t.mountains1"

 6,  0, "t.tundra1"

 7,  0, "t.arctic1"
;7,  0, "t.l1.arctic1" not redrawn
;7,  0, "t.l2.arctic1" not redrawn

 8,  0, "t.swamp1"

 9,  0, "t.jungle1"
10,  0, "t.inaccessible1"

; Terrain special resources:

 0,  2, "ts.oasis"
 0,  4, "ts.oil"

 1,  2, "ts.buffalo"
 1,  4, "ts.wheat"

 2,  2, "ts.pheasant"
 2,  4, "ts.silk"

 3,  2, "ts.coal"
 3,  4, "ts.wine"

 4,  2, "ts.gold"
 4,  4, "ts.iron"

 5,  2, "ts.tundra_game"
 5,  4, "ts.furs"

 6,  2, "ts.arctic_ivory"
 6,  4, "ts.arctic_oil"

 7,  2, "ts.peat"
 7,  4, "ts.spice"

 8,  2, "ts.gems"
 8,  4, "ts.fruit"

 9,  2, "ts.fish"
 9,  4, "ts.whales"

 10, 2, "ts.seals"
 10, 4, "ts.forest_game"

 11, 2, "ts.horses"
 11, 4, "ts.grassland_resources", "ts.river_resources"

;roads
 12, 0, "r.road_isolated"
 12, 1, "r.road_n"
 12, 2, "r.road_ne"
 12, 3, "r.road_e"
 12, 4, "r.road_se"
 12, 5, "r.road_s"
 12, 6, "r.road_sw"
 12, 7, "r.road_w"
 12, 8, "r.road_nw"

;rails
 13, 0, "r.rail_isolated"
 13, 1, "r.rail_n"
 13, 2, "r.rail_ne"
 13, 3, "r.rail_e"
 13, 4, "r.rail_se"
 13, 5, "r.rail_s"
 13, 6, "r.rail_sw"
 13, 7, "r.rail_w"
 13, 8, "r.rail_nw"

;add-ons
 0,  6, "tx.oil_mine"
 1,  6, "tx.irrigation"
 2,  6, "tx.farmland"
 3,  6, "tx.mine"
 4,  6, "tx.pollution"
 5,  6, "tx.village"
 6,  6, "tx.fallout"
 7,  6, "tx.oil_rig"

 15,  0, "t.dither_tile"
 15,  0, "tx.darkness"
 15,  2, "mask.tile"
 15,  2, "t.unknown1"
  7,  0, "t.blend.arctic" ;ice over neighbors
 15,  3, "t.blend.coast"
 15,  3, "t.blend.lake"
 15,  4, "user.attention"
 15,  5, "tx.fog"

;goto path sprites
 14,  7, "path.step"            ; turn boundary within path
 14,  8, "path.exhausted_mp"    ; tip of path, no MP left
 15,  7, "path.normal"          ; tip of path with MP remaining
 15,  8, "path.waypoint"
}
