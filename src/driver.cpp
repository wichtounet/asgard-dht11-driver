//=======================================================================
// Copyright (c) 2015 Baptiste Wicht
// Distributed under the terms of the MIT License.
// (See accompanying file LICENSE or copy at
//  http://opensource.org/licenses/MIT)
//=======================================================================

#include<iostream>
#include<cerrno>

#include<wiringPi.h>

#include<stdio.h>
#include<stdlib.h>
#include<stdint.h>
#include<sys/socket.h>
#include<sys/un.h>
#include<sys/types.h>
#include<unistd.h>
#include<signal.h>

namespace {

const std::size_t UNIX_PATH_MAX = 108;
const std::size_t gpio_pin = 24;
const std::size_t max_timings = 85;
const std::size_t buffer_size = 4096;

// Configuration (this should be in a configuration file)
const char* server_socket_path = "/tmp/asgard_socket";
const char* client_socket_path = "/tmp/asgard_dht11_socket";
const std::size_t delay_ms = 20000;

//Buffers
char write_buffer[buffer_size + 1];
char receive_buffer[buffer_size + 1];

// The socket file descriptor
int socket_fd;

// The socket addresses
struct sockaddr_un client_address;
struct sockaddr_un server_address;

// The remote IDs
int source_id = -1;
int temperature_sensor_id = -1;
int humidity_sensor_id = -1;

void stop(){
    std::cout << "asgard:dht11: stop the driver" << std::endl;

    // Unregister the temperature sensor, if necessary
    if(temperature_sensor_id >= 0){
        auto nbytes = snprintf(write_buffer, buffer_size, "UNREG_SENSOR %d %d", source_id, temperature_sensor_id);
        sendto(socket_fd, write_buffer, nbytes, 0, (struct sockaddr *) &server_address, sizeof(struct sockaddr_un));
    }

    // Unregister the humidity sensor, if necessary
    if(temperature_sensor_id >= 0){
        auto nbytes = snprintf(write_buffer, buffer_size, "UNREG_SENSOR %d %d", source_id, humidity_sensor_id);
        sendto(socket_fd, write_buffer, nbytes, 0, (struct sockaddr *) &server_address, sizeof(struct sockaddr_un));
    }

    // Unregister the source, if necessary
    if(source_id >= 0){
        auto nbytes = snprintf(write_buffer, buffer_size, "UNREG_SOURCE %d", source_id);
        sendto(socket_fd, write_buffer, nbytes, 0, (struct sockaddr *) &server_address, sizeof(struct sockaddr_un));
    }

    // Unlink the client socket
    unlink(client_socket_path);

    // Close the socket
    close(socket_fd);
}

void terminate(int){
    stop();

    std::exit(0);
}

void read_data(){
    uint8_t laststate   = HIGH;

    int dht11_dat[5] = { 0, 0, 0, 0, 0 };

    /* pull pin down for 18 milliseconds */
    pinMode(gpio_pin, OUTPUT);
    digitalWrite(gpio_pin, LOW);
    delay(18);

    /* then pull it up for 40 microseconds */
    digitalWrite(gpio_pin, HIGH);
    delayMicroseconds(40);

    /* prepare to read the pin */
    pinMode(gpio_pin, INPUT);

    /* detect change and read data */
    uint8_t j = 0;
    for(uint8_t i = 0; i < max_timings; i++){
        uint8_t counter = 0;
        while (digitalRead(gpio_pin) == laststate){
            counter++;
            delayMicroseconds(1);
            if (counter == 255){
                break;
            }
        }

        laststate = digitalRead(gpio_pin);

        if (counter == 255){
            break;
        }

        /* ignore first 3 transitions */
        if (i >= 4 && i % 2 == 0){
            /* shove each bit into the storage bytes */
            dht11_dat[j / 8] <<= 1;
            if (counter > 16){
                dht11_dat[j / 8] |= 1;
            }
            j++;
        }
    }

    /*
     * check we read 40 bits (8bit x 5 ) + verify checksum in the last byte
     */
    if (j >= 40 && dht11_dat[4] == ((dht11_dat[0] + dht11_dat[1] + dht11_dat[2] + dht11_dat[3]) & 0xFF)){
        //Send the humidity to the server
        auto nbytes = snprintf(write_buffer, buffer_size, "DATA %d %d %d", source_id, humidity_sensor_id, dht11_dat[0]);
        sendto(socket_fd, write_buffer, nbytes, 0, (struct sockaddr *) &server_address, sizeof(struct sockaddr_un));

        //Send the temperature to the server
        nbytes = snprintf(write_buffer, buffer_size, "DATA %d %d %d", source_id, temperature_sensor_id, dht11_dat[2]);
        sendto(socket_fd, write_buffer, nbytes, 0, (struct sockaddr *) &server_address, sizeof(struct sockaddr_un));
    }
}

bool revoke_root(){
    if (getuid() == 0) {
        if (setgid(1000) != 0){
            std::cout << "asgard:dht11: setgid: Unable to drop group privileges: " << strerror(errno) << std::endl;
            return false;
        }

        if (setuid(1000) != 0){
            std::cout << "asgard:dht11: setgid: Unable to drop user privileges: " << strerror(errno) << std::endl;
            return false;
        }
    }

    if (setuid(0) != -1){
        std::cout << "asgard:dht11: managed to regain root privileges, exiting..." << std::endl;
        return false;
    }

    return true;
}

} //end of namespace

int main(){
    //Run the wiringPi setup (as root)
    wiringPiSetup();

    //Drop root privileges and run as pi:pi again
    if(!revoke_root()){
       std::cout << "asgard:dht11: unable to revoke root privileges, exiting..." << std::endl;
       return 1;
    }

    // Open the socket
    socket_fd = socket(AF_UNIX, SOCK_DGRAM, 0);
    if(socket_fd < 0){
        std::cerr << "asgard:dht11: socket() failed" << std::endl;
        return 1;
    }

    // Init the client address
    memset(&client_address, 0, sizeof(struct sockaddr_un));
    client_address.sun_family = AF_UNIX;
    snprintf(client_address.sun_path, UNIX_PATH_MAX, client_socket_path);

    // Unlink the client socket
    unlink(client_socket_path);

    // Bind to client socket
    if(bind(socket_fd, (const struct sockaddr *) &client_address, sizeof(struct sockaddr_un)) < 0){
        std::cerr << "asgard:dht11: bind() failed" << std::endl;
        return 1;
    }

    //Register signals for "proper" shutdown
    signal(SIGTERM, terminate);
    signal(SIGINT, terminate);

    // Init the server address
    memset(&server_address, 0, sizeof(struct sockaddr_un));
    server_address.sun_family = AF_UNIX;
    snprintf(server_address.sun_path, UNIX_PATH_MAX, server_socket_path);

    socklen_t address_length = sizeof(struct sockaddr_un);

    // Register the source
    auto nbytes = snprintf(write_buffer, buffer_size, "REG_SOURCE dht11");
    sendto(socket_fd, write_buffer, nbytes, 0, (struct sockaddr *) &server_address, sizeof(struct sockaddr_un));

    auto bytes_received = recvfrom(socket_fd, receive_buffer, buffer_size, 0, (struct sockaddr *) &(server_address), &address_length);
    receive_buffer[bytes_received] = '\0';

    source_id = atoi(receive_buffer);

    std::cout << "asgard:dht11: remote source: " << source_id << std::endl;

    // Register the temperature sensor
    nbytes = snprintf(write_buffer, buffer_size, "REG_SENSOR %d %s %s", source_id, "TEMPERATURE", "local");
    sendto(socket_fd, write_buffer, nbytes, 0, (struct sockaddr *) &server_address, sizeof(struct sockaddr_un));

    bytes_received = recvfrom(socket_fd, receive_buffer, buffer_size, 0, (struct sockaddr *) &(server_address), &address_length);
    receive_buffer[bytes_received] = '\0';

    temperature_sensor_id = atoi(receive_buffer);

    std::cout << "asgard:random: remote temperature sensor: " << temperature_sensor_id << std::endl;

    // Register the humidity sensor
    nbytes = snprintf(write_buffer, buffer_size, "REG_SENSOR %d %s %s", source_id, "HUMIDITY", "local");
    sendto(socket_fd, write_buffer, nbytes, 0, (struct sockaddr *) &server_address, sizeof(struct sockaddr_un));

    bytes_received = recvfrom(socket_fd, receive_buffer, buffer_size, 0, (struct sockaddr *) &(server_address), &address_length);
    receive_buffer[bytes_received] = '\0';

    humidity_sensor_id = atoi(receive_buffer);

    std::cout << "asgard:random: remote humidity sensor: " << humidity_sensor_id << std::endl;

    //Read data continuously
    while (1){
        read_data();
        delay(delay_ms);
    }

    stop();

    return 0;
}
