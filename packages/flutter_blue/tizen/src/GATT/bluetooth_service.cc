#include <GATT/bluetooth_characteristic.h>
#include <GATT/bluetooth_service.h>
#include <bluetooth.h>
#include <bluetooth_device_controller.h>
#include <log.h>

namespace flutter_blue_tizen {
namespace btGatt {

BluetoothService::BluetoothService(bt_gatt_h handle) : _handle(handle) {
  int res = bt_gatt_service_foreach_characteristics(
      handle,
      [](int total, int index, bt_gatt_h handle, void* scope_ptr) -> bool {
        auto& service = *static_cast<BluetoothService*>(scope_ptr);
        service._characteristics.emplace_back(
            std::make_unique<BluetoothCharacteristic>(handle, service));
        return true;
      },
      this);

  LOG_ERROR("bt_gatt_service_foreach_characteristics", get_error_message(res));
}

PrimaryService::PrimaryService(bt_gatt_h handle,
                               BluetoothDeviceController& device)
    : BluetoothService(handle), _device(device) {
  int res = bt_gatt_service_foreach_included_services(
      handle,
      [](int total, int index, bt_gatt_h handle, void* scope_ptr) -> bool {
        auto& service = *static_cast<PrimaryService*>(scope_ptr);
        service._secondaryServices.emplace_back(
            std::make_unique<SecondaryService>(handle, service));
        return true;
      },
      this);
  LOG_ERROR("bt_gatt_service_foreach_included_services",
            get_error_message(res));
}

SecondaryService::SecondaryService(bt_gatt_h service_handle,
                                   PrimaryService& primaryService)
    : BluetoothService(service_handle), _primaryService(primaryService) {}

BluetoothDeviceController const& PrimaryService::cDevice() const noexcept {
  return _device;
}

proto::gen::BluetoothService PrimaryService::toProtoService() const noexcept {
  proto::gen::BluetoothService proto;
  proto.set_remote_id(_device.cAddress());
  proto.set_uuid(UUID());
  proto.set_is_primary(true);
  for (const auto& characteristic : _characteristics) {
    *proto.add_characteristics() = characteristic->toProtoCharacteristic();
  }
  for (const auto& secondary : _secondaryServices) {
    *proto.add_included_services() = secondary->toProtoService();
  }
  return proto;
}

ServiceType PrimaryService::getType() const noexcept {
  return ServiceType::PRIMARY;
}

BluetoothDeviceController const& SecondaryService::cDevice() const noexcept {
  return _primaryService.cDevice();
}

PrimaryService const& SecondaryService::cPrimary() const noexcept {
  return _primaryService;
}

proto::gen::BluetoothService SecondaryService::toProtoService() const noexcept {
  proto::gen::BluetoothService proto;
  proto.set_remote_id(_primaryService.cDevice().cAddress());
  proto.set_uuid(UUID());
  proto.set_is_primary(false);
  for (const auto& characteristic : _characteristics) {
    *proto.add_characteristics() = characteristic->toProtoCharacteristic();
  }
  return proto;
}

ServiceType SecondaryService::getType() const noexcept {
  return ServiceType::SECONDARY;
}

std::string SecondaryService::primaryUUID() noexcept {
  return _primaryService.UUID();
}

std::string BluetoothService::UUID() const noexcept {
  return getGattUUID(_handle);
}

BluetoothCharacteristic* BluetoothService::getCharacteristic(
    const std::string& uuid) {
  for (auto& c : _characteristics) {
    if (c->UUID() == uuid) return c.get();
  }
  return nullptr;
}

SecondaryService* PrimaryService::getSecondary(
    const std::string& uuid) noexcept {
  for (auto& s : _secondaryServices) {
    if (s->UUID() == uuid) return s.get();
  }
  return nullptr;
}

BluetoothService::~BluetoothService() {}

/**
 *
 * these must not be in a virtual destructor. Characteristic references abstract
 * method, when derived objects are already destroyed otherwise.
 *
 */
PrimaryService::~PrimaryService() { _characteristics.clear(); }

SecondaryService::~SecondaryService() { _characteristics.clear(); }

}  // namespace btGatt

}  // namespace flutter_blue_tizen