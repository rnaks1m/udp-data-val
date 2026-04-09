#include <algorithm> 
#include <array>
#include <boost/asio.hpp>
#include <chrono>
#include <exception>
#include <iostream>
#include <limits>

namespace net = boost::asio;
using net::ip::udp;

struct ClientData {
    int X, Y, V, M, S, P;
    double A;
    uint16_t R;
};

std::array<uint16_t, 4> PackData(const ClientData& data) {
    std::array<uint16_t, 4> pack_data;

    pack_data[0] = ((data.Y + 32) & 0x3F) << 8 | (data.X & 0x3F);
    pack_data[1] = ((data.S & 0x03) << 12) | ((data.M & 0x03) << 8) | (data.V & 0xFF);
    int a = std::clamp(static_cast<int>(data.A * 10), -128, 127);
    pack_data[2] = ((data.P & 0xFF) << 8) | (a & 0xFF);
    pack_data[3] = data.R;

    return pack_data;
}

int main() {
    try {    
        net::io_context io_context;
        udp::socket socket(io_context);
        socket.open(udp::v4());

        int server_port = 9999;
        udp::endpoint server_endpoint(net::ip::address::from_string("127.0.0.1"), server_port);
        net::steady_timer timer(io_context, std::chrono::seconds(1));

        auto send_data = [&](auto self) {
            ClientData data;
            std::cout << "Enter X Y V M S A P" << std::endl;

            if (!(std::cin >> data.X >> data.Y >> data.V >> data.M >> data.S >> data.A >> data.P)) {
                std::cin.clear();
                std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
                std::cout << "Invalid input! Try again." << std::endl;

                timer.expires_after(std::chrono::seconds(1));
                timer.async_wait([&, self](auto){ self(self); });
                return;
            }
            data.R = 0;

            auto pack = PackData(data);

            std::array<char, 8> buffer;
            for (int i(0); i < 4; ++i) {
                buffer[2 * i] = (pack[i] >> 8) & 0xFF;
                buffer[2 * i + 1] = pack[i] & 0xFF;
            }

            socket.send_to(net::buffer(buffer), server_endpoint);

            std::array<uint8_t, 2> recv_buf;
            udp::endpoint sender_endpoint;
            size_t len = socket.receive_from(net::buffer(recv_buf), sender_endpoint);

            if (len == 2) {
                std::cout << "Server responce: " << (recv_buf[1] ? "Valid" : "Invalid") << std::endl << std::endl;
            }

            timer.expires_after(std::chrono::seconds(1));
            timer.async_wait([&, self](auto){ self(self); });
        };

        timer.async_wait([&](auto){ send_data(send_data); });
        io_context.run();
    }
    catch (std::exception& e) {
        std::cerr << e.what() << std::endl;
    }
}