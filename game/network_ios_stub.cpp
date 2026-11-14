//
// open horizon -- undefined_darkness@outlook.com
// iOS multiplayer stub: the miso tcp client/server (and the HTTP master
// server list) are excluded from the iOS build; demo.cpp still references
// these classes, so provide no-op implementations instead.
//

#include "game/network_client.h"
#include "game/network_server.h"
#include "game/network.h"

namespace game
{

//------------------------------------------------------------
//network_client

network_client::~network_client() {}

bool network_client::connect(const char *address, short port)
{
    (void)address, (void)port;
    m_error = "multiplayer is not available on iOS";
    return false;
}

void network_client::disconnect() {}

void network_client::start() {}

const server_info &network_client::get_server_info() const { return m_server_info; }

void network_client::update() {}

void network_client::update_post(int dt) { (void)dt; }

bool network_client::send_string(const std::string &str) { (void)str; return false; }

//------------------------------------------------------------
//network_server

network_server::~network_server() {}

bool network_server::open(unsigned short port, const char *name, const char *game_mode,
                          const char *location, int max_players)
{
    (void)port, (void)name, (void)game_mode, (void)location, (void)max_players;
    return false;
}

void network_server::close() {}

int network_server::get_players_count() const { return 0; }

void network_server::update() {}

void network_server::update_post(int dt) { (void)dt; }

void network_server::process_msg(const std::pair<miso::server_tcp::client_id, std::vector<uint8_t> > &msg)
{
    (void)msg;
}

void network_server::process_msg(client &c, const std::vector<uint8_t> &msg)
{
    (void)c, (void)msg;
}

void network_server::remove_client(miso::server_tcp::client_id id) { (void)id; }

bool network_server::send_string(miso::server_tcp::client_id id, const std::string &str)
{
    (void)id, (void)str;
    return false;
}

void network_server::cache_net_game_data(const msg_game_data &data) { (void)data; }

//------------------------------------------------------------
//servers_list (HTTP master-server registration, disabled on iOS)

bool servers_list::request_update() { return false; }
bool servers_list::is_ready() const { return true; }
void servers_list::get_list(list &l) const { l.clear(); }
bool servers_list::register_server(short port) { (void)port; return false; }

//------------------------------------------------------------
}
