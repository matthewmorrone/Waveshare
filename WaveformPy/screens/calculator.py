# screens/calculator.py - Scientific calculator
import lvgl as lv
import math
from screen_manager import ScreenModule, SCREEN_CALCULATOR


def _lv_color(r, g, b):
    return lv.color_make(r, g, b)


# Button layout: (label, value/action)
_BASIC_BUTTONS = [
    [("C",  "C"),  ("±",  "±"),  ("%",  "%"),  ("÷",  "/")],
    [("7",  "7"),  ("8",  "8"),  ("9",  "9"),  ("×",  "*")],
    [("4",  "4"),  ("5",  "5"),  ("6",  "6"),  ("−",  "-")],
    [("1",  "1"),  ("2",  "2"),  ("3",  "3"),  ("+",  "+")],
    [("fn", "FN"), ("0",  "0"),  (".",  "."),  ("=",  "=")],
]

_FN_BUTTONS = [
    [("sin","sin"), ("cos","cos"), ("tan","tan"), ("÷",  "/")],
    [("7",  "7"),   ("8",  "8"),  ("9",  "9"),  ("×",  "*")],
    [("4",  "4"),   ("5",  "5"),  ("6",  "6"),  ("−",  "-")],
    [("1",  "1"),   ("2",  "2"),  ("3",  "3"),  ("+",  "+")],
    [("fn", "FN"),  ("ln","ln"),  ("√",  "sqrt"),("=",  "=")],
]

_BTN_W, _BTN_H = 80, 52
_BTN_PAD = 4


class CalculatorScreen(ScreenModule):
    def __init__(self):
        super().__init__(SCREEN_CALCULATOR, "Calculator")
        self._scr = None
        self._display_label = None
        self._expr_label = None
        self._fn_mode = False
        self._input = ""
        self._prev_val = None
        self._operator = None
        self._reset_input = False
        self._btn_widgets = []

    def build(self):
        self._scr = lv.obj(None)
        self._scr.set_style_bg_color(_lv_color(8, 8, 12), 0)
        self._scr.clear_flag(lv.obj.FLAG.SCROLLABLE)

        # Expression label (top, smaller)
        self._expr_label = lv.label(self._scr)
        self._expr_label.set_style_text_font(lv.font_montserrat_14, 0)
        self._expr_label.set_style_text_color(_lv_color(100, 100, 100), 0)
        self._expr_label.set_text("")
        self._expr_label.align(lv.ALIGN.TOP_RIGHT, -16, 14)

        # Main display
        self._display_label = lv.label(self._scr)
        self._display_label.set_style_text_font(lv.font_montserrat_48, 0)
        self._display_label.set_style_text_color(_lv_color(255, 255, 255), 0)
        self._display_label.set_text("0")
        self._display_label.align(lv.ALIGN.TOP_RIGHT, -16, 36)

        self._build_buttons()
        self._root = self._scr
        return True

    def _build_buttons(self):
        for w in self._btn_widgets:
            w.del_async()
        self._btn_widgets.clear()

        layout = _FN_BUTTONS if self._fn_mode else _BASIC_BUTTONS
        start_y = 170

        for row_i, row in enumerate(layout):
            for col_i, (label, action) in enumerate(row):
                btn = lv.btn(self._scr)
                btn.set_size(_BTN_W, _BTN_H)
                btn.set_pos(_BTN_PAD + col_i * (_BTN_W + _BTN_PAD),
                             start_y + row_i * (_BTN_H + _BTN_PAD))

                # Color coding
                if action in ("/", "*", "-", "+", "="):
                    color = _lv_color(52, 132, 255)
                elif action in ("C", "±", "%"):
                    color = _lv_color(60, 60, 70)
                elif action == "FN":
                    color = _lv_color(80, 50, 120) if not self._fn_mode else _lv_color(120, 50, 180)
                elif action in ("sin","cos","tan","ln","sqrt"):
                    color = _lv_color(40, 80, 60)
                else:
                    color = _lv_color(28, 28, 36)

                btn.set_style_bg_color(color, 0)
                btn.set_style_radius(12, 0)

                lbl = lv.label(btn)
                lbl.set_style_text_font(lv.font_montserrat_18, 0)
                lbl.set_style_text_color(_lv_color(255, 255, 255), 0)
                lbl.set_text(label)
                lbl.center()

                # Capture action in closure
                def make_handler(a):
                    return lambda e: self._handle(a)

                btn.add_event_cb(make_handler(action), lv.EVENT.CLICKED, None)
                self._btn_widgets.append(btn)

    def _handle(self, action):
        if action == "FN":
            self._fn_mode = not self._fn_mode
            self._build_buttons()
            return

        if action == "C":
            self._input = ""
            self._prev_val = None
            self._operator = None
            self._expr_label.set_text("")
            self._display_label.set_text("0")
            return

        if action == "±":
            if self._input.startswith("-"):
                self._input = self._input[1:]
            elif self._input:
                self._input = "-" + self._input
            self._display_label.set_text(self._input or "0")
            return

        if action == "=":
            self._calculate()
            return

        if action in ("+", "-", "*", "/"):
            if self._input:
                try:
                    self._prev_val = float(self._input)
                except ValueError:
                    pass
            self._operator = action
            if self._prev_val is not None:
                self._expr_label.set_text(self._fmt(self._prev_val) + " " + action)
            self._input = ""
            return

        # Function keys
        if action in ("sin", "cos", "tan", "ln", "sqrt"):
            try:
                val = float(self._input or "0")
                if action == "sin":    val = math.sin(math.radians(val))
                elif action == "cos":  val = math.cos(math.radians(val))
                elif action == "tan":  val = math.tan(math.radians(val))
                elif action == "ln":   val = math.log(val)
                elif action == "sqrt": val = math.sqrt(val)
                self._input = self._fmt(val)
                self._display_label.set_text(self._input)
            except Exception as e:
                self._display_label.set_text("Error")
                self._input = ""
            return

        if action == "%":
            try:
                val = float(self._input or "0") / 100
                self._input = self._fmt(val)
                self._display_label.set_text(self._input)
            except Exception:
                pass
            return

        # Digit / decimal
        if action == "." and "." in self._input:
            return
        self._input += action
        self._display_label.set_text(self._input)

    def _calculate(self):
        if self._prev_val is None or self._operator is None or not self._input:
            return
        try:
            b = float(self._input)
            a = self._prev_val
            op = self._operator
            if op == "+": result = a + b
            elif op == "-": result = a - b
            elif op == "*": result = a * b
            elif op == "/":
                if b == 0:
                    self._display_label.set_text("Error")
                    self._input = ""
                    return
                result = a / b
            self._input = self._fmt(result)
            self._prev_val = result
            self._operator = None
            self._expr_label.set_text("")
            self._display_label.set_text(self._input)
        except Exception:
            self._display_label.set_text("Error")
            self._input = ""

    def _fmt(self, val):
        if val == int(val):
            return str(int(val))
        return f"{val:.6g}"

    def root(self):
        return self._root
