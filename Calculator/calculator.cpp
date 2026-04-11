#include "screen_constants.h"
#include "screen_manager.h"
#include "screen_callbacks.h"
extern const lv_font_t calc_symbols_24;
extern lv_obj_t *screenRoots[];
extern lv_indev_t *touchInput;
void applyRootStyle(lv_obj_t *obj);
lv_color_t lvColor(uint8_t r, uint8_t g, uint8_t b);
void setLinePoints(lv_point_precise_t points[2], float x1, float y1, float x2, float y2);
void styleCalculatorButton(lv_obj_t *button, const char *labelText);

namespace
{
const ScreenModule kModule = {
    ScreenId::Calculator,
    "Calculator",
    waveformBuildCalculatorScreen,
    waveformRefreshCalculatorScreen,
    waveformEnterCalculatorScreen,
    waveformLeaveCalculatorScreen,
    waveformTickCalculatorScreen,
    waveformCalculatorScreenRoot,
};
} // namespace

const ScreenModule &calculatorScreenModule()
{
  return kModule;
}


struct CalculatorUi
{
  lv_obj_t *screen = nullptr;
  lv_obj_t *historyLabel = nullptr;
  lv_obj_t *displayLabel = nullptr;
  lv_obj_t *buttons[20] = {};
  lv_obj_t *buttonLabels[20] = {};
  lv_point_precise_t iconLinePoints[20][4][2] = {};

  double accumulator = 0.0;
  double lastOperand = 0.0;
  char pendingOperator = '\0';
  char lastOperator = '\0';
  bool hasAccumulator = false;
  bool enteringNewValue = true;
  bool error = false;
  bool trigUsesDegrees = true;
  bool showingFunctionSet = false;
  char currentText[24] = "0";
  char historyText[48] = "";
};

CalculatorUi calculatorUi;

void setCalculatorText(char *buffer, size_t capacity, const char *text)
{
  if (capacity == 0) {
    return;
  }

  snprintf(buffer, capacity, "%s", text ? text : "");
}

constexpr size_t kCalculatorButtonCount = 20;

constexpr const char *kCalculatorBasicLabels[kCalculatorButtonCount] = {
    "fn", "+/-", "%", "/",
    "7",  "8",   "9", "x",
    "4",  "5",   "6", "-",
    "1",  "2",   "3", "+",
    "BS", "0",   ".", "=",
};

constexpr const char *kCalculatorFunctionSetLabels[kCalculatorButtonCount] = {
    "123", "sin",  "cos", "/",
    "tan", "sqrt", "sq",  "x",
    "1/x", "ln",   "log", "-",
    "n!",  "pi",   "e",   "+",
    "BS",  "rand", ".",   "=",
};

constexpr const char *kCalculatorSupportedFunctionLabels[] = {
    "sqrt",
    "sq",
    "1/x",
    "sin",
    "cos",
    "tan",
    "ln",
    "log",
    "pi",
    "e",
    "n!",
    "rand",
};

bool isCalculatorFunctionLabel(const char *labelText)
{
  if (!labelText) {
    return false;
  }

  for (const char *functionLabel : kCalculatorSupportedFunctionLabels) {
    if (strcmp(labelText, functionLabel) == 0) {
      return true;
    }
  }

  return false;
}

void formatCalculatorValue(double value, char *buffer, size_t capacity)
{
  if (!isfinite(value)) {
    setCalculatorText(buffer, capacity, "Error");
    return;
  }

  if (fabs(value) < 0.0000001) {
    value = 0.0;
  }

  snprintf(buffer, capacity, "%.10g", value);
}

void updateCalculatorDisplayFont()
{
  if (!calculatorUi.displayLabel) {
    return;
  }

  const size_t length = strlen(calculatorUi.currentText);
  const lv_font_t *font = &lv_font_montserrat_36;
  if (length > 12) {
    font = &lv_font_montserrat_24;
  } else if (length > 9) {
    font = &lv_font_montserrat_28;
  }
  lv_obj_set_style_text_font(calculatorUi.displayLabel, font, 0);
}

void refreshCalculatorScreen()
{
  if (!calculatorUi.screen || !calculatorUi.historyLabel || !calculatorUi.displayLabel) {
    return;
  }

  bool hasHistory = calculatorUi.historyText[0] != '\0';
  if (hasHistory) {
    lv_obj_clear_flag(calculatorUi.historyLabel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_align(calculatorUi.historyLabel, LV_ALIGN_TOP_RIGHT, 0, 6);
    lv_obj_align(calculatorUi.displayLabel, LV_ALIGN_TOP_RIGHT, 0, 28);
  } else {
    lv_obj_add_flag(calculatorUi.historyLabel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_align(calculatorUi.displayLabel, LV_ALIGN_TOP_RIGHT, 0, 10);
  }

  lv_label_set_text(calculatorUi.historyLabel, calculatorUi.historyText);
  lv_label_set_text(calculatorUi.displayLabel, calculatorUi.currentText);
  updateCalculatorDisplayFont();
}

void applyCalculatorButtonSet(const char *const *labels)
{
  for (size_t i = 0; i < kCalculatorButtonCount; ++i) {
    if (!calculatorUi.buttons[i] || !calculatorUi.buttonLabels[i]) {
      continue;
    }
    styleCalculatorButton(calculatorUi.buttons[i], labels[i]);
  }
}

void setCalculatorCurrentValue(double value)
{
  formatCalculatorValue(value, calculatorUi.currentText, sizeof(calculatorUi.currentText));
}

double calculatorCurrentValue()
{
  return strtod(calculatorUi.currentText, nullptr);
}

void setCalculatorHistory(const char *text)
{
  setCalculatorText(calculatorUi.historyText, sizeof(calculatorUi.historyText), text);
}

void resetCalculator()
{
  calculatorUi.accumulator = 0.0;
  calculatorUi.lastOperand = 0.0;
  calculatorUi.pendingOperator = '\0';
  calculatorUi.lastOperator = '\0';
  calculatorUi.hasAccumulator = false;
  calculatorUi.enteringNewValue = true;
  calculatorUi.error = false;
  setCalculatorText(calculatorUi.currentText, sizeof(calculatorUi.currentText), "0");
  setCalculatorHistory("");
  refreshCalculatorScreen();
}

void toggleCalculatorButtonSet()
{
  calculatorUi.showingFunctionSet = !calculatorUi.showingFunctionSet;
  applyCalculatorButtonSet(calculatorUi.showingFunctionSet ? kCalculatorFunctionSetLabels : kCalculatorBasicLabels);
}

void showCalculatorError(const char *reason)
{
  calculatorUi.accumulator = 0.0;
  calculatorUi.lastOperand = 0.0;
  calculatorUi.pendingOperator = '\0';
  calculatorUi.lastOperator = '\0';
  calculatorUi.hasAccumulator = false;
  calculatorUi.enteringNewValue = true;
  calculatorUi.error = true;
  setCalculatorText(calculatorUi.currentText, sizeof(calculatorUi.currentText), "Error");
  setCalculatorHistory(reason);
  refreshCalculatorScreen();
}

void ensureCalculatorReadyForInput()
{
  if (calculatorUi.error) {
    resetCalculator();
  }
}

bool applyCalculatorBinaryOperation(char op, double lhs, double rhs, double &result)
{
  switch (op) {
    case '+':
      result = lhs + rhs;
      return true;
    case '-':
      result = lhs - rhs;
      return true;
    case '*':
      result = lhs * rhs;
      return true;
    case '/':
      if (fabs(rhs) < 0.0000001) {
        return false;
      }
      result = lhs / rhs;
      return true;
    default:
      result = rhs;
      return true;
  }
}

void updateCalculatorPendingHistory()
{
  if (calculatorUi.pendingOperator == '\0' || !calculatorUi.hasAccumulator) {
    return;
  }

  char lhs[24];
  formatCalculatorValue(calculatorUi.accumulator, lhs, sizeof(lhs));
  snprintf(calculatorUi.historyText, sizeof(calculatorUi.historyText), "%s %c", lhs, calculatorUi.pendingOperator);
}

bool factorialCalculatorValue(double value, double &result)
{
  double rounded = round(value);
  if (value < 0.0 || fabs(value - rounded) > 0.0000001 || rounded > 170.0) {
    return false;
  }

  result = 1.0;
  for (int i = 2; i <= static_cast<int>(rounded); ++i) {
    result *= static_cast<double>(i);
  }

  return true;
}



void inputCalculatorDigit(char digit)
{
  ensureCalculatorReadyForInput();

  if (calculatorUi.enteringNewValue) {
    calculatorUi.currentText[0] = digit;
    calculatorUi.currentText[1] = '\0';
    calculatorUi.enteringNewValue = false;
    refreshCalculatorScreen();
    return;
  }

  if ((strcmp(calculatorUi.currentText, "0") == 0) || (strcmp(calculatorUi.currentText, "-0") == 0)) {
    if (calculatorUi.currentText[0] == '-') {
      calculatorUi.currentText[1] = digit;
      calculatorUi.currentText[2] = '\0';
    } else {
      calculatorUi.currentText[0] = digit;
      calculatorUi.currentText[1] = '\0';
    }
    refreshCalculatorScreen();
    return;
  }

  size_t length = strlen(calculatorUi.currentText);
  if (length + 1 < sizeof(calculatorUi.currentText)) {
    calculatorUi.currentText[length] = digit;
    calculatorUi.currentText[length + 1] = '\0';
  }
  refreshCalculatorScreen();
}

void inputCalculatorDecimal()
{
  ensureCalculatorReadyForInput();

  if (calculatorUi.enteringNewValue) {
    setCalculatorText(calculatorUi.currentText, sizeof(calculatorUi.currentText), "0.");
    calculatorUi.enteringNewValue = false;
    refreshCalculatorScreen();
    return;
  }

  if (strchr(calculatorUi.currentText, '.') == nullptr &&
      strlen(calculatorUi.currentText) + 1 < sizeof(calculatorUi.currentText)) {
    strcat(calculatorUi.currentText, ".");
  }
  refreshCalculatorScreen();
}

void toggleCalculatorSign()
{
  ensureCalculatorReadyForInput();

  if (calculatorUi.enteringNewValue) {
    setCalculatorText(calculatorUi.currentText, sizeof(calculatorUi.currentText), "-0");
    calculatorUi.enteringNewValue = false;
    refreshCalculatorScreen();
    return;
  }

  if (strcmp(calculatorUi.currentText, "0") == 0) {
    refreshCalculatorScreen();
    return;
  }

  if (calculatorUi.currentText[0] == '-') {
    memmove(calculatorUi.currentText, calculatorUi.currentText + 1, strlen(calculatorUi.currentText));
  } else if (strlen(calculatorUi.currentText) + 1 < sizeof(calculatorUi.currentText)) {
    memmove(calculatorUi.currentText + 1, calculatorUi.currentText, strlen(calculatorUi.currentText) + 1);
    calculatorUi.currentText[0] = '-';
  }
  refreshCalculatorScreen();
}

void backspaceCalculator()
{
  ensureCalculatorReadyForInput();

  if (calculatorUi.enteringNewValue) {
    refreshCalculatorScreen();
    return;
  }

  size_t length = strlen(calculatorUi.currentText);
  if (length <= 1 || (length == 2 && calculatorUi.currentText[0] == '-')) {
    setCalculatorText(calculatorUi.currentText, sizeof(calculatorUi.currentText), "0");
    calculatorUi.enteringNewValue = true;
    refreshCalculatorScreen();
    return;
  }

  calculatorUi.currentText[length - 1] = '\0';
  refreshCalculatorScreen();
}

void applyCalculatorPercent()
{
  ensureCalculatorReadyForInput();

  double value = calculatorCurrentValue();
  if (calculatorUi.pendingOperator != '\0' && calculatorUi.hasAccumulator) {
    value = calculatorUi.accumulator * value / 100.0;
  } else {
    value /= 100.0;
  }

  setCalculatorCurrentValue(value);
  calculatorUi.enteringNewValue = false;
  refreshCalculatorScreen();
}

bool commitCalculatorPendingOperation(double rhs)
{
  double result = 0.0;
  if (!applyCalculatorBinaryOperation(calculatorUi.pendingOperator, calculatorUi.accumulator, rhs, result)) {
    showCalculatorError("Cannot divide by zero");
    return false;
  }

  calculatorUi.accumulator = result;
  calculatorUi.hasAccumulator = true;
  setCalculatorCurrentValue(result);
  return true;
}

void queueCalculatorOperator(char op)
{
  ensureCalculatorReadyForInput();

  double value = calculatorCurrentValue();
  if (!calculatorUi.hasAccumulator) {
    calculatorUi.accumulator = value;
    calculatorUi.hasAccumulator = true;
  } else if (calculatorUi.pendingOperator != '\0' && !calculatorUi.enteringNewValue) {
    if (!commitCalculatorPendingOperation(value)) {
      return;
    }
  } else if (calculatorUi.pendingOperator == '\0') {
    calculatorUi.accumulator = value;
  }

  calculatorUi.pendingOperator = op;
  calculatorUi.enteringNewValue = true;
  calculatorUi.lastOperator = '\0';
  updateCalculatorPendingHistory();
  refreshCalculatorScreen();
}

void applyCalculatorEquals()
{
  ensureCalculatorReadyForInput();

  if (calculatorUi.pendingOperator != '\0') {
    double rhs = calculatorUi.enteringNewValue ? calculatorUi.accumulator : calculatorCurrentValue();
    char lhsText[24];
    char rhsText[24];
    formatCalculatorValue(calculatorUi.accumulator, lhsText, sizeof(lhsText));
    formatCalculatorValue(rhs, rhsText, sizeof(rhsText));

    if (!commitCalculatorPendingOperation(rhs)) {
      return;
    }

    snprintf(calculatorUi.historyText,
             sizeof(calculatorUi.historyText),
             "%s %c %s",
             lhsText,
             calculatorUi.pendingOperator,
             rhsText);
    calculatorUi.lastOperator = calculatorUi.pendingOperator;
    calculatorUi.lastOperand = rhs;
    calculatorUi.pendingOperator = '\0';
    calculatorUi.enteringNewValue = true;
    refreshCalculatorScreen();
    return;
  }

  if (calculatorUi.lastOperator != '\0') {
    double lhs = calculatorCurrentValue();
    char lhsText[24];
    char rhsText[24];
    double result = 0.0;

    formatCalculatorValue(lhs, lhsText, sizeof(lhsText));
    formatCalculatorValue(calculatorUi.lastOperand, rhsText, sizeof(rhsText));
    if (!applyCalculatorBinaryOperation(calculatorUi.lastOperator, lhs, calculatorUi.lastOperand, result)) {
      showCalculatorError("Cannot divide by zero");
      return;
    }

    calculatorUi.accumulator = result;
    calculatorUi.hasAccumulator = true;
    setCalculatorCurrentValue(result);
    snprintf(calculatorUi.historyText,
             sizeof(calculatorUi.historyText),
             "%s %c %s",
             lhsText,
             calculatorUi.lastOperator,
             rhsText);
    calculatorUi.enteringNewValue = true;
    refreshCalculatorScreen();
    return;
  }

  refreshCalculatorScreen();
}

void applyCalculatorFunction(const char *action)
{
  ensureCalculatorReadyForInput();

  if (!action) {
    return;
  }

  double result = 0.0;
  double value = calculatorCurrentValue();
  char inputText[24];
  setCalculatorText(inputText, sizeof(inputText), calculatorUi.currentText);

  if (strcmp(action, "sqrt") == 0) {
    if (value < 0.0) {
      showCalculatorError("sqrt needs >= 0");
      return;
    }
    result = sqrt(value);
    snprintf(calculatorUi.historyText, sizeof(calculatorUi.historyText), "sqrt(%s)", inputText);
  } else if (strcmp(action, "sq") == 0) {
    result = value * value;
    snprintf(calculatorUi.historyText, sizeof(calculatorUi.historyText), "(%s)^2", inputText);
  } else if (strcmp(action, "1/x") == 0) {
    if (fabs(value) < 0.0000001) {
      showCalculatorError("Cannot divide by zero");
      return;
    }
    result = 1.0 / value;
    snprintf(calculatorUi.historyText, sizeof(calculatorUi.historyText), "1/(%s)", inputText);
  } else if (strcmp(action, "abs") == 0) {
    result = fabs(value);
    snprintf(calculatorUi.historyText, sizeof(calculatorUi.historyText), "abs(%s)", inputText);
  } else if (strcmp(action, "sin") == 0) {
    double radians = calculatorUi.trigUsesDegrees ? value * (M_PI / 180.0) : value;
    result = sin(radians);
    snprintf(calculatorUi.historyText, sizeof(calculatorUi.historyText), "sin(%s)", inputText);
  } else if (strcmp(action, "cos") == 0) {
    double radians = calculatorUi.trigUsesDegrees ? value * (M_PI / 180.0) : value;
    result = cos(radians);
    snprintf(calculatorUi.historyText, sizeof(calculatorUi.historyText), "cos(%s)", inputText);
  } else if (strcmp(action, "tan") == 0) {
    double radians = calculatorUi.trigUsesDegrees ? value * (M_PI / 180.0) : value;
    result = tan(radians);
    if (!isfinite(result)) {
      showCalculatorError("tan is undefined");
      return;
    }
    snprintf(calculatorUi.historyText, sizeof(calculatorUi.historyText), "tan(%s)", inputText);
  } else if (strcmp(action, "ln") == 0) {
    if (value <= 0.0) {
      showCalculatorError("ln needs > 0");
      return;
    }
    result = log(value);
    snprintf(calculatorUi.historyText, sizeof(calculatorUi.historyText), "ln(%s)", inputText);
  } else if (strcmp(action, "log") == 0) {
    if (value <= 0.0) {
      showCalculatorError("log needs > 0");
      return;
    }
    result = log10(value);
    snprintf(calculatorUi.historyText, sizeof(calculatorUi.historyText), "log(%s)", inputText);
  } else if (strcmp(action, "pi") == 0) {
    result = M_PI;
    setCalculatorHistory("pi");
  } else if (strcmp(action, "e") == 0) {
    result = M_E;
    setCalculatorHistory("e");
  } else if (strcmp(action, "n!") == 0) {
    if (!factorialCalculatorValue(value, result)) {
      showCalculatorError("n! needs int 0-170");
      return;
    }
    snprintf(calculatorUi.historyText, sizeof(calculatorUi.historyText), "(%s)!", inputText);
  } else if (strcmp(action, "rand") == 0) {
    result = static_cast<double>(random(0, 1000000)) / 1000000.0;
    setCalculatorHistory("rand");
  } else {
    return;
  }

  if (!isfinite(result)) {
    showCalculatorError("Invalid operation");
    return;
  }

  setCalculatorCurrentValue(result);
  calculatorUi.accumulator = result;
  calculatorUi.hasAccumulator = true;
  calculatorUi.enteringNewValue = true;
  calculatorUi.pendingOperator = '\0';
  calculatorUi.lastOperator = '\0';
  calculatorUi.lastOperand = 0.0;
  refreshCalculatorScreen();
}

void handleCalculatorAction(const char *action)
{
  if (!action) {
    return;
  }

  if (strcmp(action, "C") == 0) {
    resetCalculator();
    return;
  }
  if (strcmp(action, "+/-") == 0) {
    toggleCalculatorSign();
    return;
  }
  if (strcmp(action, "%") == 0) {
    applyCalculatorPercent();
    return;
  }
  if (strcmp(action, "BS") == 0) {
    backspaceCalculator();
    return;
  }
  if (strcmp(action, "fn") == 0 || strcmp(action, "123") == 0) {
    toggleCalculatorButtonSet();
    return;
  }
  if (isCalculatorFunctionLabel(action)) {
    applyCalculatorFunction(action);
    return;
  }
  if (strcmp(action, ".") == 0) {
    inputCalculatorDecimal();
    return;
  }
  if (strcmp(action, "=") == 0) {
    applyCalculatorEquals();
    return;
  }
  if (strcmp(action, "x") == 0) {
    queueCalculatorOperator('*');
    return;
  }
  if (strcmp(action, "/") == 0) {
    queueCalculatorOperator('/');
    return;
  }
  if (strlen(action) == 1 && strchr("+-", action[0])) {
    queueCalculatorOperator(action[0]);
    return;
  }
  if (strlen(action) == 1 && action[0] >= '0' && action[0] <= '9') {
    inputCalculatorDigit(action[0]);
  }
}

void handleCalculatorButtonEvent(lv_event_t *event)
{
  lv_obj_t *button = static_cast<lv_obj_t *>(lv_event_get_target(event));
  lv_obj_t *label = lv_obj_get_child(button, 0);
  if (!label) {
    return;
  }

  const char *action = lv_label_get_text(label);
  lv_event_code_t code = lv_event_get_code(event);
  if (strcmp(action, "BS") == 0 && code == LV_EVENT_LONG_PRESSED) {
    lv_obj_add_flag(button, LV_OBJ_FLAG_USER_1);
    resetCalculator();
    return;
  }

  if (code != LV_EVENT_CLICKED) {
    return;
  }

  if (lv_obj_has_flag(button, LV_OBJ_FLAG_USER_1)) {
    lv_obj_clear_flag(button, LV_OBJ_FLAG_USER_1);
    return;
  }

  handleCalculatorAction(action);
}

int calculatorButtonIndex(lv_obj_t *button)
{
  for (size_t i = 0; i < kCalculatorButtonCount; ++i) {
    if (calculatorUi.buttons[i] == button) {
      return static_cast<int>(i);
    }
  }

  return -1;
}

void clearCalculatorButtonDecorations(lv_obj_t *button)
{
  while (lv_obj_get_child_count(button) > 1) {
    lv_obj_delete(lv_obj_get_child(button, lv_obj_get_child_count(button) - 1));
  }
}

void addCalculatorIconLine(lv_obj_t *button,
                           int buttonIndex,
                           int slot,
                           float x1,
                           float y1,
                           float x2,
                           float y2,
                           int width,
                           lv_color_t color)
{
  setLinePoints(calculatorUi.iconLinePoints[buttonIndex][slot], x1, y1, x2, y2);
  lv_obj_t *line = lv_line_create(button);
  lv_line_set_points(line, calculatorUi.iconLinePoints[buttonIndex][slot], 2);
  lv_obj_set_style_line_width(line, width, 0);
  lv_obj_set_style_line_rounded(line, true, 0);
  lv_obj_set_style_line_color(line, color, 0);
  lv_obj_center(line);
}

void addCalculatorIconDot(lv_obj_t *button, int size, int x, int y, lv_color_t color)
{
  lv_obj_t *dot = lv_obj_create(button);
  lv_obj_set_size(dot, size, size);
  lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(dot, color, 0);
  lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(dot, 0, 0);
  lv_obj_set_style_pad_all(dot, 0, 0);
  lv_obj_align(dot, LV_ALIGN_CENTER, x, y);
}

void addCalculatorButtonIcon(lv_obj_t *button, const char *labelText, lv_color_t color)
{
  int buttonIndex = calculatorButtonIndex(button);
  if (buttonIndex < 0) {
    return;
  }

  if (strcmp(labelText, "x") == 0) {
    addCalculatorIconLine(button, buttonIndex, 0, 8, 8, 26, 26, 3, color);
    addCalculatorIconLine(button, buttonIndex, 1, 26, 8, 8, 26, 3, color);
    return;
  }

  if (strcmp(labelText, "/") == 0) {
    addCalculatorIconDot(button, 6, 0, -11, color);
    addCalculatorIconLine(button, buttonIndex, 0, 6, 16, 28, 16, 3, color);
    addCalculatorIconDot(button, 6, 0, 11, color);
    return;
  }

  if (strcmp(labelText, "BS") == 0) {
    addCalculatorIconLine(button, buttonIndex, 0, 3, 16, 12, 6, 3, color);
    addCalculatorIconLine(button, buttonIndex, 1, 3, 16, 12, 26, 3, color);
    addCalculatorIconLine(button, buttonIndex, 2, 16, 10, 28, 22, 3, color);
    addCalculatorIconLine(button, buttonIndex, 3, 28, 10, 16, 22, 3, color);
    return;
  }

  if (strcmp(labelText, "+/-") == 0) {
    // ± icon: plus sign on top, minus bar on bottom
    addCalculatorIconLine(button, buttonIndex, 0, 8, 12, 26, 12, 3, color); // horizontal of +
    addCalculatorIconLine(button, buttonIndex, 1, 17, 5, 17, 19, 3, color); // vertical of +
    addCalculatorIconLine(button, buttonIndex, 2, 8, 22, 26, 22, 3, color); // minus bar
    return;
  }
}

void styleCalculatorButton(lv_obj_t *button, const char *labelText)
{
  lv_color_t bg = lvColor(18, 24, 30);
  lv_color_t border = lvColor(36, 44, 54);
  lv_color_t text = lvColor(248, 250, 252);

  if (strcmp(labelText, "C") == 0 || strcmp(labelText, "BS") == 0) {
    bg = lvColor(44, 22, 24);
    border = lvColor(92, 46, 52);
  } else if (strcmp(labelText, "fn") == 0 || strcmp(labelText, "123") == 0 ||
             strcmp(labelText, "+/-") == 0 || strcmp(labelText, "%") == 0 || isCalculatorFunctionLabel(labelText)) {
    bg = lvColor(28, 34, 42);
    border = lvColor(58, 72, 88);
  } else if (strcmp(labelText, "x") == 0 || strcmp(labelText, "/") == 0 ||
             (strlen(labelText) == 1 && strchr("+-=", labelText[0]))) {
    bg = lvColor(12, 68, 142);
    border = lvColor(52, 126, 212);
  }

  lv_obj_set_size(button, kCalculatorButtonWidth, kCalculatorButtonHeight);
  lv_obj_set_style_radius(button, 20, 0);
  lv_obj_set_style_bg_color(button, bg, 0);
  lv_obj_set_style_bg_opa(button, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(button, 1, 0);
  lv_obj_set_style_border_color(button, border, 0);
  lv_obj_set_style_shadow_width(button, 0, 0);
  lv_obj_set_style_pad_all(button, 0, 0);
  lv_obj_set_style_bg_color(button, lvColor(24, 92, 182), LV_STATE_PRESSED);

  lv_obj_t *label = lv_obj_get_child(button, 0);
  if (!label) {
    label = lv_label_create(button);
  }
  const lv_font_t *font = &lv_font_montserrat_24;
  size_t length = strlen(labelText);
  if (length > 6) {
    font = &lv_font_montserrat_14;
  } else if (length > 4) {
    font = &lv_font_montserrat_16;
  } else if (length > 3) {
    font = &lv_font_montserrat_18;
  } else if (length > 2) {
    font = &lv_font_montserrat_20;
  }
  lv_obj_set_style_text_font(label, font, 0);
  lv_obj_set_style_text_color(label, text, 0);
  lv_label_set_text(label, labelText);
  lv_obj_clear_flag(label, LV_OBJ_FLAG_HIDDEN);
  lv_obj_center(label);

  clearCalculatorButtonDecorations(button);
  // Use dedicated symbol glyphs for operator buttons
  const char *glyph = nullptr;
  if      (strcmp(labelText, "x")   == 0) glyph = "\xC3\x97";      // ×
  else if (strcmp(labelText, "/")   == 0) glyph = "\xC3\xB7";      // ÷
  else if (strcmp(labelText, "+/-") == 0) glyph = "\xC2\xB1";      // ±
  else if (strcmp(labelText, "BS")  == 0) glyph = "\xEF\x95\x9A";  // ⌫ (FA5 backspace)

  if (glyph) {
    lv_obj_set_style_text_font(label, &calc_symbols_24, 0);
    lv_label_set_text(label, glyph);
    lv_obj_clear_flag(label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_center(label);
  }
}

lv_obj_t *buildCalculatorScreen()
{
  calculatorUi.screen = lv_obj_create(nullptr);
  applyRootStyle(calculatorUi.screen);

  lv_obj_t *card = lv_obj_create(calculatorUi.screen);
  lv_obj_set_size(card, kCalculatorCardWidth, LCD_HEIGHT - 32);
  lv_obj_align(card, LV_ALIGN_CENTER, 0, 0);
  lv_obj_set_style_radius(card, 28, 0);
  lv_obj_set_style_bg_color(card, lvColor(6, 10, 16), 0);
  lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(card, 1, 0);
  lv_obj_set_style_border_color(card, lvColor(24, 34, 46), 0);
  lv_obj_set_style_pad_all(card, 18, 0);
  lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

  calculatorUi.historyLabel = lv_label_create(card);
  lv_obj_set_width(calculatorUi.historyLabel, kCalculatorCardWidth - 36);
  lv_obj_set_style_text_font(calculatorUi.historyLabel, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(calculatorUi.historyLabel, lvColor(112, 124, 140), 0);
  lv_obj_set_style_text_align(calculatorUi.historyLabel, LV_TEXT_ALIGN_RIGHT, 0);
  lv_label_set_long_mode(calculatorUi.historyLabel, LV_LABEL_LONG_CLIP);
  lv_obj_align(calculatorUi.historyLabel, LV_ALIGN_TOP_RIGHT, 0, 6);

  calculatorUi.displayLabel = lv_label_create(card);
  lv_obj_set_width(calculatorUi.displayLabel, kCalculatorCardWidth - 36);
  lv_obj_set_style_text_color(calculatorUi.displayLabel, lvColor(250, 252, 255), 0);
  lv_obj_set_style_text_align(calculatorUi.displayLabel, LV_TEXT_ALIGN_RIGHT, 0);
  lv_label_set_long_mode(calculatorUi.displayLabel, LV_LABEL_LONG_CLIP);
  lv_obj_align(calculatorUi.displayLabel, LV_ALIGN_TOP_RIGHT, 0, 10);

  lv_obj_t *divider = lv_obj_create(card);
  lv_obj_set_size(divider, kCalculatorCardWidth - 36, 1);
  lv_obj_align(divider, LV_ALIGN_TOP_MID, 0, 84);
  lv_obj_set_style_bg_color(divider, lvColor(22, 30, 40), 0);
  lv_obj_set_style_border_width(divider, 0, 0);
  lv_obj_set_style_pad_all(divider, 0, 0);

  lv_obj_t *grid = lv_obj_create(card);
  lv_obj_set_size(grid, kCalculatorCardWidth - 36, 282);
  lv_obj_align(grid, LV_ALIGN_BOTTOM_MID, 0, 0);
  lv_obj_set_layout(grid, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(grid, LV_FLEX_FLOW_ROW_WRAP);
  lv_obj_set_flex_align(grid, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
  lv_obj_set_style_pad_all(grid, 0, 0);
  lv_obj_set_style_pad_row(grid, kCalculatorButtonGap, 0);
  lv_obj_set_style_pad_column(grid, kCalculatorButtonGap, 0);
  lv_obj_set_style_bg_opa(grid, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(grid, 0, 0);
  lv_obj_clear_flag(grid, LV_OBJ_FLAG_SCROLLABLE);

  for (size_t i = 0; i < kCalculatorButtonCount; ++i) {
    lv_obj_t *button = lv_button_create(grid);
    styleCalculatorButton(button, kCalculatorBasicLabels[i]);
    calculatorUi.buttons[i] = button;
    calculatorUi.buttonLabels[i] = lv_obj_get_child(button, 0);
    lv_indev_set_long_press_time(touchInput, kCalculatorClearLongPressMs);
    lv_obj_add_event_cb(button, handleCalculatorButtonEvent, LV_EVENT_LONG_PRESSED, nullptr);
    lv_obj_add_event_cb(button, handleCalculatorButtonEvent, LV_EVENT_CLICKED, nullptr);
  }

  calculatorUi.showingFunctionSet = false;
  resetCalculator();
  return calculatorUi.screen;
}
void buildCalculatorScreenRoot()
{
  screenRoots[static_cast<size_t>(ScreenId::Calculator)] = buildCalculatorScreen();
}

lv_obj_t *waveformCalculatorScreenRoot()
{
  return screenRoots[static_cast<size_t>(ScreenId::Calculator)];
}

bool waveformBuildCalculatorScreen()
{
  if (!waveformCalculatorScreenRoot()) {
    buildCalculatorScreenRoot();
  }
  return waveformCalculatorScreenRoot() && calculatorUi.screen && calculatorUi.historyLabel && calculatorUi.displayLabel;
}

bool waveformRefreshCalculatorScreen()
{
  if (!calculatorUi.screen || !calculatorUi.historyLabel || !calculatorUi.displayLabel) {
    return false;
  }
  refreshCalculatorScreen();
  return true;
}

void waveformEnterCalculatorScreen()
{
}

void waveformLeaveCalculatorScreen()
{
}

void waveformTickCalculatorScreen(uint32_t nowMs)
{
  (void)nowMs;
  refreshCalculatorScreen();
}
