
#include "../client_connection.hpp"


v1Conversation::Status v1Conversation::_handlePacket(nlohmann::json const & packet) {
    std::array const handlers{
        std::map<PacketType, TransitionFunc>{ // NONE
            std::pair{ClientPacketType::PULSE, [&](nlohmann::json const &) {
                return std::pair(Status::FINISHED, State::NONE);
            }},

            std::pair{ClientPacketType::PL_SEL, [&](nlohmann::json const & packet) {
                std::string playlist = packet.at("name").get<std::string>();
                if (_owner.playlistExists(playlist)) {
                    _owner.selectPlaylist(playlist);
                    sendSuccess();
                    return std::pair(Status::FINISHED, State::NONE);

                } else {
                    nlohmann::json packet{
                        {"type", ServerPacketType::STATUS},
                        {"code", ServerStatuses::NOT_FOUND},
                        {"msg", "Please enter a password to create the playlist with"}
                    };
                    sendPacket(packet);
                    _playlist = playlist;
                    return std::pair(Status::CONTINUING, State::PL_SEL);
                }
            }},

            std::pair{ClientPacketType::PL_SUB, [&](nlohmann::json const & packet) {
                packet.at("sub").get<bool>() ? _owner.subscribe() : _owner.unsubscribe();
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

            std::pair{ClientPacketType::POS_SET, [&](nlohmann::json const & packet) {
                _owner.seek(packet.at("pos").get<double>());
                sendSuccess();
                return std::pair(Status::FINISHED, State::NONE);
            }},

            std::pair{ClientPacketType::PAUSE, [&](nlohmann::json const & packet) {
                packet.at("stop").get<bool>() ? _owner.pause() : _owner.play();
                sendSuccess();
                return std::pair(Status::FINISHED, State::NONE);
            }}
        },


        std::map<PacketType, TransitionFunc>{ // AUTH
        },


        std::map<PacketType, TransitionFunc>{ // PL_SEL
            std::pair{ClientPacketType::PASSWORD, [&](nlohmann::json const & packet) {
                std::string password = packet.at("pass").get<std::string>();
                if (password.empty()) {
                    nlohmann::json error{
                        {"type", ServerPacketType::STATUS},
                        {"code", ServerStatuses::BAD_PASS},
                        {"msg", "A playlist cannot have an empty password"}
                    };
                    sendPacket(error);
                    return std::pair(Status::CONTINUING, State::PL_SEL);
                }

                _owner.newPlaylist(_playlist, packet.at("pass").get<std::string>());
                _owner.selectPlaylist(_playlist);
                sendSuccess();
                return std::pair(Status::FINISHED, State::NONE);
            }}
        },


        std::map<PacketType, TransitionFunc>{ // PL_DEL
        }
    };

    try {
        return processStateMachine(handlers, packet.at("type").get<unsigned>(), packet);

    } catch (StateMachineRejection const & e) {
        nlohmann::json packet{
            {"type", ServerPacketType::STATUS},
            {"code", ServerStatuses::UNEXPECTED},
            {"msg", "Packet could not be handled by the state machine"}
        };
        sendPacket(packet);
        throw e;

    } catch (nlohmann::json::exception const & e) {
        nlohmann::json packet{
            {"type", ServerPacketType::STATUS},
            {"code", ServerStatuses::REJECTED},
            {"msg", e.what()} // TODO: process the message better than the default message
        };
        sendPacket(packet);
        throw e;

    } catch (std::exception const & e) {
        nlohmann::json packet{
            {"type", ServerPacketType::STATUS},
            {"code", ServerStatuses::ERROR},
            {"msg", e.what()}
        };
        sendPacket(packet);
        throw e;
    } catch (...) {
        nlohmann::json packet{
            {"type", ServerPacketType::STATUS},
            {"code", ServerStatuses::ERROR},
            {"msg", "Unknown exception occurred within state machine"}
        };
        sendPacket(packet);
        throw;
    }
}


void v1Conversation::sendTimeout() {
    nlohmann::json packet{
        {"type", ServerPacketType::STATUS},
        {"code", ServerStatuses::TIMEOUT}
    };
    sendPacket(packet);
}

void v1Conversation::heartbeat(nlohmann::json const & status) {
    nlohmann::json packet{
        {"type", ServerPacketType::PULSE},
        {"duration", status["duration"]},
        {"pause", status["pause"]},
        {"playlist", status["playlist"]},
        {"position", status["position"]}
    };
    sendPacket(packet);
}

void v1Conversation::sendSuccess() {
    nlohmann::json packet{
        {"type", ServerPacketType::STATUS},
        {"code", ServerStatuses::OK}
    };
    sendPacket(packet);
}
