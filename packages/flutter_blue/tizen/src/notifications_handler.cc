#include "notifications_handler.h"

#include "state_handler.h"

namespace flutter_blue_tizen {

NotificationsHandler::NotificationsHandler(
    std::shared_ptr<MethodChannel> method_channel)
    : method_channel_(method_channel) {}

void NotificationsHandler::NotifyUIThread(
    const std::string& method,
    const google::protobuf::MessageLite& message) const noexcept {
  std::vector<uint8_t> encodable(message.ByteSizeLong());
  message.SerializeToArray(encodable.data(), message.ByteSizeLong());
  method_channel_->InvokeMethod(
      method, std::make_unique<flutter::EncodableValue>(encodable));
}
}  // namespace flutter_blue_tizen
