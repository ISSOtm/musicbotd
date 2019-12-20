
#include "../client_connection.hpp"


v1Conversation::v1Conversation()
 : _type(NONE) {}

static std::array const handlers{
    std::map{ // NONE
        std::pair{ v1Conversation::ClientPacketType::PULSE, +[](nlohmann::json const &){
            return std::pair(v1Conversation::Status::FINISHED, 1u);
        }}
    }
};

v1Conversation::Status v1Conversation::_handlePacket(nlohmann::json const & packet) {
    return processStateMachine(handlers, ClientPacketType(packet["type"].get<unsigned>()), packet);
}
