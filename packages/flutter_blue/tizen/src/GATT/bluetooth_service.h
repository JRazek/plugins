#ifndef FLUTTER_BLUE_TIZEN_GATT_BLEUTOOTH_SERVICE_H
#define FLUTTER_BLUE_TIZEN_GATT_BLEUTOOTH_SERVICE_H

#include <GATT/bluetooth_characteristic.h>
#include <bluetooth.h>
#include <flutterblue.pb.h>

#include <vector>

namespace flutter_blue_tizen {

class BluetoothDeviceController;

namespace btGatt {

enum class ServiceType {
  PRIMARY,
  SECONDARY,
};

class SecondaryService;

class BluetoothService {
 protected:
  bt_gatt_h _handle;

  std::vector<std::unique_ptr<BluetoothCharacteristic>> _characteristics;

  BluetoothService(bt_gatt_h handle);

  BluetoothService(const BluetoothService&) = default;

  virtual ~BluetoothService();

 public:
  virtual proto::gen::BluetoothService toProtoService() const noexcept = 0;

  virtual BluetoothDeviceController const& cDevice() const noexcept = 0;

  virtual ServiceType getType() const noexcept = 0;

  std::string UUID() const noexcept;

  BluetoothCharacteristic* getCharacteristic(const std::string& uuid);
};

class PrimaryService : public BluetoothService {
  BluetoothDeviceController& _device;

  std::vector<std::unique_ptr<SecondaryService>> _secondaryServices;

 public:
  PrimaryService(bt_gatt_h handle, BluetoothDeviceController& device);

  PrimaryService(const PrimaryService&) = default;

  ~PrimaryService();

  BluetoothDeviceController const& cDevice() const noexcept override;

  proto::gen::BluetoothService toProtoService() const noexcept override;

  ServiceType getType() const noexcept override;

  SecondaryService* getSecondary(const std::string& uuid) noexcept;
};

class SecondaryService : public BluetoothService {
  PrimaryService& _primaryService;

 public:
  SecondaryService(bt_gatt_h service_handle, PrimaryService& primaryService);

  SecondaryService(const SecondaryService&) = default;

  ~SecondaryService();

  BluetoothDeviceController const& cDevice() const noexcept override;

  PrimaryService const& cPrimary() const noexcept;

  proto::gen::BluetoothService toProtoService() const noexcept override;

  ServiceType getType() const noexcept override;

  std::string primaryUUID() noexcept;
};

}  // namespace btGatt
}  // namespace flutter_blue_tizen
#endif  // FLUTTER_BLUE_TIZEN_GATT_BLEUTOOTH_SERVICE_H
