// RetroRadio case — parametric, OpenSCAD 2021+
// Coordinates (case): X = left→right seen from the front, Y = front(0)→back, Z = bottom(0)→top.
// Export one part:  openscad -D 'part="body"' -o body.stl retroradio.scad
// Parts: body, panel, base, grid, window, window_test, assembly (preview only)

part = "assembly";
body_alpha = 0.35;   // preview only; 0 hides the body
$fn = 48;

// ---------------- overall ----------------
W = 220; D = 150; H = 56;
wall   = 3;       // body side/top/back walls
base_t = 3;       // base plate
corner_r = 3;     // footprint corner radius
lip    = 3;       // front frame lip, inward from walls
pnl_t  = 3;       // front panel thickness
slot_y0 = 2;      // front lip depth (Y 0..2)
slot_w = pnl_t + 0.2;
rib_t  = 2;
fit    = 0.2;     // general clearance

slot_y1  = slot_y0 + slot_w;          // back of panel slot
pnl_y0   = slot_y0 + 0.1;             // panel front face
pnl_back = pnl_y0 + pnl_t;            // panel back face
inner_top = H - wall;

// ---------------- display (XC3746 ×2) ----------------
mw = 71.5; mh = 31.5; mt = 1.5;       // matrix PCB
pitch = 4; led_x0 = 6; led_z0 = 2;    // PCB edge → first LED centre
overlap = 3.5;                        // right PCB overlaps left (right one sits in front)
cell = 3.2;                           // grid hole
disp_x = 17;                          // case X of left PCB's left edge
disp_ztop = 46;                       // case Z of PCB top edge
span = 2*mw - overlap;                // 139.5
gR = 3;                               // grid thickness in front of right board
gL = gR + mt;                         // left board sits one PCB thickness further back
win_t = 1.04;                         // window: 0.96 clear + 1 black layer (0.08), chosen from test tile
win_recess = win_t + 0.06;            // recess depth in the panel back
hold_w = 1.6;                         // holder wall
grid_ext = 0.8;                       // grid front slab extends past the PCB top/bottom for a printable rim
hold_d = gL + mt + 1;                 // holder depth behind panel

function ledx(c) = disp_x + (c < 16 ? led_x0 + pitch*c : (mw - overlap) + led_x0 + pitch*(c-16));
function ledz(r) = disp_ztop - (led_z0 + pitch*r);
open_x0 = ledx(0) - cell/2;  open_x1 = ledx(31) + cell/2;
open_z1 = disp_ztop - 0.4;   open_z0 = disp_ztop - 31.2;
reb = 2.5;                            // window recess margin around opening

// ---------------- encoder (XC3736) + knob ----------------
knob_x = 184;
knob_z = disp_ztop - mh/2;
enc_face_to_pcb = 13;                 // panel FRONT face → encoder PCB front (shaft top 22 above PCB, knob 9 tall)
enc_pcb = [30, 19];
bush_d = 8.4;

// ---------------- main board (HiFi-ESP32), lying flat, USB-C edge to the back ----------------
mb = [85, 56];
mb_x0 = (W - mb[0])/2; mb_y0 = (D - mb[1])/2 + 10;   // centred left-right, 10 mm toward the back
// measured on the test base: 58 x 49 c-c (kept centred where the first guess was)
mb_holes = [[23.5,3.5],[23.5,52.5],[81.5,3.5],[81.5,52.5]];
riser_h = 5;
pin_d = 1.7; pin_h = 2.5;

// ---------------- HUSB238 (power inlet, port flush with back wall) ----------------
hu = [22, 25]; hu_x0 = 150;
hu_y0 = D - wall - hu[1];
hu_riser = 4;
hu_port_z = base_t + hu_riser + 1.6 + 1.6;

// ---------------- back wall features ----------------
ant_x = 200; ant_z = 32; ant_d = 6.4;
anchor_xy = [wall + 12, D - wall - 10];   // zip-tie anchor in the back-left corner, clear of the screw boss
cable_x = anchor_xy[0] + 7; cable_d = 5;   // line-out notch straight behind the anchor (x = 22)

// ---------------- screws (#4 self-tapping, 12 mm) ----------------
pilot_d = 2.4; clear_d = 3.4; head_d = 6.5;
boss_r = 4.5;
bo = wall + boss_r - 0.6;   // overlap the walls (a tangent boss makes a non-manifold edge)
bosses = [[bo,14],[W-bo,14],[bo,D-bo],[W-bo,D-bo]];

font = "DejaVu Sans:style=Bold";

// ======================================================================
// helpers
module rrect(w, d, r) { offset(r) offset(-r) square([w, d]); }
// extrude a 2-D shape drawn in the (x, z) plane along +Y, starting at y0
module xz(y0, depth) { translate([0, y0, 0]) rotate([-90,0,0]) linear_extrude(depth) mirror([0,1,0]) children(); }
module xz_rect(x0, z0, x1, z1) { translate([x0, z0]) square([x1-x0, z1-z0]); }

// ======================================================================
// BODY: top + sides + back, open at front and bottom. Print top-down.
module body() {
  difference() {
    union() {
      difference() {
        translate([0,0,base_t]) linear_extrude(H-base_t) rrect(W, D, corner_r);
        // hollow (open bottom), front opening
        translate([wall, -1, base_t-1]) cube([W-2*wall, D-wall+1, H-wall-base_t+1]);
      }
      // front frame lip + panel retaining rib (sides and top)
      for (y = [[0, slot_y0], [slot_y1, slot_y1+rib_t]]) {
        translate([wall, y[0], base_t]) cube([lip, y[1]-y[0], inner_top-base_t]);
        translate([W-wall-lip, y[0], base_t]) cube([lip, y[1]-y[0], inner_top-base_t]);
        translate([wall, y[0], inner_top-lip]) cube([W-2*wall, y[1]-y[0], lip]);
      }
      // screw bosses
      for (b = bosses) translate([b[0], b[1], base_t]) cylinder(r=boss_r, h=inner_top-base_t);
    }
    for (b = bosses) translate([b[0], b[1], base_t-1]) cylinder(d=pilot_d, h=inner_top-base_t-1);
    // USB-C power inlet: thin the wall around the port so the plug seats fully
    translate([hu_x0+hu[0]/2, D, hu_port_z]) {
      // through-slot: centred on the outer face, so it must reach a full wall+ inward
      rotate([90,0,0]) linear_extrude(2*wall+2, center=true) offset(1.2) square([9.4-2.4, 3.8-2.4], center=true);
      translate([0, -0.9+0.01, 0]) rotate([90,0,0]) linear_extrude(1.8+0.02, center=true) offset(2) square([13.5-4, 7.5-4], center=true);
    }
    // antenna bulkhead
    translate([ant_x, D-wall-1, ant_z]) rotate([-90,0,0]) cylinder(d=ant_d, h=wall+2);
    // line-out cable notch (open at the bottom so the plug end needn't pass through)
    translate([cable_x, D-wall-1, 0]) rotate([-90,0,0]) linear_extrude(wall+2)
      hull() { translate([-cable_d/2, -(base_t+cable_d/2)]) square([cable_d, 0.1]);
               translate([0, -(base_t+cable_d/2)]) circle(d=cable_d);
               translate([-cable_d/2, -base_t+0.1]) square([cable_d, 0.1]); }
  }
}

// ======================================================================
// FRONT PANEL with display holder and encoder cradle. Print face-down.
module panel() {
  px0 = wall + fit; px1 = W - wall - fit;
  pz0 = base_t + 0.3; pz1 = inner_top - 0.1;
  difference() {
    union() {
      xz(pnl_y0, pnl_t) xz_rect(px0, pz0, px1, pz1);
      // display holder walls
      hx0 = disp_x - 0.3; hx1 = disp_x + span + 0.3;
      hz0 = disp_ztop - mh - 0.3; hz1 = disp_ztop + 0.3;
      // front gR deep: taller pocket so the grid gets a solid top/bottom rim;
      // behind that: PCB-sized pocket that locates the boards
      difference() {
        xz(pnl_back - 0.01, hold_d)
          xz_rect(hx0-hold_w, hz0-grid_ext-hold_w, hx1+hold_w, hz1+grid_ext+hold_w);
        xz(pnl_back - 0.02, gR + 0.02) xz_rect(hx0, hz0-grid_ext, hx1, hz1+grid_ext);
        xz(pnl_back - 0.02, hold_d + 0.04) xz_rect(hx0, hz0, hx1, hz1);
      }
      // encoder: bushing sleeve (depth stop on the encoder body) + cradle ribs
      xz(pnl_back - 0.01, enc_face_to_pcb - pnl_t - 7 + 0.01)
        translate([knob_x, knob_z]) circle(d=bush_d + 4);
      for (s = [-1, 1])
        xz(pnl_back - 0.01, enc_face_to_pcb - pnl_t + 1.6 + 2)
          xz_rect(knob_x-12, knob_z + s*(enc_pcb[1]/2+0.3) + (s>0 ? 0 : -2),
                  knob_x+12, knob_z + s*(enc_pcb[1]/2+0.3) + (s>0 ? 2 : 0));
    }
    // display opening + window recess
    xz(pnl_y0 - 1, pnl_t + 2) xz_rect(open_x0, open_z0, open_x1, open_z1);
    xz(pnl_back - win_recess, win_recess + 1) xz_rect(open_x0-reb, open_z0-reb, open_x1+reb, open_z1+reb);
    // shaft / bushing hole
    xz(pnl_y0 - 1, 20) translate([knob_x, knob_z]) circle(d=bush_d);
    // debossed labels (tone-on-tone)
    xz(pnl_y0 - 0.01, 0.6) {
      translate([open_x0, 8.5]) text("RETRORADIO", size=3.2, font=font, spacing=1.15);
      translate([knob_x, 8.5]) text("VOL · TUNE", size=2.6, font=font, halign="center", spacing=1.1);
    }
  }
}

// ======================================================================
// PIXEL GRID: black, sits behind the window. Stepped: thicker over the left (rear) board.
module grid() {
  gx0 = disp_x - 0.15; gx1 = disp_x + span + 0.15;
  gz0 = disp_ztop - mh - 0.15; gz1 = disp_ztop + 0.15;
  split = disp_x + mw - overlap;      // left edge of the right (front) board
  difference() {
    union() {
      xz(pnl_back, gR) xz_rect(gx0, gz0 - grid_ext, gx1, gz1 + grid_ext);   // front slab, full rim
      xz(pnl_back + gR - 0.01, gL - gR + 0.01) xz_rect(gx0, gz0, split, gz1); // step behind the left board
    }
    xz(pnl_back - 1, gL + 2)
      for (c = [0:31], r = [0:7]) translate([ledx(c), ledz(r)]) square(cell, center=true);
  }
}

// ======================================================================
// WINDOW: clear PETG with a thin black layer on the inside (last layers).
module window() {
  xz(pnl_back - win_recess, win_t)
    xz_rect(open_x0-reb+fit, open_z0-reb+fit, open_x1+reb-fit, open_z1+reb-fit);
}
// Test tile: 0.96 mm clear, then swap to black at Z=0.96. Steps get 1–4 black layers at 0.08 mm.
module window_test() {
  cube([60, 20, 0.96]);
  for (i = [0:3]) translate([i*15, 0, 0]) cube([15, 20, 0.96 + 0.08*(i+1)]);
  for (i = [0:3]) translate([i*15 + 7.5, 3, 0.96 + 0.08*(i+1) - 0.01])
    linear_extrude(0.3) text(str(i+1), size=4, font=font, halign="center");
}

// ======================================================================
// BASE PLATE: carries the main board, HUSB238, converters, cable anchor.
module riser(h, pin=true, d=5) {
  cylinder(d=d, h=h);
  if (pin) translate([0,0,h-0.01]) cylinder(d=pin_d, h=pin_h);
}
module base() {
  difference() {
    union() {
      linear_extrude(base_t) rrect(W, D, corner_r);
      // front frame lip + retaining rib (bottom edge of the panel)
      for (y = [[0, slot_y0], [slot_y1, slot_y1+rib_t]])
        translate([wall+lip, y[0], base_t-0.01]) cube([W-2*wall-2*lip, y[1]-y[0], lip+0.01]);
      // main board risers
      for (h = mb_holes) translate([mb_x0+h[0], mb_y0+h[1], base_t-0.01]) riser(riser_h);
      // HUSB238: pinned at the port end, posts + stops at the far end
      for (s = [-1,1]) translate([hu_x0+hu[0]/2 + s*8, D-wall-2.5, base_t-0.01]) riser(hu_riser, true, 4.5);
      for (x = [hu_x0+1.5, hu_x0+hu[0]-1.5]) {
        translate([x-1.5, hu_y0+0.5, base_t-0.01]) cube([3, 3, hu_riser]);
        translate([x-1.5, hu_y0-1.6, base_t-0.01]) cube([3, 1.6, hu_riser+1.6+1.5]);
      }
      // level converter pads (hot glue)
      // stacked in a column right of the main board (~15 mm in from the right edge),
      // centred on the board front-to-back, 8 mm apart
      conv_x = W - 14.5 - 17;
      conv_cy = mb_y0 + (mb_holes[0][1] + mb_holes[1][1]) / 2;
      for (y = [conv_cy - 21, conv_cy + 4]) translate([conv_x, y, base_t-0.01]) difference() {
        cube([17, 17, 1]); translate([1,1,-1]) cube([15, 15, 3]);
      }
      // zip-tie anchor for the line-out cable
      translate([anchor_xy[0], anchor_xy[1], base_t-0.01]) difference() {
        cube([14, 8, 3]);
        translate([-1, 1.5, -1]) cube([16, 5, 2.8]);
      }
    }
    // screw holes, counterbored from below
    for (b = bosses) translate([b[0], b[1], -1]) {
      cylinder(d=clear_d, h=base_t+2);
      cylinder(d=head_d, h=1+1.5);
    }
    // vent slots under the main board
    for (i = [0:5]) translate([mb_x0 + (mb_holes[0][0]+mb_holes[2][0])/2 - 25 + i*10, mb_y0+10, -1]) linear_extrude(base_t+2)
      hull() { circle(d=3); translate([0, 36]) circle(d=3); }
  }
}

// ======================================================================
// reference blocks (preview only)
module ghosts() {
  color("#1e5aa8", 0.9) {
    translate([disp_x, pnl_back+gL, disp_ztop-mh]) cube([mw, mt, mh]);
    translate([disp_x+mw-overlap, pnl_back+gR, disp_ztop-mh]) cube([mw, mt, mh]);
  }
  color("#2d6a2d", 0.9) translate([mb_x0, mb_y0, base_t+riser_h]) cube([mb[0], mb[1], 1.6]);
  color("#7a2d2d", 0.9) translate([hu_x0, hu_y0, base_t+hu_riser]) cube([hu[0], hu[1], 1.6]);
  color("#444") translate([knob_x, pnl_y0, knob_z]) rotate([90,0,0]) translate([0,0,1]) cylinder(d=28, h=9);
}

module assembly() {
  color("#222") base();
  if (body_alpha > 0) color("#2a2a2a", body_alpha) body();
  color("#303030") panel();
  color("#111") grid();
  color("#6a7a8a", 0.5) window();
  ghosts();
}

// ======================================================================
// print orientations
module face_down(y0) { translate([0,0,-y0]) rotate([90,0,0]) children(); }

if (part == "body")        translate([0,0,H]) rotate([180,0,0]) body();
else if (part == "panel")  face_down(pnl_y0) panel();
else if (part == "grid")   face_down(pnl_back) grid();
else if (part == "window") face_down(pnl_back - win_recess) window();
else if (part == "window_test") window_test();
else if (part == "base")   base();
else assembly();
