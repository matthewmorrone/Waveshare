# screens/motion.py - IMU motion visualization (Dot, Cube, Raw modes)
import lvgl as lv
import math
import prefs
from screen_manager import ScreenModule, SCREEN_MOTION

MODE_DOT  = 0
MODE_CUBE = 1
MODE_RAW  = 2

_MODE_KEY = "motion_mode"


def _lv_color(r, g, b):
    return lv.color_make(r, g, b)


class MotionScreen(ScreenModule):
    def __init__(self, imu):
        super().__init__(SCREEN_MOTION, "Motion")
        self._imu = imu
        self._mode = prefs.get_int(_MODE_KEY, MODE_DOT)
        self._scr = None
        self._dot = None
        self._raw_labels = []
        self._canvas = None
        self._cube_lines = []
        # Low-pass filter state
        self._ax = self._ay = self._az = 0.0
        self._gx = self._gy = self._gz = 0.0
        self._alpha = 0.2

    def build(self):
        self._scr = lv.obj(None)
        self._scr.set_style_bg_color(_lv_color(0, 0, 0), 0)
        self._scr.clear_flag(lv.obj.FLAG.SCROLLABLE)

        self._build_header()
        self._build_mode_ui()
        self._root = self._scr
        return True

    def _build_header(self):
        title = lv.label(self._scr)
        title.set_style_text_font(lv.font_montserrat_18, 0)
        title.set_style_text_color(_lv_color(180, 180, 255), 0)
        title.set_text("Motion")
        title.align(lv.ALIGN.TOP_MID, 0, 10)

        self._mode_label = lv.label(self._scr)
        self._mode_label.set_style_text_font(lv.font_montserrat_14, 0)
        self._mode_label.set_style_text_color(_lv_color(100, 100, 100), 0)
        mode_names = ["Dot", "Cube", "Raw"]
        self._mode_label.set_text(mode_names[self._mode])
        self._mode_label.align(lv.ALIGN.TOP_MID, 0, 36)

        # Tap header to cycle modes
        self._mode_label.add_event_cb(
            lambda e: self._cycle_mode(), lv.EVENT.CLICKED, None
        )

    def _build_mode_ui(self):
        # Clear old mode widgets
        for l in self._raw_labels:
            l.del_async()
        self._raw_labels.clear()
        if self._dot:
            self._dot.del_async()
            self._dot = None
        if self._canvas:
            self._canvas.del_async()
            self._canvas = None

        if self._mode == MODE_DOT:
            self._build_dot_mode()
        elif self._mode == MODE_CUBE:
            self._build_cube_mode()
        else:
            self._build_raw_mode()

    def _build_dot_mode(self):
        # Crosshair background circle
        bg = lv.obj(self._scr)
        bg.set_size(240, 240)
        bg.align(lv.ALIGN.CENTER, 0, 20)
        bg.set_style_bg_color(_lv_color(10, 10, 18), 0)
        bg.set_style_radius(120, 0)
        bg.set_style_border_color(_lv_color(40, 40, 80), 0)
        bg.set_style_border_width(1, 0)
        bg.clear_flag(lv.obj.FLAG.SCROLLABLE)

        # Indicator dot
        self._dot = lv.obj(bg)
        self._dot.set_size(24, 24)
        self._dot.set_style_bg_color(_lv_color(52, 132, 255), 0)
        self._dot.set_style_radius(12, 0)
        self._dot.set_style_border_width(0, 0)
        self._dot.align(lv.ALIGN.CENTER, 0, 0)

    def _build_cube_mode(self):
        self._canvas = lv.canvas(self._scr)
        self._canvas.set_size(280, 280)
        self._canvas.align(lv.ALIGN.CENTER, 0, 20)
        self._cube_angle_x = 0.0
        self._cube_angle_y = 0.0
        self._cube_angle_z = 0.0

    def _build_raw_mode(self):
        labels_def = [
            "AX:", "AY:", "AZ:",
            "GX:", "GY:", "GZ:",
            "Pitch:", "Roll:",
        ]
        for i, name in enumerate(labels_def):
            row = lv.obj(self._scr)
            row.set_size(300, 32)
            row.set_style_bg_opa(lv.OPA.TRANSP, 0)
            row.set_style_border_width(0, 0)
            row.align(lv.ALIGN.TOP_MID, 0, 80 + i * 36)

            name_l = lv.label(row)
            name_l.set_style_text_font(lv.font_montserrat_14, 0)
            name_l.set_style_text_color(_lv_color(120, 120, 120), 0)
            name_l.set_text(name)
            name_l.align(lv.ALIGN.LEFT_MID, 10, 0)

            val_l = lv.label(row)
            val_l.set_style_text_font(lv.font_montserrat_14, 0)
            val_l.set_style_text_color(_lv_color(200, 200, 255), 0)
            val_l.set_text("0.000")
            val_l.align(lv.ALIGN.RIGHT_MID, -10, 0)
            self._raw_labels.append(val_l)

    def _cycle_mode(self):
        self._mode = (self._mode + 1) % 3
        prefs.put_int(_MODE_KEY, self._mode)
        mode_names = ["Dot", "Cube", "Raw"]
        self._mode_label.set_text(mode_names[self._mode])
        self._build_mode_ui()

    def tick(self, elapsed_ms):
        if not self._imu:
            return
        try:
            ax, ay, az = self._imu.read_accel()
            gx, gy, gz = self._imu.read_gyro()
        except Exception:
            return

        # Low-pass filter
        a = self._alpha
        self._ax += a * (ax - self._ax)
        self._ay += a * (ay - self._ay)
        self._az += a * (az - self._az)
        self._gx += a * (gx - self._gx)
        self._gy += a * (gy - self._gy)
        self._gz += a * (gz - self._gz)

        pitch = math.atan2(-self._ax, math.sqrt(self._ay**2 + self._az**2)) * 180 / math.pi
        roll  = math.atan2(self._ay, self._az) * 180 / math.pi

        if self._mode == MODE_DOT and self._dot:
            # Map pitch/roll to dot position in 240x240 area
            cx = int(roll  / 90 * 96)   # ±96 px
            cy = int(pitch / 90 * 96)
            self._dot.align(lv.ALIGN.CENTER, cx, cy)

        elif self._mode == MODE_RAW and self._raw_labels:
            vals = [self._ax, self._ay, self._az,
                    self._gx, self._gy, self._gz,
                    pitch, roll]
            for i, v in enumerate(vals):
                if i < len(self._raw_labels):
                    self._raw_labels[i].set_text(f"{v:.3f}")

        elif self._mode == MODE_CUBE and self._canvas:
            self._cube_angle_x += self._gy * elapsed_ms / 1000.0
            self._cube_angle_y += self._gx * elapsed_ms / 1000.0
            self._draw_cube()

    def _draw_cube(self):
        """Draw rotating 3D cube on canvas."""
        W, H = 280, 280
        cx, cy = W // 2, H // 2
        size = 60

        ax = math.radians(self._cube_angle_x % 360)
        ay = math.radians(self._cube_angle_y % 360)

        verts = [
            [-1,-1,-1],[1,-1,-1],[1,1,-1],[-1,1,-1],
            [-1,-1, 1],[1,-1, 1],[1,1, 1],[-1,1, 1],
        ]

        def rotate(v):
            x, y, z = v
            # Rotate around Y
            x2 = x * math.cos(ay) + z * math.sin(ay)
            z2 = -x * math.sin(ay) + z * math.cos(ay)
            # Rotate around X
            y2 = y * math.cos(ax) - z2 * math.sin(ax)
            z3 = y * math.sin(ax) + z2 * math.cos(ax)
            return x2, y2, z3

        def project(v):
            x, y, z = v
            f = 3.0 / (3.0 + z)
            return int(cx + x * size * f), int(cy + y * size * f)

        pts = [project(rotate(v)) for v in verts]
        edges = [
            (0,1),(1,2),(2,3),(3,0),  # back face
            (4,5),(5,6),(6,7),(7,4),  # front face
            (0,4),(1,5),(2,6),(3,7),  # sides
        ]

        self._canvas.fill_bg(_lv_color(0,0,0), lv.OPA.COVER)
        color = _lv_color(52, 132, 255)
        for a, b in edges:
            self._canvas.draw_line([
                lv.point_t({'x': pts[a][0], 'y': pts[a][1]}),
                lv.point_t({'x': pts[b][0], 'y': pts[b][1]}),
            ], 2, color, lv.OPA.COVER)

    def root(self):
        return self._root
