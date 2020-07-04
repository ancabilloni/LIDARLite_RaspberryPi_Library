#include <linux/types.h>
#include <cstdio>
#include <include/lidarlite_v3.h>
// #include "lidar.capnp.h"
// #include <capnp/message.h>
// #include <capnp/serialize-packed.h>
#include <iostream>
#include <chrono>
#include <zmq.hpp>

using namespace std::chrono;
using namespace std;
LIDARLite_v3 myLidarLite;

// void writeLidar(int fd, uint32_t elapsed_t, uint16_t distance)
// {
//     ::capnp::MallocMessageBuilder message;

//     Lidar::Builder lidar_msg = message.initRoot<Lidar>();
//     lidar_msg.time = elapsed_t;
//     lidar_msg.distance = distance;


// }

#pragma pack(1)
union {
    char data[6];
    struct {
        uint16_t distance;
        uint32_t time_ms;
    };
} lidar;

void s_send(zmq::socket_t &socket, string TOPIC, char data[], int size, int flags=0)
{
    zmq::message_t topic(TOPIC.size());
    zmq::message_t message(size);
    memcpy(topic.data(), TOPIC.data(), TOPIC.size());
    memcpy (message.data(), data, size);
    socket.send(topic, ZMQ_SNDMORE);
    socket.send(message, flags);
}

int main()
{
    // Set up zmq
    zmq::context_t context(1);
    zmq::socket_t publisher(context, ZMQ_PUB);
    publisher.bind("tcp://*:5563");

    cout << "ZMQ publisher start..." << endl;
    time_point<system_clock> start, end;
    __u16 distance;
    __u8  busyFlag;

    // Initialize i2c peripheral in the cpu core
    myLidarLite.i2c_init();

    // Optionally configure LIDAR-Lite
    myLidarLite.configure(3);

    start = system_clock::now();
    duration<double> elapsed_s;
    uint32_t elapsed_ms;
    while(1)
    {
        // Each time through the loop, check BUSY
        busyFlag = myLidarLite.getBusyFlag();

        if (busyFlag == 0x00)
        {
            // When no longer busy, immediately initialize another measurement
            // and then read the distance data from the last measurement.
            // This method will result in faster I2C rep rates.
            myLidarLite.takeRange();
            distance = myLidarLite.readDistance();
            elapsed_s = system_clock::now() - start;
            lidar.time_ms = elapsed_s.count()*1000;
            lidar.distance = distance;
            s_send(publisher, "lidar", lidar.data, sizeof(lidar.data));
            printf("%4d\n", distance);
        }
    }
}
