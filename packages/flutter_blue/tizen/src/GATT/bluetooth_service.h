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

 public:

  virtual proto::gen::BluetoothService toProtoService() const noexcept = 0;

  virtual BluetoothDeviceController const& cDevice() const noexcept = 0;

  virtual ServiceType getType() const noexcept = 0;

  std::string UUID() const noexcept;

  BluetoothCharacteristic* getCharacteristic(const std::string& uuid);

 protected:

  bt_gatt_h handle_;

  std::vector<std::unique_ptr<BluetoothCharacteristic>> _characteristics;

  BluetoothService(bt_gatt_h handle);

  BluetoothService(const BluetoothService&) = default;

  virtual ~BluetoothService();
};


class PrimaryService : public BluetoothService {

public:

  PrimaryService(bt_gatt_h handle, BluetoothDeviceController& device);

  PrimaryService(const PrimaryService&) = default;

  ~PrimaryService();

  BluetoothDeviceController const& cDevice() const noexcept override;

  proto::gen::BluetoothService toProtoService() const noexcept override;

  ServiceType getType() const noexcept override;

  SecondaryService* getSecondary(const std::string& uuid) noexcept;

private:

  BluetoothDeviceController& device_;

  std::vector<std::unique_ptr<SecondaryService>> secondaryServices_;
};

class SecondaryService : public BluetoothService {

public:
  SecondaryService(bt_gatt_h service_handle, PrimaryService& primaryService);

  SecondaryService(const SecondaryService&) = default;

  ~SecondaryService();

  BluetoothDeviceController const& cDevice() const noexcept override;

  PrimaryService const& cPrimary() const noexcept;

  proto::gen::BluetoothService toProtoService() const noexcept override;

  ServiceType getType() const noexcept override;

  std::string primaryUUID() noexcept;
  
private:

  PrimaryService& primaryService_;
};

}  // namespace btGatt
}  // namespace flutter_blue_tizen
#endif  // FLUTTER_BLUE_TIZEN_GATT_BLEUTOOTH_SERVICE_H
