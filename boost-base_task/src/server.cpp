#include <array>
#include <boost/asio.hpp>
#include <boost/json.hpp>
#include <cstdint>
#include <exception>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

namespace net = boost::asio;
namespace json = boost::json;

using net::ip::udp;

struct Limits {
    int X_min, X_max;
    int Y_min, Y_max;
    int V_min, V_max;
    int M_min, M_max;
    int S_min, S_max;
    double A_min, A_max;
    int P_min, P_max;
};

struct Packet {
    int X, Y, V, M, S, P;
    double A;
    uint16_t R;
};

Packet UnpackPacket(const std::array<uint16_t, 4>& data) {
    Packet packet;

    packet.X = data[0] & 0x3F;
    packet.Y = ((data[0] >> 8) & 0x3F) - 32;
    packet.V = data[1] & 0xFF;  
    packet.M = (data[1] >> 8) & 0x03;
    packet.S = (data[1] >> 12) & 0x03;
    int8_t a = static_cast<int8_t>(data[2] & 0xFF);
    packet.A = static_cast<double>(a) / 10.0;
    packet.P = (data[2] >> 8) & 0xFF;
    packet.R = data[3];

    return packet;
}

bool CheckLimits(const Packet& packet, const Limits& limits) {
    std::cout << "Values:\n";
    std::cout << "X: " << packet.X << "\n";
    std::cout << "Y: " << packet.Y << "\n";
    std::cout << "V: " << packet.V << "\n";
    std::cout << "M: " << packet.M << "\n";
    std::cout << "S: " << packet.S << "\n";
    std::cout << "A: " << packet.A << "\n";
    std::cout << "P: " << packet.P << "\n";

    std::cout << "Limits:\n";
    std::cout << "X - max: " << limits.X_max << " min: " << limits.X_min << "\n";
    std::cout << "Y - max: " << limits.Y_max << " min: " << limits.Y_min << "\n";
    std::cout << "V - max: " << limits.V_max << " min: " << limits.V_min << "\n";
    std::cout << "M - max: " << limits.M_max << " min: " << limits.M_min << "\n";
    std::cout << "S - max: " << limits.S_max << " min: " << limits.S_min << "\n";
    std::cout << "A - max: " << limits.A_max << " min: " << limits.A_min << "\n";
    std::cout << "P - max: " << limits.P_max << " min: " << limits.P_min << "\n";

    return packet.X >= limits.X_min && packet.X <= limits.X_max &&
           packet.Y >= limits.Y_min && packet.Y <= limits.Y_max &&
           packet.V >= limits.V_min && packet.V <= limits.V_max &&
           packet.M >= limits.M_min && packet.M <= limits.M_max &&
           packet.S >= limits.S_min && packet.S <= limits.S_max &&
           packet.A >= limits.A_min && packet.A <= limits.A_max &&
           packet.P >= limits.P_min && packet.P <= limits.P_max;
}

Limits ReadLimits(const std::string& filename) {
    Limits lim;
    std::ifstream file("../data/" + filename);

    if (!file) {
        throw std::runtime_error("file not found!");
    }

    std::stringstream buf;
    buf << file.rdbuf();

    json::value val;

    try {
        val = json::parse(buf.str());
    }
    catch (std::exception& e) {
        std::cerr << e.what() << std::endl;
    }

    json::object root = val.as_object();

    const auto& lim_array = root.at("limits").as_array();
    const auto& obj = lim_array.at(0).as_object();

    auto get_int_range = [&obj](const char* key, int& min, int& max) {
        const auto& arr = obj.at(key).as_array();
        min = static_cast<int>(arr.at(0).get_int64());
        max = static_cast<int>(arr.at(1).get_int64());
    };

    auto get_double_range = [&obj](const char* key, double& min, double& max) {
        const auto& arr = obj.at(key).as_array();
        min = arr.at(0).get_double();
        max = arr.at(1).get_double();
    };

    get_int_range("X", lim.X_min, lim.X_max);
    get_int_range("Y", lim.Y_min, lim.Y_max);
    get_int_range("V", lim.V_min, lim.V_max);
    get_int_range("M", lim.M_min, lim.M_max);
    get_int_range("S", lim.S_min, lim.S_max);
    get_double_range("A", lim.A_min, lim.A_max);
    get_int_range("P", lim.P_min, lim.P_max);

    return lim;
}

class UDPServer {
public: 
    UDPServer(net::io_context& io_context, udp::endpoint& endpoint, const Limits& limits) :
        endpoint_(endpoint), 
        socket_(io_context, endpoint), 
        limits_(limits) 
        {
            StartReceive();
        }

private:

    void StartReceive() {
        socket_.async_receive_from(net::buffer(recv_buf_), endpoint_, [this](boost::system::error_code ec, std::size_t bytes_recvd){
            if (!ec && bytes_recvd == 8) {
                std::array<uint16_t, 4> data;

                for (int i(0); i < 4; ++i) {
                    data[i] = static_cast<uint8_t>(recv_buf_[2 * i]) << 8 | static_cast<uint8_t>(recv_buf_[2 * i + 1]);
                }

                Packet pack = UnpackPacket(data);
                bool valid = CheckLimits(pack, limits_);

                std::array<uint8_t, 2> responce{0, valid ? static_cast<uint8_t>(1) : static_cast<uint8_t>(0)};
                socket_.async_send_to(net::buffer(responce), endpoint_, [](boost::system::error_code, std::size_t){});
            }
            StartReceive();
        });
    }

    udp::endpoint endpoint_;
    udp::socket socket_;
    std::array<char, 8> recv_buf_;
    Limits limits_;
};

int main() {
    try {
        const int port = 9999;
        net::io_context io_context;

        udp::endpoint endpoint(udp::v4(), port);
        Limits limits = ReadLimits("limits.json");

        UDPServer server(io_context, endpoint, limits);

        std::cout << "Async server started on port " << port << "..." << std::endl;
        io_context.run();
    }
    catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }
}
