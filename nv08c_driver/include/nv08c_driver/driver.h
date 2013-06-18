#ifndef DRIVER_H
#define DRIVER_H

#include <boost/asio/serial_port.hpp>


namespace nv08c_driver {
    static const uint8_t DLE = 0x10;
    static const uint8_t ETX = 0x03;
    
    class Device {
        private:
            const std::string port;
            const int baudrate;
            boost::asio::io_service io;
            boost::asio::serial_port p;
            
            bool read_byte(uint8_t &res) {
                try {
                    p.read_some(boost::asio::buffer(&res, sizeof(res)));
                    return true;
                } catch(const std::exception &exc) {
                    ROS_ERROR("error on read: %s; reopening", exc.what());
                    open();
                    return false;
                }
            }
            
            void open() {
                try {
                    p.close();
                    p.open(port);
                    p.set_option(boost::asio::serial_port::baud_rate(baudrate));
                    p.set_option(boost::asio::serial_port::parity(boost::asio::serial_port::parity::odd));
                } catch(const std::exception &exc) {
                    ROS_ERROR("error on open(%s): %s; reopening after delay", port.c_str(), exc.what());
                    boost::this_thread::sleep(boost::posix_time::seconds(1));
                }
            }
            
        public:
            Device(const std::string port, int baudrate) : port(port), baudrate(baudrate), p(io) {
                // open is called on first read() in the _polling_ thread
            }
            
            bool read(std::string &res) {
                if(!p.is_open()) {
                    open();
                    return false;
                }
                
                uint8_t firstbyte; if(!read_byte(firstbyte)) return false;
                if(firstbyte != DLE) {
                    std::cout << "firstbyte wasn't DLE" << std::endl;
                    return false;
                }
                
                uint8_t ID; if(!read_byte(ID)) return false;
                if(ID == ETX) {
                    std::cout << "ID was ETX" << std::endl;
                    return false;
                }
                if(ID == DLE) {
                    std::cout << "ID was DLE" << std::endl;
                    return false;
                }
                
                std::vector<uint8_t> data;
                data.push_back(ID);
                while(true) {
                    uint8_t b; if(!read_byte(b)) return false;
                    if(b == DLE) {
                        uint8_t b2; if(!read_byte(b2)) return false;
                        if(b2 == DLE) {
                            data.push_back(DLE);
                        } else if(b2 == ETX) {
                            break;
                        } else {
                            std::cout << "DLE followed by " << (int)b2 << "!" << std::endl;
                            return false;
                        }
                    } else {
                        data.push_back(b);
                    }
                }
                
                std::cout << "finished" << std::endl;
                
                res = std::string(data.begin(), data.end());
                
                return true;
            }
            
            void send_packet(const std::vector<uint8_t> out) {
                try {
                    size_t written = 0;
                    while(written < out.size()) {
                        written += p.write_some(boost::asio::buffer(out.data() + written, out.size() - written));
                    }
                } catch(const std::exception &exc) {
                    ROS_ERROR("error on write: %s; dropping", exc.what());
                }
            }
            
            void send_heartbeat() {
                { uint8_t d[] = {DLE, 0x0E, DLE, ETX}; // disable all
                  send_packet(std::vector<uint8_t>(d, d+sizeof(d)/sizeof(d[0]))); }
                { uint8_t d[] = {DLE, 0xD7, 0x02, 0x0A, DLE, ETX}; // set navigation rate to 10hz
                  send_packet(std::vector<uint8_t>(d, d+sizeof(d)/sizeof(d[0]))); }
                { uint8_t d[] = {DLE, 0xF4, 1, DLE, ETX}; // output raw data as fast as possible
                  send_packet(std::vector<uint8_t>(d, d+sizeof(d)/sizeof(d[0]))); }
                { uint8_t d[] = {DLE, 0x27, 1, DLE, ETX}; // PVT too
                  send_packet(std::vector<uint8_t>(d, d+sizeof(d)/sizeof(d[0]))); }
            }
            void abort() {
                p.close();
            }
    };
    
}

#endif
