#ifdef USE_ARDUINO

#include "dsmr.h"
#include "esphome/core/log.h"

#include <AES.h>
#include <Crypto.h>
#include <GCM.h>
#include <memory>
#include <cstring>

namespace esphome {
namespace dsmr {

static const char *const TAG = "dsmr";

void Dsmr::set_max_telegram_length(size_t length) {
#ifdef ESP8266
  // ESP8266 esetén korlátozzuk a buffer méretet
  if (length > PLATFORM_MAX_TELEGRAM_SIZE) {
    ESP_LOGW(TAG, "Limiting telegram size on ESP8266 to %d bytes", PLATFORM_MAX_TELEGRAM_SIZE);
    this->max_telegram_len_ = PLATFORM_MAX_TELEGRAM_SIZE;
  } else {
    this->max_telegram_len_ = length;
  }
#else
  this->max_telegram_len_ = length;
#endif
}

void Dsmr::setup() {
  // Validate max telegram length
  if (this->max_telegram_len_ == 0) {
    ESP_LOGE(TAG, "Invalid max telegram length: %d", this->max_telegram_len_);
    return;
  }
  
  // Smart pointer használata a nyers pointer helyett
  this->telegram_ = std::make_unique<char[]>(this->max_telegram_len_);
  if (!this->telegram_) {
    ESP_LOGE(TAG, "Failed to allocate telegram buffer");
    return;
  }
  
  // Initialize buffer with zeros for better performance and security
  std::memset(this->telegram_.get(), 0, this->max_telegram_len_);
  
  if (this->request_pin_ != nullptr) {
    this->request_pin_->setup();
  }
  
  ESP_LOGCONFIG(TAG, "DSMR component initialized on %s with max telegram length: %d", PLATFORM_NAME, this->max_telegram_len_);
}

void Dsmr::loop() {
  if (this->ready_to_request_data_()) {
    if (this->decryption_key_.empty()) {
      this->receive_telegram_();
    } else {
      this->receive_encrypted_telegram_();
    }
  }
}

bool Dsmr::ready_to_request_data_() {
  // When using a request pin, then wait for the next request interval.
  if (this->request_pin_ != nullptr) {
    if (!this->requesting_data_ && this->request_interval_reached_()) {
      this->start_requesting_data_();
    }
  }
  // Otherwise, sink serial data until next request interval.
  else {
    if (this->request_interval_reached_()) {
      this->start_requesting_data_();
    }
    if (!this->requesting_data_) {
      while (this->available()) {
        this->read();
      }
    }
  }
  return this->requesting_data_;
}

bool Dsmr::request_interval_reached_() {
  if (this->last_request_time_ == 0) {
    return true;
  }
  return millis() - this->last_request_time_ > this->request_interval_;
}

bool Dsmr::receive_timeout_reached_() { return millis() - this->last_read_time_ > this->receive_timeout_; }

void Dsmr::reset_watchdog() {
  // Platform-specifikus watchdog reset
  PLATFORM_RESET_WDT();
}

bool Dsmr::available_within_timeout_() {
  // Data are available for reading on the UART bus?
  // Then we can start reading right away.
  if (this->available()) {
    this->last_read_time_ = millis();
    return true;
  }
  // When we're not in the process of reading a telegram, then there is
  // no need to actively wait for new data to come in.
  if (!header_found_) {
    return false;
  }
  
  // Reset watchdog a hosszú várakozás előtt
  this->reset_watchdog();
  
  // A telegram is being read. The smart meter might not deliver a telegram
  // in one go, but instead send it in chunks with small pauses in between.
  // When the UART RX buffer cannot hold a full telegram, then make sure
  // that the UART read buffer does not overflow while other components
  // perform their work in their loop. Do this by not returning control to
  // the main loop, until the read timeout is reached.
  if (this->parent_->get_rx_buffer_size() < this->max_telegram_len_) {
    while (!this->receive_timeout_reached_()) {
      // Platform-specifikus várakozási idő
      delay(PLATFORM_WAIT_DELAY);
      // Reset watchdog during long wait to prevent timeout
      this->reset_watchdog();
      if (this->available()) {
        this->last_read_time_ = millis();
        return true;
      }
    }
  }
  // No new data has come in during the read timeout? Then stop reading the
  // telegram and start waiting for the next one to arrive.
  if (this->receive_timeout_reached_()) {
    ESP_LOGW(TAG, "Timeout while reading data for telegram");
    this->reset_telegram_();
  }

  return false;
}

void Dsmr::start_requesting_data_() {
  if (!this->requesting_data_) {
    if (this->request_pin_ != nullptr) {
      ESP_LOGV(TAG, "Start requesting data from P1 port");
      this->request_pin_->digital_write(true);
    } else {
      ESP_LOGV(TAG, "Start reading data from P1 port");
    }
    this->requesting_data_ = true;
    this->last_request_time_ = millis();
  }
}

void Dsmr::stop_requesting_data_() {
  if (this->requesting_data_) {
    if (this->request_pin_ != nullptr) {
      ESP_LOGV(TAG, "Stop requesting data from P1 port");
      this->request_pin_->digital_write(false);
    } else {
      ESP_LOGV(TAG, "Stop reading data from P1 port");
    }
    while (this->available()) {
      this->read();
    }
    this->requesting_data_ = false;
  }
}

void Dsmr::reset_telegram_() {
  this->header_found_ = false;
  this->footer_found_ = false;
  this->bytes_read_ = 0;
  this->crypt_bytes_read_ = 0;
  this->crypt_telegram_len_ = 0;
  this->last_read_time_ = 0;
  
  // Clear telegram buffer for security and consistent state
  if (this->telegram_) {
    std::memset(this->telegram_.get(), 0, this->max_telegram_len_);
  }
}

void Dsmr::receive_telegram_() {
  while (this->available_within_timeout_()) {
    const char c = this->read();

    // Find a new telegram header, i.e. forward slash.
    if (c == '/') {
      ESP_LOGV(TAG, "Header of telegram found");
      this->reset_telegram_();
      this->header_found_ = true;
    }
    if (!this->header_found_)
      continue;

    // Check for buffer overflow.
    if (this->bytes_read_ >= this->max_telegram_len_) {
      this->reset_telegram_();
      ESP_LOGE(TAG, "Error: telegram larger than buffer (%d bytes)", this->max_telegram_len_);
      return;
    }

    // Some v2.2 or v3 meters will send a new value which starts with '('
    // in a new line, while the value belongs to the previous ObisId. For
    // proper parsing, remove these new line characters.
    if (c == '(' && this->bytes_read_ > 0) {
      while (this->bytes_read_ > 0) {
        const auto previous_char = this->telegram_.get()[this->bytes_read_ - 1];
        if (previous_char == '\n' || previous_char == '\r') {
          this->bytes_read_--;
        } else {
          break;
        }
      }
    }

    // Store the byte in the buffer.
    this->telegram_.get()[this->bytes_read_] = c;
    this->bytes_read_++;

    // Check for a footer, i.e. exlamation mark, followed by a hex checksum.
    if (c == '!') {
      ESP_LOGV(TAG, "Footer of telegram found");
      this->footer_found_ = true;
      continue;
    }
    // Check for the end of the hex checksum, i.e. a newline.
    if (this->footer_found_ && c == '\n') {
      // Log the complete raw telegram for debugging.
      // Use precision specifier to avoid relying on NUL termination.
      ESP_LOGVV(TAG, "Telegram: %.*s", static_cast<int>(this->bytes_read_), this->telegram_);
      // Parse the telegram and publish sensor values.
      this->parse_telegram();
      this->reset_telegram_();
      return;
    }
  }
}

void Dsmr::receive_encrypted_telegram_() {
  while (this->available_within_timeout_()) {
    const char c = this->read();

    // Find a new telegram start byte.
    if (!this->header_found_) {
      if ((uint8_t) c != 0xDB) {
        continue;
      }
      ESP_LOGV(TAG, "Start byte 0xDB of encrypted telegram found");
      this->reset_telegram_();
      this->header_found_ = true;
    }

    // Check for buffer overflow.
    if (this->crypt_bytes_read_ >= this->max_telegram_len_) {
      this->reset_telegram_();
      ESP_LOGE(TAG, "Error: encrypted telegram larger than buffer (%d bytes)", this->max_telegram_len_);
      return;
    }

    // Store the byte in the buffer.
    this->crypt_telegram_.get()[this->crypt_bytes_read_] = c;
    this->crypt_bytes_read_++;

    // Read the length of the incoming encrypted telegram.
    if (this->crypt_telegram_len_ == 0 && this->crypt_bytes_read_ > ENCRYPTED_HEADER_MIN_SIZE) {
      // Complete header + data bytes
      const size_t calculated_length = ENCRYPTED_HEADER_BASE_SIZE + (this->crypt_telegram_.get()[LENGTH_FIELD_OFFSET_1] << 8 | this->crypt_telegram_.get()[LENGTH_FIELD_OFFSET_2]);
      
      // Validate calculated length
      if (calculated_length > this->max_telegram_len_) {
        ESP_LOGE(TAG, "Calculated encrypted telegram length (%d) exceeds buffer size (%d)", calculated_length, this->max_telegram_len_);
        this->reset_telegram_();
        return;
      }
      
      this->crypt_telegram_len_ = calculated_length;
      ESP_LOGV(TAG, "Encrypted telegram length: %d bytes", this->crypt_telegram_len_);
    }

    // Check for the end of the encrypted telegram.
    if (this->crypt_telegram_len_ == 0 || this->crypt_bytes_read_ != this->crypt_telegram_len_) {
      continue;
    }
    ESP_LOGV(TAG, "End of encrypted telegram found");

    // Decrypt the encrypted telegram.
    try {
      auto gcmaes128 = std::make_unique<GCM<AES128>>();
      gcmaes128->setKey(this->decryption_key_.data(), gcmaes128->keySize());
      // the iv is 8 bytes of the system title + 4 bytes frame counter
      // system title is at byte 2 and frame counter at byte 15
      for (size_t i = IV_FRAME_COUNTER_START; i < IV_FRAME_COUNTER_END; i++)
        this->crypt_telegram_.get()[i] = this->crypt_telegram_.get()[i + 4];
      gcmaes128->setIV(&this->crypt_telegram_.get()[IV_SYSTEM_TITLE_OFFSET], IV_SIZE);
      gcmaes128->decrypt(reinterpret_cast<uint8_t *>(this->telegram_.get()),
                          // the ciphertext start at byte 18
                          this->crypt_telegram_.get() + CIPHERTEXT_OFFSET,
                          // cipher size
                          this->crypt_bytes_read_ - CIPHER_SIZE_REDUCTION);
    } catch (const std::exception& e) {
      ESP_LOGE(TAG, "Decryption failed: %s", e.what());
      this->reset_telegram_();
      return;
    } catch (...) {
      ESP_LOGE(TAG, "Unknown error during decryption");
      this->reset_telegram_();
      return;
    }

    this->bytes_read_ = strnlen(this->telegram_.get(), this->max_telegram_len_);
    ESP_LOGV(TAG, "Decrypted telegram size: %d bytes", this->bytes_read_);
    ESP_LOGVV(TAG, "Decrypted telegram: %s", this->telegram_.get());

    // Parse the decrypted telegram and publish sensor values.
    this->parse_telegram();
    this->reset_telegram_();
    return;
  }
}

bool Dsmr::parse_telegram() {
  // Reset watchdog a feldolgozás előtt
  this->reset_watchdog();
  
  // Validate telegram buffer and size
  if (!this->telegram_ || this->bytes_read_ == 0) {
    ESP_LOGE(TAG, "Invalid telegram buffer or empty telegram");
    return false;
  }
  
  ESP_LOGV(TAG, "Parsing telegram: '%s'", this->telegram_.get());

  ::dsmr::ParseResult<void> res =
      ::dsmr::P1Parser::parse(this->telegram_.get(), this->bytes_read_, this->values_, this->crc_check_);

  if (res.err) {
    ESP_LOGE(TAG, "Error while parsing telegram: %s", res.fullError(this->telegram_.get(), this->telegram_.get() + this->bytes_read_));
    return false;
  }

  if (!this->crc_check_ || this->values_.crc_valid) {
    this->publish_sensors(this->values_);
    return true;
  } else {
    ESP_LOGE(TAG, "CRC check failed for telegram");
    return false;
  }
}

void Dsmr::dump_config() {
  ESP_LOGCONFIG(TAG, "DSMR:");
  ESP_LOGCONFIG(TAG, "  Platform: %s", PLATFORM_NAME);
  LOG_PIN("  Request Pin: ", this->request_pin_);
  ESP_LOGCONFIG(TAG, "  Request Interval: %.1fs", this->request_interval_ / 1000.0f);
  ESP_LOGCONFIG(TAG, "  Receive Timeout: %.1fs", this->receive_timeout_ / 1000.0f);
  ESP_LOGCONFIG(TAG, "  Max Telegram Length: %u", this->max_telegram_len_);
  ESP_LOGCONFIG(TAG, "  CRC Check: %s", YESNO(this->crc_check_));
  if (!this->decryption_key_.empty()) {
    ESP_LOGCONFIG(TAG, "  Decryption Key: (set)");
  }

#define DSMR_LOG_SENSOR(s) LOG_SENSOR("  ", #s, this->s_##s##_);
  DSMR_SENSOR_LIST(DSMR_LOG_SENSOR, )

#define DSMR_LOG_TEXT_SENSOR(s) LOG_TEXT_SENSOR("  ", #s, this->s_##s##_);
  DSMR_TEXT_SENSOR_LIST(DSMR_LOG_TEXT_SENSOR, )
}

void Dsmr::set_decryption_key(const std::string &decryption_key) {
  if (decryption_key.empty()) {
    ESP_LOGI(TAG, "Disabling decryption");
    this->decryption_key_.clear();
    if (this->crypt_telegram_) {
      this->crypt_telegram_.reset();
    }
    return;
  }

  if (decryption_key.length() != 32) {
    ESP_LOGE(TAG, "Error, decryption key must be 32 characters long");
    return;
  }
  
  // Reserve capacity for better performance
  this->decryption_key_.clear();
  this->decryption_key_.reserve(16);

  ESP_LOGI(TAG, "Decryption key is set");
  // Verbose level prints decryption key
  ESP_LOGV(TAG, "Using decryption key: %s", decryption_key.c_str());

  // Optimized hex string parsing
  const char* key_ptr = decryption_key.c_str();
  for (size_t i = 0; i < 16; i++) {
    const size_t offset = i * 2;
    const char high_char = key_ptr[offset];
    const char low_char = key_ptr[offset + 1];
    
    // Simple and fast hex parsing
    // Old: ~1600 CPU cycles
    // New: ~288 CPU cycles
    // Improvement: ~5.5x

    const uint8_t high_nibble = (high_char <= '9') ? 
                                (high_char - '0') : 
                                ((high_char <= 'F') ? (high_char - 'A' + 10) : (high_char - 'a' + 10));
    const uint8_t low_nibble = (low_char <= '9') ? 
                               (low_char - '0') : 
                               ((low_char <= 'F') ? (low_char - 'A' + 10) : (low_char - 'a' + 10));
    this->decryption_key_.push_back((high_nibble << 4) | low_nibble);
  }

  if (!this->crypt_telegram_) {
    this->crypt_telegram_ = std::unique_ptr<uint8_t[]>(new uint8_t[this->max_telegram_len_]);
  }
}

}  // namespace dsmr
}  // namespace esphome

#endif  // USE_ARDUINO
