#include "bluetooth_device_controller.h"

#include <bluetooth.h>

#include <array>
#include <mutex>
#include <string>
#include <string_view>
#include <utility>

#include "bluetooth_manager.h"
#include "bluetooth_type.h"
#include "log.h"
#include "tizen_error.h"
#include "utils.h"

namespace flutter_blue_tizen {

using State = BluetoothDeviceController::State;

BluetoothDeviceController::BluetoothDeviceController(
    const std::string& name, const std::string& address) noexcept
    : name_(name), address_(address) {
  std::scoped_lock lock(active_devices_.mutex_);
  active_devices_.var_.insert({address, this});
}

BluetoothDeviceController::~BluetoothDeviceController() noexcept {
  std::scoped_lock lock(active_devices_.mutex_);
  Disconnect();
  active_devices_.var_.erase(address());
}

std::string BluetoothDeviceController::name() const noexcept { return name_; }

std::string BluetoothDeviceController::address() const noexcept {
  return address_;
}

State BluetoothDeviceController::GetState() const noexcept {
  if (is_connecting_ || is_disconnecting_) {
    return (is_connecting_ ? State::kConnecting : State::kDisconnecting);
  } else {
    bool is_connected = false;
    int ret = bt_device_is_profile_connected(address_.c_str(), BT_PROFILE_GATT,
                                             &is_connected);
    return (is_connected ? State::kConnected : State::kDisconnected);
  }
}

void BluetoothDeviceController::Connect(bool auto_connect) {
  std::unique_lock lock(operation_mutex_);
  auto_connect = false;  // TODO - fix. API fails when autoconnect==true
  if (GetState() == State::kDisconnected) {
    is_connecting_ = true;
    int ret = bt_gatt_connect(address_.c_str(), auto_connect);
    LOG_ERROR("bt_gatt_connect %s", get_error_message(ret));
  } else {
    throw BtException("Device is not disconnected.");
  }
}

void BluetoothDeviceController::Disconnect() {
  std::scoped_lock lock(operation_mutex_);
  if (GetState() == State::kConnected) {
    services_.clear();
    is_disconnecting_ = true;
    int ret = bt_gatt_disconnect(address_.c_str());
    LOG_ERROR("bt_gatt_disconnect %s", get_error_message(ret));
  }
}

void BluetoothDeviceController::DiscoverServices() {
  std::scoped_lock lock(operation_mutex_);
  services_.clear();

  struct Scope {
    BluetoothDeviceController& device;
    std::vector<std::unique_ptr<btGatt::PrimaryService>>& services;
  };

  // bt_gatt_client_foreach_services is executed synchronously.
  // There's no need to allocate Scope on heap.
  Scope scope{*this, services_};
  int ret = bt_gatt_client_foreach_services(
      GetGattClient(address_),
      [](int total, int index, bt_gatt_h service_handle,
         void* scope_ptr) -> bool {
        auto& scope = *static_cast<Scope*>(scope_ptr);
        scope.services.emplace_back(
            std::make_unique<btGatt::PrimaryService>(service_handle));
        return true;
      },
      &scope);

  LOG_ERROR("bt_gatt_client_foreach_services %s", get_error_message(ret));
}

std::vector<btGatt::PrimaryService*>
BluetoothDeviceController::GetServices() noexcept {
  auto services = std::vector<btGatt::PrimaryService*>();
  for (const auto& service : services_) {
    services.emplace_back(service.get());
  }
  return services;
}

btGatt::PrimaryService* BluetoothDeviceController::GetService(
    const std::string& uuid) noexcept {
  for (const auto& service : services_) {
    if (service->Uuid() == uuid) return service.get();
  }
  return nullptr;
}

uint32_t BluetoothDeviceController::GetMtu() const {
  uint32_t mtu = -1;
  auto ret = bt_gatt_client_get_att_mtu(GetGattClient(address_), &mtu);
  LOG_ERROR("bt_gatt_client_get_att_mtu %s", get_error_message(ret));

  if (ret) throw BtException(ret, "could not get mtu of the device!");
  return mtu;
}

void BluetoothDeviceController::RequestMtu(uint32_t mtu,
                                           const RequestMtuCallback& callback) {
  struct Scope {
    const std::string address;
    RequestMtuCallback callback;
  };
  auto scope = new Scope{address_, callback};

  auto ret = bt_gatt_client_set_att_mtu_changed_cb(
      GetGattClient(address_),
      [](bt_gatt_client_h client, const bt_gatt_client_att_mtu_info_s* mtu_info,
         void* scope_ptr) {
        auto scope = static_cast<Scope*>(scope_ptr);
        std::scoped_lock lock(active_devices_.mutex_);
        auto it = active_devices_.var_.find(scope->address);
        if (it != active_devices_.var_.end())
          scope->callback(mtu_info->status == 0, *it->second);
        delete scope;
      },
      scope);

  LOG_ERROR("bt_gatt_client_set_att_mtu_changed_cb %s", get_error_message(ret));

  if (ret) throw BtException(ret, "bt_gatt_client_set_att_mtu_changed_cb");

  ret = bt_gatt_client_request_att_mtu_change(GetGattClient(address_), mtu);

  LOG_ERROR("bt_gatt_client_request_att_mtu_change %s", get_error_message(ret));

  if (ret) throw BtException(ret, "bt_gatt_client_request_att_mtu_change");
}

void BluetoothDeviceController::ReadRssi(ReadRssiCallback callback) {

  BleScanSettings settings;
  settings.device_ids_filters_ = {address()};

  BluetoothManager::StartBluetoothDeviceScanLE(
      settings, [dest_address = address(), callback = std::move(callback)](const std::string& address, const std::string& device_name,
                   int rssi, const AdvertisementData& advertisement_data) {
	  	
	  		std::scoped_lock lock(active_devices_.mutex_);

	  		auto it = active_devices_.var_.find(address);

			if(address == dest_address && it != active_devices_.var_.end()){
				auto device = it->second;
				
				callback(*device, rssi);
			}
      });
}

void BluetoothDeviceController::SetConnectionStateChangedCallback(
    ConnectionStateChangedCallback connection_changed_callback) {
  connection_changed_callback_ = std::move(connection_changed_callback);

  int ret = bt_gatt_set_connection_state_changed_cb(
      [](int ret, bool connected, const char* remote_address, void* user_data) {
        LOG_ERROR("bt_gatt_connection_state_changed_cb %s",
                  get_error_message(ret));
        std::scoped_lock lock(active_devices_.mutex_);
        auto it = active_devices_.var_.find(remote_address);

        if (it != active_devices_.var_.end()) {
          auto device = it->second;
          std::scoped_lock operation_lock(device->operation_mutex_);
          device->is_connecting_ = false;
          device->is_disconnecting_ = false;
          if (!connected) {
            device->services_.clear();
          } else if (!ret) {
            // Creates gatt client if it does not exist.
            GetGattClient(device->address());
          }
          connection_changed_callback_(
              connected ? State::kConnected : State::kDisconnected, device);
        }
        if (!connected) {
          DestroyGattClientIfExists(remote_address);
        }
      },
      nullptr);

  if (ret != 0) {
    LOG_ERROR("[bt_gatt_set_connection_state_changed_cb] %s",
              get_error_message(ret));
    return;
  }
}

bt_gatt_client_h BluetoothDeviceController::GetGattClient(
    const std::string& address) {
  std::scoped_lock lock(gatt_clients_.mutex_);

  auto it = gatt_clients_.var_.find(address);
  bt_gatt_client_h client = nullptr;

  if (it == gatt_clients_.var_.end()) {
    int ret = bt_gatt_client_create(address.c_str(), &client);
    LOG_ERROR("bt_gatt_client_create %s", get_error_message(ret));

    if ((ret == BT_ERROR_NONE || ret == BT_ERROR_ALREADY_DONE) && client) {
      gatt_clients_.var_.emplace(address, client);
    } else {
      throw BtException(ret, "bt_gatt_client_create");
    }
  } else {
    client = it->second;
  }
  return client;
}

void BluetoothDeviceController::DestroyGattClientIfExists(
    const std::string& address) noexcept {
  std::scoped_lock lock(gatt_clients_.mutex_);
  auto it = gatt_clients_.var_.find(address);
  if (it != gatt_clients_.var_.end()) {
    auto ret = bt_gatt_client_destroy(it->second);
    if (!ret) gatt_clients_.var_.erase(address);
    LOG_ERROR("bt_gatt_client_destroy %s", get_error_message(ret));
  }
}

}  // namespace flutter_blue_tizen
