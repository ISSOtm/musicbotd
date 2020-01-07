
#include "../client_connection.hpp"


v1Conversation::Status v1Conversation::_handlePacket(nlohmann::json const & packet) {
    std::array const handlers{
        std::map<ClientPacketType, TransitionFunc<State>>{ // NONE
            std::pair{ClientPacketType::PULSE, [&](nlohmann::json const &) {
                return std::pair(Status::FINISHED, State::NONE);
            }},

            std::pair{ClientPacketType::PL_SEL, [&](nlohmann::json const & packet) {
                std::string playlist = packet["name"].get<std::string>();
                if (_owner.playlistExists(playlist)) {
                    _owner.selectPlaylist(playlist);
                    sendSuccess();
                    return std::pair(Status::FINISHED, State::NONE);

                } else {
                    nlohmann::json packet{
                        {"type", static_cast<unsigned>(ServerPacketType::STATUS)},
                        {"code", static_cast<unsigned>(ServerStatuses::NOT_FOUND)},
                        {"msg", "Please enter a password to create the playlist with"}
                    };
                    sendPacket(packet);
                    _playlist = playlist;
                    return std::pair(Status::CONTINUING, State::PL_SEL);
                }
            }},

            std::pair{ClientPacketType::PL_SUB, [&](nlohmann::json const & packet) {
                packet["sub"].get<bool>() ? _owner.subscribe() : _owner.unsubscribe();
                sendSuccess();
                return std::pair(Status::FINISHED, State::NONE);
            }},

            std::pair{ClientPacketType::MUS_ADD, [&](nlohmann::json const & packet) {
                Music music(packet.at("url").get<std::string>());

                // Parse options
                std::map<std::string, std::string> options;
                auto iter = packet.find("options");
                if (iter != packet.end()) { // The `options` object is optional
                    for (auto const & [key, value] : (*iter).items()) {
                        music.setOption(key, value.get<std::string>());
                    }
                }

                _owner.addMusic(music);
                // Do not add to the queue if we're already subscribed (already adding musics)
                if (!_owner.subscribed()) _owner.appendMusic(music);

                sendSuccess();
                return std::pair(Status::FINISHED, State::NONE);
            }},

            std::pair{ClientPacketType::PAUSE, [&](nlohmann::json const & packet) {
                packet["stop"].get<bool>() ? _owner.pause() : _owner.play();
                sendSuccess();
                return std::pair(Status::FINISHED, State::NONE);
            }}
        },


        std::map<ClientPacketType, TransitionFunc<State>>{ // AUTH
        },


        std::map<ClientPacketType, TransitionFunc<State>>{ // PL_SEL
            std::pair{ClientPacketType::PASSWORD, [&](nlohmann::json const & packet) {
                _owner.newPlaylist(_playlist, packet["pass"].get<std::string>());
                _owner.selectPlaylist(_playlist);
                sendSuccess();
                return std::pair(Status::FINISHED, State::NONE);
            }}
        },


        std::map<ClientPacketType, TransitionFunc<State>>{ // PL_DEL
        }
    };

    try {
        return processStateMachine(handlers, ClientPacketType(packet.at("type").get<unsigned>()),
                                   packet);

    } catch (StateMachineRejection const & e) {
        nlohmann::json packet{
            {"type", static_cast<unsigned>(ServerPacketType::STATUS)},
            {"code", static_cast<unsigned>(ServerStatuses::UNEXPECTED)},
            {"msg", "Packet could not be handled by the state machine"}
        };
        sendPacket(packet);
        throw e;

    } catch (nlohmann::json::exception const & e) {
        nlohmann::json packet{
            {"type", static_cast<unsigned>(ServerPacketType::STATUS)},
            {"code", static_cast<unsigned>(ServerStatuses::REJECTED)},
            {"msg", e.what()} // TODO: process the message better than the default message
        };
        sendPacket(packet);
        throw e;
    }
}


void v1Conversation::sendTimeout() {
    nlohmann::json packet{
        {"type", static_cast<unsigned>(ServerPacketType::STATUS)},
        {"code", static_cast<unsigned>(ServerStatuses::TIMEOUT)}
    };
    sendPacket(packet);
}

void v1Conversation::sendSuccess() {
    nlohmann::json packet{
        {"type", static_cast<unsigned>(ServerPacketType::STATUS)},
        {"code", static_cast<unsigned>(ServerStatuses::OK)}
    };
    sendPacket(packet);
}
