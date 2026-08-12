"""
Pre-build patch: expose DCS command writes on Arduino_ESP32DSIPanel.

The RM69A10 AMOLED has no backlight rail - its brightness is the DCS 0x51
(WRDISBV) command on the DSI bus. Upstream Arduino_ESP32DSIPanel keeps its
esp_lcd_panel_io handle as a local inside begin() and exposes nothing but
begin() and getFrameBuffer(), so there is no way to issue that command after
init. This adds a writeCommand() method and keeps the io handle around.

Arduino_GFX is a git submodule, so the change cannot simply be committed - it
would be lost on the next submodule checkout. Hence a build-time patch.

The patch is idempotent (safe to run on every build) and refuses to guess: if
an anchor is missing it aborts the build rather than producing a binary that
silently lacks brightness control. If upstream reworks the file, update the
anchors here.
"""

import sys
from pathlib import Path

Import("env")  # type: ignore  # noqa: F821

MARKER = "Launcher patch: expose DCS command writes"

PANEL_DIR = Path("lib_modules/Arduino_GFX/src/databus")
HEADER = PANEL_DIR / "Arduino_ESP32DSIPanel.h"
SOURCE = PANEL_DIR / "Arduino_ESP32DSIPanel.cpp"

HEADER_ANCHOR = "  uint16_t *getFrameBuffer();"
HEADER_ADDITION = """  uint16_t *getFrameBuffer();

  // --- {marker} ---
  // Sends a DCS/MIPI command (optionally with parameters) to the panel after
  // begin(). Returns false when called before begin(), i.e. no io handle yet.
  bool writeCommand(uint8_t cmd, const uint8_t *data = nullptr, size_t data_bytes = 0);""".format(
    marker=MARKER
)

HEADER_MEMBER_ANCHOR = "  esp_lcd_panel_handle_t _panel_handle = NULL;"
HEADER_MEMBER_ADDITION = """  esp_lcd_panel_handle_t _panel_handle = NULL;
  esp_lcd_panel_io_handle_t _io_handle = NULL;  // {marker}""".format(marker=MARKER)

SOURCE_ANCHOR = (
    "  ESP_ERROR_CHECK(esp_lcd_new_panel_io_dbi(mipi_dsi_bus, &dbi_config, &io_handle));"
)
SOURCE_ADDITION = """  ESP_ERROR_CHECK(esp_lcd_new_panel_io_dbi(mipi_dsi_bus, &dbi_config, &io_handle));
  _io_handle = io_handle;  // {marker}""".format(marker=MARKER)

SOURCE_IMPL_ANCHOR = "#endif // #if defined(ESP32) && (CONFIG_IDF_TARGET_ESP32P4)"
SOURCE_IMPL_ADDITION = """bool Arduino_ESP32DSIPanel::writeCommand(uint8_t cmd, const uint8_t *data, size_t data_bytes)
{{
  // {marker}
  if (_io_handle == NULL)
  {{
    return false;
  }}
  return esp_lcd_panel_io_tx_param(_io_handle, cmd, data, data_bytes) == ESP_OK;
}}

#endif // #if defined(ESP32) && (CONFIG_IDF_TARGET_ESP32P4)""".format(marker=MARKER)


def fail(message):
    print("[patch_dsi_writecommand] ERROR: %s" % message)
    env.Exit(1)  # type: ignore  # noqa: F821


def apply_patch(path, replacements):
    if not path.is_file():
        fail("%s not found. Run 'git submodule update --init --recursive'." % path)

    text = path.read_text(encoding="utf-8")
    if MARKER in text:
        return False  # already patched

    for anchor, addition in replacements:
        if anchor not in text:
            fail(
                "anchor not found in %s:\n    %s\n"
                "Arduino_GFX probably changed upstream - update the anchors in "
                "support_files/patch_dsi_writecommand.py." % (path, anchor)
            )
        # Only the first occurrence: the anchors are unique statements, and
        # replacing further matches would duplicate the addition.
        text = text.replace(anchor, addition, 1)

    path.write_text(text, encoding="utf-8")
    return True


patched = False
patched |= apply_patch(
    HEADER,
    [
        (HEADER_ANCHOR, HEADER_ADDITION),
        (HEADER_MEMBER_ANCHOR, HEADER_MEMBER_ADDITION),
    ],
)
patched |= apply_patch(
    SOURCE,
    [
        (SOURCE_ANCHOR, SOURCE_ADDITION),
        (SOURCE_IMPL_ANCHOR, SOURCE_IMPL_ADDITION),
    ],
)

print(
    "[patch_dsi_writecommand] %s"
    % ("Arduino_ESP32DSIPanel patched" if patched else "already patched, skipping")
)
