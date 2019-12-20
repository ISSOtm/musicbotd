
#include "../client_connection.hpp"


v1Conversation::v1Conversation() {}

v1Conversation::Status v1Conversation::_handlePacket(nlohmann::json const & packet) {
    std::array const handlers{
        std::map{ // NONE
            std::pair{ClientPacketType::PULSE, +[](nlohmann::json const &) {
                return std::pair(Status::FINISHED, State::NONE);
            }}
        }
    };

    return processStateMachine(handlers, ClientPacketType(packet["type"].get<unsigned>()), packet);
}
