#ifndef FLUTTER_BLUE_PLUS_TIZEN_GATT_BLEUTOOTH_SERVICE_H
#define FLUTTER_BLUE_PLUS_TIZEN_GATT_BLEUTOOTH_SERVICE_H

#include <bluetooth.h>

#include <memory>
#include <string>
#include <vector>

#include "GATT/bluetooth_characteristic.h"

namespace flutter_blue_plus_tizen::btGatt {

class SecondaryService;

class BluetoothService {
 public:
  virtual bool IsPrimary() const noexcept = 0;

  std::string Uuid() const noexcept;

  BluetoothCharacteristic* GetCharacteristic(
      const std::string& uuid) const noexcept;

  std::vector<BluetoothCharacteristic*> GetCharacteristics() const noexcept;

 protected:
  bt_gatt_h handle_;

  std::vector<std::unique_ptr<BluetoothCharacteristic>> characteristics_;

  explicit BluetoothService(bt_gatt_h handle);

  BluetoothService(const BluetoothService&) = delete;

  BluetoothService(BluetoothService&&) = default;

  virtual ~BluetoothService() = default;
};

class PrimaryService : public BluetoothService {
 public:
  explicit PrimaryService(bt_gatt_h handle);

  ~PrimaryService();

  bool IsPrimary() const noexcept override;

  SecondaryService* GetSecondary(const std::string& uuid) const noexcept;

  std::vector<SecondaryService*> getSecondaryServices() const noexcept;

 private:
  std::vector<std::unique_ptr<SecondaryService>> secondary_services_;
};

class SecondaryService : public BluetoothService {
 public:
  SecondaryService(bt_gatt_h service_handle,
                   const PrimaryService& primary_service);

  ~SecondaryService();

  bool IsPrimary() const noexcept override;

  std::string PrimaryUuid() const noexcept;

 private:
  const PrimaryService& primary_service_;
};

}  // namespace flutter_blue_plus_tizen::btGatt
#endif  // FLUTTER_BLUE_PLUS_TIZEN_GATT_BLEUTOOTH_SERVICE_H
