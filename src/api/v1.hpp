#ifndef API_V1_HPP
#define API_V1_HPP


class v1Conversation : public ClientConnection::Conversation {
public:
    enum class ClientPacketType : unsigned {
        STATUS,      // Response (OK, etc)
        PULSE,       // Heartbeat
        PASSWORD,    // Password sending (both for auth and playlists)
        AUTH_REQ,    // Auth request
        USER_LIST,   // User list request
        PL_LIST,     // Playlist list request
        PL_SEL,      // Playlist selection
        PL_DEL,      // Playlist deletion
        PL_SUB,      // (Un)subscription
        MUS_LIST,    // Playlist contents request
        MUS_ADD,     // Music addition (in playlist, possibly in queue as well)
        MUS_IMPORT,  // Playlist import (basically a repeated `MUS_ADD`)
        MUS_REORDER, // Music queue reordering
        MUS_DEL,     // Music removal from playlist
        MUS_SKIP,    // Music removal from queue
        VOL_SET,     // Volume setting
        POS_SET,     // Playback position setting
        PAUSE        // (Un)pausing
    };
    enum class ServerPacketType : unsigned {
        STATUS,      // Response (OK, etc)
        PULSE,       // Status report (queue, volume, etc)
        AUTH_SALT,   // Authentication salt
        USER_LIST,   // User list
        PL_LIST,     // Playlist list
        MUS_LIST     // Playlist contents
    };

    enum State : unsigned {
        NONE,
        AUTH,
        PL_SEL,
        PL_DEL
    };

    enum class ServerStatuses : unsigned {
        OK,
        REJECTED,   // Input packet itself was malformed
        ERROR,      // Internal error occurred during processing
        TIMEOUT,    // Conversation timed out
        UNEXPECTED, // Packet was rejected by the state machine

        PERMS, // Permission denied, reason depends on context
        BAD_PASS,
        NOT_FOUND
    };

private:
    std::string _playlist;

public:
    v1Conversation(ClientConnection & owner, int id) : Conversation(owner, id) {}

    Status _handlePacket(nlohmann::json const & packet) override;
private:
    void sendSuccess();
};


#endif
