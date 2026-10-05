/* Render firmware UI scenarios to a raw RGB565 framebuffer. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app/fw_info.h"
#include "app/gauge.h"
#include "app/gauge_sweep.h"
#include "hal/hal_ble.h"
#include "hal/hal_dfu.h"
#include "hal/hal_gpio.h"
#include "hal/hal_log.h"
#include "hal/hal_nvm.h"
#include "lib/display/display.h"
#include "lib/ecu/ecu.h"
#include "lib/ui/dial.h"
#include "lib/ui/dial_layout.h"

#define PANEL_W (DISPLAY_WIDTH)
#define PANEL_H (DISPLAY_HEIGHT)
#define NEXT_ID ((hal_gpio_id_t)1u)
#define ACTION_ID ((hal_gpio_id_t)2u)

static uint16_t frame[PANEL_W * PANEL_H];
static const dial_widget_t *observed_dial;
static int32_t observed_aim;

/* Only gauge.c's host build calls this observer. The actual drawing and
 * motion still go through the production dial_poll. */
void preview_dial_poll(dial_widget_t *d, uint32_t now_ms, bool has_value,
                       int32_t aim) {
  dial_poll(d, now_ms, has_value, aim);
  observed_dial = d;
  observed_aim = aim;
}
static size_t selected;
static size_t initial_selection;
static size_t initial_theme; /* 0 light, 1 dark */
/* How many 33 ms polls run before the frame is captured. The power-on sweep
 * takes about a second, so the default lets it finish. */
static unsigned polls = 120u;
static int32_t value;
static bool value_available = true;
static bool ecu_available = true;
static ecu_clear_t clear_result = ECU_CLEAR_IDLE;
static bool action_edge_pending;
static uint32_t action_edge_at;
/* NEXT taps after the power-on sweep, to reach a later page. */
static bool next_edge_level;
static bool next_edge_pending;
static uint32_t next_edge_at;
static unsigned next_taps;
/* Hold ACTION after the NEXT taps: on the info page that starts update mode. */
static bool hold_action_after_taps;
/* What the ECU reports as its ROM ID: 0 until it has identified itself. */
static uint32_t rom_id_value;

/* Same shapes and polarities as ssm1_params.c: stored codes 2/3 and the
 * JECS2 input switches (Neutral is active-low). */
static const ecu_flag_t codes_flags[] = {
    {"21 Temp", 0x01u, true}, {"22 Knock", 0x02u, true},
    {"23 MAF", 0x04u, true},  {"24 IAC", 0x08u, true},
    {"31 TPS", 0x10u, true},  {"32 Oxygen", 0x20u, true},
    {"33 VSS", 0x40u, true},  {"35 Purge", 0x80u, true},
};
static const ecu_flag_t switch_flags[] = {
    {"Ignition", 0x80u, true},    {"Auto Trans", 0x40u, true},
    {"Test Mode", 0x20u, true},   {"Read Mode", 0x10u, true},
    {"Neutral", 0x04u, false},    {"Park", 0x02u, true},
    {"California", 0x01u, false},
};

size_t ecu_param_count(void) { return ecu_available ? 7u : 0u; }

const ecu_param_info_t *ecu_param_info(size_t idx) {
  static const ecu_param_info_t info[] = {
      {.name = "Boost/Vacuum",
       .unit = "psi",
       .unit_neg = "inHg",
       .decode = ECU_DEC_BOOST,
       .scale_num = 1,
       .scale_den = 1,
       .offset = 0,
       .decimals = 3,
       .lookup = NULL,
       .flags = NULL,
       .flag_count = 0,
       .trouble_codes = false},
      {.name = "Engine Speed",
       .unit = "rpm",
       .unit_neg = NULL,
       .decode = ECU_DEC_LINEAR,
       .scale_num = 1,
       .scale_den = 1,
       .offset = 0,
       .decimals = 0,
       .lookup = NULL,
       .flags = NULL,
       .flag_count = 0,
       .trouble_codes = false},
      {.name = "Load",
       .unit = "",
       .unit_neg = NULL,
       .decode = ECU_DEC_LINEAR,
       .scale_num = 1,
       .scale_den = 1,
       .offset = 0,
       .decimals = 0,
       .lookup = NULL,
       .flags = NULL,
       .flag_count = 0,
       .trouble_codes = false},
      {.name = "Stored Codes 2/3",
       .unit = "",
       .unit_neg = NULL,
       .decode = ECU_DEC_FLAGS,
       .scale_num = 1,
       .scale_den = 1,
       .offset = 0,
       .decimals = 0,
       .lookup = NULL,
       .flags = codes_flags,
       .flag_count = 8,
       .trouble_codes = true},
      {.name = "Input Switches",
       .unit = "",
       .unit_neg = NULL,
       .decode = ECU_DEC_FLAGS,
       .scale_num = 1,
       .scale_den = 1,
       .offset = 0,
       .decimals = 0,
       .lookup = NULL,
       .flags = switch_flags,
       .flag_count = 7,
       .trouble_codes = false},
      {.name = "Coolant Temp",
       .unit = "F",
       .decode = ECU_DEC_LINEAR,
       .scale_num = 1,
       .scale_den = 1,
       .decimals = 1},
      {.name = "System Voltage",
       .unit = "V",
       .decode = ECU_DEC_LINEAR,
       .scale_num = 1,
       .scale_den = 1,
       .decimals = 2},
  };

  return (idx < 7u) ? &info[idx] : NULL;
}

bool ecu_select_param(size_t idx) {
  selected = idx;
  return idx < ecu_param_count();
}

bool ecu_get_value(size_t idx, int32_t *out) {
  bool found = ecu_available && value_available && (idx == selected);

  if (found) {
    *out = value;
  }
  return found;
}

ecu_link_t ecu_link(void) {
  return ecu_available ? ECU_LINK_UP : ECU_LINK_DOWN;
}

const fw_info_t fw_info = {"0.1.0"};

/* Update mode: Bluetooth and the restart are not part of the picture. */
hal_status_t hal_ble_open(const hal_ble_cfg_t *cfg) {
  (void)cfg;
  return HAL_OK;
}

hal_ble_evt_t hal_ble_poll_event(void) { return HAL_BLE_EVT_NONE; }

hal_status_t hal_random_u32(uint32_t *out) {
  *out = 123456u;
  return HAL_OK;
}

void hal_dfu_reboot(void) {}

uint16_t ecu_assume_profile(uint16_t profile_key) { return profile_key; }

uint32_t ecu_rom_id(void) { return rom_id_value; }

bool ecu_clear_start(void) {
  clear_result = ECU_CLEAR_BUSY;
  return true;
}

ecu_clear_t ecu_clear_state(void) { return clear_result; }

void hal_log_write(const char *text) { (void)text; }

hal_status_t hal_nvm_save(const char *key, const void *data, size_t len) {
  (void)key;
  (void)data;
  (void)len;
  return HAL_OK;
}

hal_status_t hal_nvm_load(const char *key, void *data, size_t max,
                          size_t *len) {
  uint32_t saved = (uint32_t)initial_selection;

  if (strcmp(key, "theme") == 0) {
    saved = (uint32_t)initial_theme;
  }
  if (strcmp(key, "ecu_profile") == 0) {
    /* The ECU we are connected to, so nothing looks like a new car. */
    saved = rom_id_value & 0xFFFFu;
  }
  if (max < sizeof(saved)) {
    return HAL_ERR_INVALID;
  }
  (void)memcpy(data, &saved, sizeof(saved));
  *len = sizeof(saved);
  return HAL_OK;
}

hal_status_t hal_gpio_open(hal_gpio_id_t id, hal_gpio_mode_t mode) {
  (void)id;
  (void)mode;
  return HAL_OK;
}

hal_status_t hal_gpio_watch(hal_gpio_id_t id) {
  (void)id;
  return HAL_OK;
}

bool hal_gpio_take_edge(hal_gpio_id_t id, bool *level, uint32_t *timestamp_ms) {
  if ((id == NEXT_ID) && next_edge_pending) {
    *level = next_edge_level;
    *timestamp_ms = next_edge_at;
    next_edge_pending = false;
    return true;
  }
  if ((id != ACTION_ID) || !action_edge_pending) {
    return false;
  }
  *level = true;
  *timestamp_ms = action_edge_at;
  action_edge_pending = false;
  return true;
}

hal_status_t display_init(const display_pins_t *pins) {
  (void)pins;
  return display_clear(0u);
}

hal_status_t display_on(void) { return HAL_OK; }

hal_status_t display_clear(uint16_t color_rgb565) {
  for (size_t i = 0; i < (PANEL_W * PANEL_H); i++) {
    frame[i] = color_rgb565;
  }
  return HAL_OK;
}

hal_status_t display_blit(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                          const uint16_t *pixels) {
  if ((pixels == NULL) || ((uint32_t)x + w > PANEL_W) ||
      ((uint32_t)y + h > PANEL_H)) {
    return HAL_ERR_INVALID;
  }
  for (uint16_t row = 0; row < h; row++) {
    for (uint16_t col = 0; col < w; col++) {
      frame[((size_t)y + row) * PANEL_W + x + col] =
          pixels[((size_t)row * w) + col];
    }
  }
  return HAL_OK;
}

static bool choose_scene(const char *scene) {
  if (strcmp(scene, "boost_vacuum") == 0) {
    value = -18000;
  } else if (strcmp(scene, "boost_zero") == 0) {
    value = 0;
  } else if (strcmp(scene, "boost_positive") == 0) {
    value = 15000;
  } else if (strcmp(scene, "rpm_3500") == 0) {
    initial_selection = 1u;
    value = 3500;
  } else if (strcmp(scene, "rpm_7000") == 0) {
    initial_selection = 1u;
    value = 7000;
  } else if (strcmp(scene, "coolant_180") == 0) {
    initial_selection = 5u;
    value = 1800;
  } else if (strcmp(scene, "coolant_240") == 0) {
    initial_selection = 5u;
    value = 2400;
  } else if (strcmp(scene, "voltage_14") == 0) {
    initial_selection = 6u;
    value = 1400;
  } else if (strcmp(scene, "digital_load") == 0) {
    initial_selection = 2u;
    value = 42;
  } else if (strcmp(scene, "flags_codes") == 0) {
    initial_selection = 3u;
    value = 0x22; /* 22 Knock and 32 Oxygen stored */
  } else if (strcmp(scene, "flags_no_codes") == 0) {
    initial_selection = 3u;
    value = 0x00;
  } else if (strcmp(scene, "flags_switches") == 0) {
    initial_selection = 4u;
    value = 0xD5;
  } else if (strcmp(scene, "flags_stale") == 0) {
    initial_selection = 3u;
    value_available = false;
  } else if ((strcmp(scene, "flags_hold_to_clear") == 0) ||
             (strcmp(scene, "flags_clearing") == 0) ||
             (strcmp(scene, "flags_cleared") == 0)) {
    initial_selection = 3u;
    value = 0x22;
  } else if (strcmp(scene, "boost_sweep_peak") == 0) {
    value = 15000;
    /* The sweep has just reached full scale: the needle is eased and speed
     * limited, so it takes eight frames to get there, and the reading takes
     * over a frame later. */
    polls = 8u;
  } else if (strcmp(scene, "anim_power_on") == 0) {
    value = 0;
  } else if ((strcmp(scene, "anim_boost_step") == 0) ||
             (strcmp(scene, "anim_boost_ramp") == 0)) {
    value = 0;
  } else if (strcmp(scene, "anim_coolant_sweep") == 0) {
    initial_selection = 5u;
    value = 1000;
  } else if (strcmp(scene, "anim_voltage_ramp") == 0) {
    initial_selection = 6u;
    value = 800;
  } else if (strcmp(scene, "info_no_ecu") == 0) {
    /* No ECU: no pages, so the info page is one tap away. */
    ecu_available = false;
    value_available = false;
    next_taps = 1u;
  } else if (strcmp(scene, "info_connected") == 0) {
    initial_selection = 6u; /* the last parameter: the info page is next */
    rom_id_value = 0x013672u;
    next_taps = 1u;
  } else if (strcmp(scene, "update_mode") == 0) {
    initial_selection = 6u;
    rom_id_value = 0x013672u;
    next_taps = 1u;
    hold_action_after_taps = true;
  } else if (strcmp(scene, "no_ecu") == 0) {
    ecu_available = false;
    value_available = false;
  } else {
    return false;
  }
  return true;
}

/* A straight line from v0 at t0 to v1 at t1, held at both ends. */
static int32_t ramp(uint32_t t, uint32_t t0, uint32_t t1, int32_t v0,
                    int32_t v1) {
  if (t <= t0) {
    return v0;
  }
  if (t >= t1) {
    return v1;
  }
  return v0 + (int32_t)(((int64_t)(v1 - v0) * (int64_t)(t - t0)) /
                        (int64_t)(t1 - t0));
}

/* The reading at t milliseconds into an animation, in the ECU's own units. */
static int32_t anim_value(const char *scene, uint32_t t) {
  if (strcmp(scene, "anim_boost_step") == 0) {
    return (t < 500u) ? 0 : (t < 2500u) ? 15000 : -20000;
  }
  if (strcmp(scene, "anim_boost_ramp") == 0) {
    return (t < 1200u) ? ramp(t, 0u, 800u, 0, 30000)
                       : ramp(t, 1200u, 2400u, 30000, -30000);
  }
  if (strcmp(scene, "anim_coolant_sweep") == 0) {
    return (t < 1200u) ? ramp(t, 0u, 800u, 1000, 2600)
                       : ramp(t, 1200u, 2400u, 2600, 1000);
  }
  if (strcmp(scene, "anim_voltage_ramp") == 0) {
    return (t < 1200u) ? ramp(t, 0u, 800u, 800, 1600)
                       : ramp(t, 1200u, 2400u, 1600, 800);
  }
  return 0; /* anim_power_on: the needle sweeps by itself */
}

/* Appends the frame to an open file as RGB565, little endian. */
static bool append_frame(FILE *output) {
  for (size_t i = 0; i < (PANEL_W * PANEL_H); i++) {
    uint8_t pixel[2] = {(uint8_t)frame[i], (uint8_t)(frame[i] >> 8)};

    if (fwrite(pixel, sizeof(pixel), 1u, output) != 1u) {
      return false;
    }
  }
  return true;
}

/* Runs the gauge on its real 33 ms rhythm while the reading follows a script,
 * and writes every frame, one after another, to path. The time and reading of
 * each frame go to path.csv. */
static int run_anim(const char *scene, const char *path, unsigned duration_ms,
                    uint32_t now) {
  char csv_path[256];
  FILE *output = fopen(path, "wb");
  FILE *csv;
  uint32_t start = now;

  (void)snprintf(csv_path, sizeof(csv_path), "%s.csv", path);
  csv = fopen(csv_path, "w");
  if ((output == NULL) || (csv == NULL)) {
    return 1;
  }
  (void)fputs("t_ms,value,aim_value,target_deg,displayed_deg,pivot_x,pivot_y\n",
              csv);
  /* Only the power-on scene shows the sweep; the others start at rest and
   * follow their script. */
  if (strcmp(scene, "anim_power_on") != 0) {
    gauge_sweep_cancel();
  }
  while ((now - start) <= duration_ms) {
    value = anim_value(scene, now - start);
    observed_dial = NULL;
    gauge_poll(now);
    if (observed_dial == NULL) {
      (void)fputs("animation scene has no analog dial\n", stderr);
      (void)fclose(csv);
      (void)fclose(output);
      return 1;
    }
    (void)fprintf(csv, "%u,%d,%d,%d,%d,%d,%d\n", (unsigned)(now - start),
                  (int)value, (int)observed_aim, (int)observed_dial->target_deg,
                  (int)observed_dial->displayed_deg, (int)DIAL_CX,
                  (int)(DIAL_CY + dial_pivot_down(observed_dial->cfg)));
    if (!append_frame(output)) {
      return 1;
    }
    now += 33u;
  }
  (void)fclose(csv);
  return (fclose(output) == 0) ? 0 : 1;
}

static bool write_frame(const char *path) {
  FILE *output = fopen(path, "wb");

  if (output == NULL) {
    return false;
  }
  for (size_t i = 0; i < (PANEL_W * PANEL_H); i++) {
    uint8_t pixel[2] = {(uint8_t)frame[i], (uint8_t)(frame[i] >> 8)};

    if (fwrite(pixel, sizeof(pixel), 1u, output) != 1u) {
      (void)fclose(output);
      return false;
    }
  }
  return fclose(output) == 0;
}

int main(int argc, char **argv) {
  const display_pins_t pins = {0};
  uint32_t now = 1u;

  char scene[64];
  size_t scene_len = (argc >= 3) ? strlen(argv[1]) : 0u;

  /* A "_dark" suffix picks the dark theme for any scene. */
  if ((scene_len > 5u) && (scene_len < sizeof(scene)) &&
      (strcmp(&argv[1][scene_len - 5u], "_dark") == 0)) {
    initial_theme = 1u;
    (void)memcpy(scene, argv[1], scene_len - 5u);
    scene[scene_len - 5u] = '\0';
  } else if (argc >= 3) {
    (void)snprintf(scene, sizeof(scene), "%s", argv[1]);
  } else {
    scene[0] = '\0';
  }
  if ((argc < 3) || !choose_scene(scene)) {
    (void)fputs("usage: preview_ui SCENE OUTPUT.rgb565 [POLLS]\n"
                "       preview_ui anim_NAME OUTPUT.frames [DURATION_MS]\n",
                stderr);
    return 2;
  }
  if (argc == 4) {
    polls = (unsigned)strtoul(argv[3], NULL, 10);
  }
  if (gauge_init(&pins, NEXT_ID, ACTION_ID) != HAL_OK) {
    return 1;
  }
  if (strncmp(scene, "anim_", 5u) == 0) {
    return run_anim(scene, argv[2], (argc == 4) ? polls : 4000u, now);
  }
  gauge_poll(now);

  if ((strcmp(scene, "flags_hold_to_clear") == 0) ||
      (strcmp(scene, "flags_clearing") == 0) ||
      (strcmp(scene, "flags_cleared") == 0)) {
    /* Hold ACTION: prompt after debounce, clearing after 2 s, then the
     * result. */
    action_edge_at = now;
    action_edge_pending = true;
    now += 35u;
    gauge_poll(now);
    if (strcmp(scene, "flags_hold_to_clear") != 0) {
      now += 2001u;
      gauge_poll(now);
      if (strcmp(scene, "flags_cleared") == 0) {
        clear_result = ECU_CLEAR_DONE;
        gauge_poll(now + 1u);
      } else {
        /* A few dot steps in, so the dots show. */
        gauge_poll(now + 900u);
      }
    }
  } else {
    for (unsigned i = 0u; i < polls; i++) {
      now += 33u;
      gauge_poll(now);
    }
    for (unsigned tap = 0u; tap < next_taps; tap++) {
      next_edge_level = true;
      next_edge_at = now;
      next_edge_pending = true;
      now += 40u;
      gauge_poll(now);
      next_edge_level = false;
      next_edge_at = now;
      next_edge_pending = true;
      now += 40u;
      gauge_poll(now);
      now += 40u;
      gauge_poll(now);
    }
    if (hold_action_after_taps) {
      action_edge_at = now;
      action_edge_pending = true;
      now += 35u;
      gauge_poll(now);
      now += 2600u;
      gauge_poll(now);
      now += 40u;
      gauge_poll(now);
    }
  }

  if (!write_frame(argv[2])) {
    (void)fputs("could not write preview framebuffer\n", stderr);
    return 1;
  }
  return 0;
}
