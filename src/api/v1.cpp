
#include "../client_connection.hpp"


v1Conversation::v1Conversation(ClientConnection & owner)
 : Conversation(owner) {}

v1Conversation::Status v1Conversation::_handlePacket(nlohmann::json const & packet) {
    std::array const handlers{
        std::map{ // NONE
            std::pair{ClientPacketType::PULSE, +[](nlohmann::json const &, ClientConnection &) {
                return std::pair(Status::FINISHED, State::NONE);
            }},
            std::pair{ClientPacketType::MUS_ADD, +[](nlohmann::json const & packet, ClientConnection & connection){
                connection.addMusic(packet["url"].get<std::string>());
                return std::pair(Status::FINISHED, State::NONE);
            }}
        }
    };

    return processStateMachine(handlers, ClientPacketType(packet["type"].get<unsigned>()), packet);
}
