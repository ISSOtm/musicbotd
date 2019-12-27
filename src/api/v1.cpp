
#include "../client_connection.hpp"


v1Conversation::Status v1Conversation::_handlePacket(nlohmann::json const & packet) {
    std::array const handlers{
        std::map{ // NONE
            std::pair{ClientPacketType::PULSE, +[](nlohmann::json const &, ClientConnection &) {
                return std::pair(Status::FINISHED, State::NONE);
            }},

            std::pair{ClientPacketType::MUS_ADD, +[](nlohmann::json const & packet, ClientConnection & connection){
                // Parse options
                std::map<std::string, std::string> options;
                auto iter = packet.find("options");
                if (iter != packet.end()) { // The `options` object is optional
                    for (auto const & [key, value] : (*iter).items()) {
                        options.emplace(key, value.get<std::string>());
                    }
                }

                connection.addMusic(packet["url"].get<std::string>(), options);
                return std::pair(Status::FINISHED, State::NONE);
            }}
        }
    };

    try {
        return processStateMachine(handlers, ClientPacketType(packet["type"].get<unsigned>()),
                                   packet);
    } catch (StateMachineRejection const & e) {
        nlohmann::json packet{
            {"type", static_cast<unsigned>(ServerPacketType::STATUS)},
            {"code", static_cast<unsigned>(ServerStatuses::UNEXPECTED)},
            {"msg", "Packet could not be handled by the state machine"}
        };
        sendPacket(packet);
        throw e;
    }
}
