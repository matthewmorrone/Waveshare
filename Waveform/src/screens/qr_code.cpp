#include "config/screen_constants.h"
#include "core/screen_manager.h"
#include "modules/wifi_manager.h"
#include "screens/screen_callbacks.h"
#include <qrcode.h>

extern lv_obj_t *screenRoots[];
void applyRootStyle(lv_obj_t *obj);
lv_color_t lvColor(uint8_t r, uint8_t g, uint8_t b);

namespace
{
const ScreenModule kModule = {
    ScreenId::Qr,
    "QR",
    waveformBuildQrScreen,
    waveformRefreshQrScreen,
    waveformEnterQrScreen,
    waveformLeaveQrScreen,
    waveformTickQrScreen,
    waveformQrScreenRoot,
};
} // namespace

const ScreenModule &qrScreenModule()
{
  return kModule;
}


struct QrUi
{
  lv_obj_t *screen = nullptr;
  lv_obj_t *titleLabel = nullptr;
  lv_obj_t *panel = nullptr;
  lv_obj_t *qrcode = nullptr;
  lv_obj_t *contentLabel = nullptr;
  lv_obj_t *indexLabel = nullptr;
};


QrUi qrUi;
bool qrBuilt = false;
size_t currentQrIndex = 0;
size_t renderedQrIndex = SIZE_MAX;
bool qrNeedsRender = true;
lv_obj_t *qrRenderTargetCanvas = nullptr;
uint8_t qrCanvasBuffer[LV_CANVAS_BUF_SIZE(kQrCanvasSize, kQrCanvasSize, 1, 1)] = {};

const QrEntry &currentQrEntry()
{
  return kQrEntries[currentQrIndex % kQrEntryCount];
}

lv_obj_t *buildQrScreen()
{
  qrUi.screen = lv_obj_create(nullptr);
  applyRootStyle(qrUi.screen);

  qrUi.titleLabel = lv_label_create(qrUi.screen);
  lv_obj_set_width(qrUi.titleLabel, LCD_WIDTH - 40);
  lv_obj_set_style_text_font(qrUi.titleLabel, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(qrUi.titleLabel, lvColor(248, 250, 252), 0);
  lv_obj_set_style_text_align(qrUi.titleLabel, LV_TEXT_ALIGN_CENTER, 0);
  lv_label_set_text(qrUi.titleLabel, "QR");
  lv_obj_align(qrUi.titleLabel, LV_ALIGN_TOP_MID, 0, kQrTitleY);

  qrUi.panel = lv_obj_create(qrUi.screen);
  lv_obj_set_size(qrUi.panel, kQrPanelSize, kQrPanelSize);
  lv_obj_align(qrUi.panel, LV_ALIGN_TOP_MID, 0, kQrPanelY);
  lv_obj_set_style_bg_color(qrUi.panel, lvColor(255, 255, 255), 0);
  lv_obj_set_style_bg_opa(qrUi.panel, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(qrUi.panel, 24, 0);
  lv_obj_set_style_border_width(qrUi.panel, 0, 0);
  lv_obj_set_style_pad_all(qrUi.panel, 0, 0);
  lv_obj_clear_flag(qrUi.panel, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(qrUi.panel, LV_OBJ_FLAG_CLICKABLE);

  qrUi.qrcode = lv_canvas_create(qrUi.panel);
  lv_canvas_set_buffer(qrUi.qrcode, qrCanvasBuffer, kQrCanvasSize, kQrCanvasSize, LV_COLOR_FORMAT_I1);
  lv_canvas_set_palette(qrUi.qrcode, 0, lv_color_to_32(lvColor(255, 255, 255), LV_OPA_COVER));
  lv_canvas_set_palette(qrUi.qrcode, 1, lv_color_to_32(lvColor(0, 0, 0), LV_OPA_COVER));
  lv_canvas_fill_bg(qrUi.qrcode, lvColor(255, 255, 255), LV_OPA_COVER);
  lv_obj_set_size(qrUi.qrcode, kQrCanvasSize, kQrCanvasSize);
  lv_obj_center(qrUi.qrcode);
  lv_obj_clear_flag(qrUi.qrcode, LV_OBJ_FLAG_CLICKABLE);

  qrUi.contentLabel = lv_label_create(qrUi.screen);
  lv_obj_set_width(qrUi.contentLabel, LCD_WIDTH - 44);
  lv_obj_set_style_text_font(qrUi.contentLabel, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(qrUi.contentLabel, lvColor(164, 174, 188), 0);
  lv_obj_set_style_text_align(qrUi.contentLabel, LV_TEXT_ALIGN_CENTER, 0);
  lv_label_set_long_mode(qrUi.contentLabel, LV_LABEL_LONG_WRAP);
  lv_label_set_text(qrUi.contentLabel, "");
  lv_obj_align(qrUi.contentLabel, LV_ALIGN_TOP_MID, 0, kQrTextY);

  qrUi.indexLabel = lv_label_create(qrUi.screen);
  lv_obj_set_width(qrUi.indexLabel, LCD_WIDTH - 44);
  lv_obj_set_style_text_font(qrUi.indexLabel, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(qrUi.indexLabel, lvColor(112, 124, 140), 0);
  lv_obj_set_style_text_align(qrUi.indexLabel, LV_TEXT_ALIGN_CENTER, 0);
  lv_label_set_text(qrUi.indexLabel, "");
  lv_obj_align(qrUi.indexLabel, LV_ALIGN_BOTTOM_MID, 0, -22);

  qrNeedsRender = true;
  renderedQrIndex = SIZE_MAX;
  qrBuilt = true;
  return qrUi.screen;
}

void buildQrScreenRoot()
{
  screenRoots[static_cast<size_t>(ScreenId::Qr)] = buildQrScreen();
}

void renderQrCodeToCanvas(esp_qrcode_handle_t qrcode)
{
  if (!qrRenderTargetCanvas) {
    return;
  }

  int qrSize = esp_qrcode_get_size(qrcode);
  int moduleSize = max(1, kQrCanvasSize / qrSize);
  int drawSize = moduleSize * qrSize;
  int start = (kQrCanvasSize - drawSize) / 2;
  lv_display_t *targetDisplay = lv_obj_get_display(qrRenderTargetCanvas);
  lv_display_enable_invalidation(targetDisplay, false);

  for (int y = 0; y < qrSize; ++y) {
    for (int x = 0; x < qrSize; ++x) {
      if (!esp_qrcode_get_module(qrcode, x, y)) {
        continue;
      }

      int baseX = start + (x * moduleSize);
      int baseY = start + (y * moduleSize);
      for (int py = 0; py < moduleSize; ++py) {
        for (int px = 0; px < moduleSize; ++px) {
          lv_canvas_set_px(qrRenderTargetCanvas, baseX + px, baseY + py, lvColor(0, 0, 0), LV_OPA_COVER);
        }
      }
    }
  }

  lv_display_enable_invalidation(targetDisplay, true);
  lv_obj_invalidate(qrRenderTargetCanvas);
}

void refreshQrScreen()
{
  if (!qrBuilt || !qrUi.panel || !qrUi.qrcode || !qrUi.titleLabel || !qrUi.contentLabel || !qrUi.indexLabel) {
    return;
  }

  const QrEntry &entry = currentQrEntry();
  lv_label_set_text(qrUi.titleLabel, entry.title);
  lv_label_set_text(qrUi.contentLabel, entry.payload);

  String indexText = String(currentQrIndex + 1) + " / " + String(kQrEntryCount) +
                     "  swipe up/down";
  lv_label_set_text(qrUi.indexLabel, indexText.c_str());

  if (!qrNeedsRender && renderedQrIndex == currentQrIndex) {
    return;
  }

  lv_canvas_fill_bg(qrUi.qrcode, lvColor(255, 255, 255), LV_OPA_COVER);
  qrRenderTargetCanvas = qrUi.qrcode;
  esp_qrcode_config_t qrConfig = ESP_QRCODE_CONFIG_DEFAULT();
  qrConfig.display_func = renderQrCodeToCanvas;
  qrConfig.max_qrcode_version = 10;
  qrConfig.qrcode_ecc_level = ESP_QRCODE_ECC_LOW;
  esp_err_t result = esp_qrcode_generate(&qrConfig, entry.payload);
  qrRenderTargetCanvas = nullptr;
  if (result != ESP_OK) {
    lv_label_set_text(qrUi.indexLabel, "QR generation failed");
    return;
  }

  renderedQrIndex = currentQrIndex;
  qrNeedsRender = false;
}

lv_obj_t *waveformQrScreenRoot()
{
  return screenRoots[static_cast<size_t>(ScreenId::Qr)];
}

bool waveformBuildQrScreen()
{
  if (!waveformQrScreenRoot()) {
    buildQrScreenRoot();
  }

  return qrBuilt && waveformQrScreenRoot() && qrUi.screen && qrUi.titleLabel && qrUi.panel && qrUi.qrcode &&
         qrUi.contentLabel;
}

bool waveformRefreshQrScreen()
{
  if (!qrBuilt || !qrUi.screen || !qrUi.titleLabel || !qrUi.panel || !qrUi.qrcode || !qrUi.contentLabel) {
    return false;
  }

  refreshQrScreen();
  return true;
}

void waveformEnterQrScreen()
{
  qrNeedsRender = true;
}

void waveformLeaveQrScreen()
{
}

void waveformTickQrScreen(uint32_t nowMs)
{
  (void)nowMs;
}
