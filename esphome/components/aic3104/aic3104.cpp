#include "aic3104.h"

#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace aic3104 {

static const char *const TAG = "aic3104";

#define ERROR_CHECK(err, msg) \
  if (!(err)) { \
    ESP_LOGE(TAG, msg); \
    this->mark_failed(); \
    return; \
  }

void AIC3104::setup() {
  ERROR_CHECK(this->write_byte(AIC3104_PAGE_CTRL, 0x00), "Set page 0 failed");
  ERROR_CHECK(this->write_byte(AIC3104_SW_RST, 0x01), "Software reset failed");

  // The XVF3800 I2S clock is 48 kHz with a 32-bit stereo frame.
  ERROR_CHECK(this->write_byte(AIC3104_NDAC, 0x82), "Set NDAC failed");
  ERROR_CHECK(this->write_byte(AIC3104_MDAC, 0x82), "Set MDAC failed");
  ERROR_CHECK(this->write_byte(AIC3104_DOSR, 0x80), "Set DOSR failed");
  ERROR_CHECK(this->write_byte(AIC3104_CODEC_IF, 0x30), "Set CODEC_IF failed");
  ERROR_CHECK(this->write_byte(AIC3104_SCLK_MFP3, 0x02), "Set SCLK/MFP3 failed");
  ERROR_CHECK(this->write_byte(AIC3104_AUDIO_IF_4, 0x01), "Set AUDIO_IF_4 failed");
  ERROR_CHECK(this->write_byte(AIC3104_AUDIO_IF_5, 0x01), "Set AUDIO_IF_5 failed");

  // Route and power the line outputs used by the board's integrated amplifier.
  ERROR_CHECK(this->write_byte(AIC3104_PAGE_CTRL, 0x01), "Set page 1 failed");
  ERROR_CHECK(this->write_byte(AIC3104_LDO_CTRL, 0x09), "Set LDO_CTRL failed");
  ERROR_CHECK(this->write_byte(AIC3104_PWR_CFG, 0x08), "Set PWR_CFG failed");
  ERROR_CHECK(this->write_byte(AIC3104_LDO_CTRL, 0x01), "Enable analog power failed");
  ERROR_CHECK(this->write_byte(AIC3104_CM_CTRL, 0x40), "Set CM_CTRL failed");
  ERROR_CHECK(this->write_byte(AIC3104_PLAY_CFG1, 0x00), "Set PLAY_CFG1 failed");
  ERROR_CHECK(this->write_byte(AIC3104_PLAY_CFG2, 0x00), "Set PLAY_CFG2 failed");
  ERROR_CHECK(this->write_byte(AIC3104_REF_STARTUP, 0x01), "Set REF_STARTUP failed");
  ERROR_CHECK(this->write_byte(AIC3104_HP_START, 0x25), "Set HP_START failed");
  ERROR_CHECK(this->write_byte(AIC3104_HPL_ROUTE, 0x08), "Set HPL_ROUTE failed");
  ERROR_CHECK(this->write_byte(AIC3104_HPR_ROUTE, 0x08), "Set HPR_ROUTE failed");
  ERROR_CHECK(this->write_byte(AIC3104_LOL_ROUTE, 0x08), "Set LOL_ROUTE failed");
  ERROR_CHECK(this->write_byte(AIC3104_LOR_ROUTE, 0x08), "Set LOR_ROUTE failed");
  ERROR_CHECK(this->write_byte(AIC3104_PAGE_CTRL, 0x00), "Restore page 0 failed");
  // Set both analog output paths to the maximum level documented by Seeed.
  // This covers the board's headphone and JST amplifier routes.
  ERROR_CHECK(this->write_byte(AIC3104_HPLOUT_LEVEL, 0x0D), "Set HPLOUT level failed");
  ERROR_CHECK(this->write_byte(AIC3104_HPROUT_LEVEL, 0x0D), "Set HPROUT level failed");
  ERROR_CHECK(this->write_byte(AIC3104_LEFT_LOP_LEVEL, 0x0B), "Set LEFT_LOP level failed");
  ERROR_CHECK(this->write_byte(AIC3104_RIGHT_LOP_LEVEL, 0x0B), "Set RIGHT_LOP level failed");
  ERROR_CHECK(this->write_byte(AIC3104_HPL_GAIN, 0x3E), "Set HPL_GAIN failed");
  ERROR_CHECK(this->write_byte(AIC3104_HPR_GAIN, 0x3E), "Set HPR_GAIN failed");
  ERROR_CHECK(this->write_byte(AIC3104_LOL_DRV_GAIN, 0x00), "Set LOL_DRV_GAIN failed");
  ERROR_CHECK(this->write_byte(AIC3104_LOR_DRV_GAIN, 0x00), "Set LOR_DRV_GAIN failed");
  ERROR_CHECK(this->write_byte(AIC3104_OP_PWR_CTRL, 0x3C), "Enable output drivers failed");

  this->set_timeout(2500, [this]() {
    ERROR_CHECK(this->write_byte(AIC3104_DAC_CH_SET1, 0xD4), "Enable DAC channels failed");
    ERROR_CHECK(this->write_volume_(), "Set volume failed");
    ERROR_CHECK(this->write_mute_(), "Set mute failed");
    ERROR_CHECK(this->write_byte(AIC3104_PAGE_CTRL, 0x00), "Set page 0 failed");
    auto hp_left = this->read_byte(AIC3104_HPLOUT_LEVEL);
    auto hp_right = this->read_byte(AIC3104_HPROUT_LEVEL);
    auto line_left = this->read_byte(AIC3104_LEFT_LOP_LEVEL);
    auto line_right = this->read_byte(AIC3104_RIGHT_LOP_LEVEL);
    ESP_LOGI(TAG, "AIC3104 output levels: HP=0x%02X/0x%02X LOP=0x%02X/0x%02X",
             hp_left.value_or(0xFF), hp_right.value_or(0xFF), line_left.value_or(0xFF),
             line_right.value_or(0xFF));
  });
}

void AIC3104::dump_config() {
  ESP_LOGCONFIG(TAG, "AIC3104:");
  LOG_I2C_DEVICE(this);

  if (this->is_failed()) {
    ESP_LOGE(TAG, ESP_LOG_MSG_COMM_FAIL);
  }
}

bool AIC3104::set_mute_off() {
  this->is_muted_ = false;
  return this->write_volume_();
}

bool AIC3104::set_mute_on() {
  this->is_muted_ = true;
  return this->write_volume_();
}

bool AIC3104::set_volume(float volume) {
  this->volume_ = clamp<float>(volume, 0.0, 1.0);
  ESP_LOGD(TAG, "AIC3104 set_volume called: %.2f", this->volume_);
  bool result = this->write_volume_();
  ESP_LOGD(TAG, "AIC3104 write_volume result: %s", result ? "SUCCESS" : "FAILED");
  return result;
}

bool AIC3104::is_muted() { return this->is_muted_; }

float AIC3104::volume() { return this->volume_; }

bool AIC3104::write_mute_() {
  return this->write_volume_();
}

bool AIC3104::write_volume_() {
  if (!this->write_byte(AIC3104_PAGE_CTRL, 0x00)) {
    ESP_LOGE(TAG, "Failed to set page 0");
    return false;
  }

  // Map ESPHome's quiet-to-loud volume direction onto the board's 0-17 range.
  // The DAC attenuation uses 0.5 dB steps; the upper half adds output gain.
  const uint8_t level = static_cast<uint8_t>(this->volume_ * 17.0f + 0.5f);
  const uint8_t dac_val = this->is_muted_ ? 0x80 : (level <= 8 ? (8 - level) * 9 : 0);
  const uint8_t output_level = level <= 8 ? 0x0D : ((level - 8) << 4) | 0x0B;

  if (!this->write_byte(AIC3104_LEFT_DAC_VOLUME, dac_val) ||
      !this->write_byte(AIC3104_RIGHT_DAC_VOLUME, dac_val) ||
      !this->write_byte(AIC3104_HPLOUT_LEVEL, output_level) ||
      !this->write_byte(AIC3104_HPROUT_LEVEL, output_level) ||
      !this->write_byte(AIC3104_LEFT_LOP_LEVEL, output_level) ||
      !this->write_byte(AIC3104_RIGHT_LOP_LEVEL, output_level)) {
    ESP_LOGE(TAG, "Writing AIC3104 volume failed");
    return false;
  }

  ESP_LOGD(TAG, "Volume %.1f%% -> level %u, DAC 0x%02X, output 0x%02X%s",
           this->volume_ * 100.0f, level, dac_val, output_level,
           this->is_muted_ ? " (muted)" : "");
  return true;
}

}  // namespace aic3104
}  // namespace esphome
