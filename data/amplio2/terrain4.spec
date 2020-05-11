
[spec]

; Format and options of this spec file:
options = "+spec3"

[info]

artists = "
    Hogne Håskjold
    Tim F. Smith <yoohootim@hotmail.com>
    Yautja
    Daniel Speyer
    Eleazar
"

[file]
gfx = "amplio2/terrain4"

[grid_coasts]

x_top_left = 1
y_top_left = 645
dx = 48
dy = 24
pixel_border = 1

tiles = { "row", "column","tag"

; ocean cell sprites.  See doc/README.graphics
 0, 0,  "t.ocean_cell_u000"
 0, 2,  "t.ocean_cell_u100"
 0, 4,  "t.ocean_cell_u010"
 0, 6,  "t.ocean_cell_u110"
 0, 8,  "t.ocean_cell_u001"
 0, 10, "t.ocean_cell_u101"
 0, 12, "t.ocean_cell_u011"
 0, 14, "t.ocean_cell_u111"
 
 1, 0,  "t.ocean_cell_d000"
 1, 2,  "t.ocean_cell_d100"
 1, 4,  "t.ocean_cell_d010"
 1, 6,  "t.ocean_cell_d110"
 1, 8,  "t.ocean_cell_d001"
 1, 10, "t.ocean_cell_d101"
 1, 12, "t.ocean_cell_d011"
 1, 14, "t.ocean_cell_d111"

 2, 0,  "t.ocean_cell_l000"
 2, 2,  "t.ocean_cell_l100"
 2, 4,  "t.ocean_cell_l010"
 2, 6,  "t.ocean_cell_l110"
 2, 8,  "t.ocean_cell_l001"
 2, 10, "t.ocean_cell_l101"
 2, 12, "t.ocean_cell_l011"
 2, 14, "t.ocean_cell_l111"

 2, 1,  "t.ocean_cell_r000"
 2, 3,  "t.ocean_cell_r100"
 2, 5,  "t.ocean_cell_r010"
 2, 7,  "t.ocean_cell_r110"
 2, 9,  "t.ocean_cell_r001"
 2, 11, "t.ocean_cell_r101"
 2, 13, "t.ocean_cell_r011"
 2, 15, "t.ocean_cell_r111"

; Deep Ocean sprites.
; 3, 0,  "t.deep_cell_u000"
; 3, 2,  "t.deep_cell_u100"
; 3, 4,  "t.deep_cell_u010"
; 3, 6,  "t.deep_cell_u110"
; 3, 8,  "t.deep_cell_u001"
; 3, 10, "t.deep_cell_u101"
; 3, 12, "t.deep_cell_u011"
; 3, 14, "t.deep_cell_u111"
 
; 4, 0,  "t.deep_cell_d000"
; 4, 2,  "t.deep_cell_d100"
; 4, 4,  "t.deep_cell_d010"
; 4, 6,  "t.deep_cell_d110"
; 4, 8,  "t.deep_cell_d001"
; 4, 10, "t.deep_cell_d101"
; 4, 12, "t.deep_cell_d011"
; 4, 14, "t.deep_cell_d111"

; 5, 0,  "t.deep_cell_l000"
; 5, 2,  "t.deep_cell_l100"
; 5, 4,  "t.deep_cell_l010"
; 5, 6,  "t.deep_cell_l110"
; 5, 8,  "t.deep_cell_l001"
; 5, 10, "t.deep_cell_l101"
; 5, 12, "t.deep_cell_l011"
; 5, 14, "t.deep_cell_l111"

; 5, 1,  "t.deep_cell_r000"
; 5, 3,  "t.deep_cell_r100"
; 5, 5,  "t.deep_cell_r010"
; 5, 7,  "t.deep_cell_r110"
; 5, 9,  "t.deep_cell_r001"
; 5, 11, "t.deep_cell_r101"
; 5, 13, "t.deep_cell_r011"
; 5, 15, "t.deep_cell_r111"
}
