//=======================================================================
// Copyright (c) 2015-2016 Baptiste Wicht
// Distributed under the terms of the MIT License.
// (See accompanying file LICENSE or copy at
//  http://opensource.org/licenses/MIT)
//=======================================================================

#include "asgard/driver.hpp"

#include<wiringPi.h>

namespace {

// Configuration (this should be in a configuration file)
const char* server_socket_path = "/tmp/asgard_socket";
const char* client_socket_path = "/tmp/asgard_dht11_socket";
<<<<<<< HEAD
const std::size_t delay_ms = 60000;
=======
const std::size_t delay_ms = 20000;
const std::size_t gpio_pin = 24;
const std::size_t max_timings = 85;
>>>>>>> upstream/master

// The driver connection
asgard::driver_connector driver;

// The remote IDs
int source_id = -1;
int temperature_sensor_id = -1;
int humidity_sensor_id = -1;

void stop(){
    std::cout << "asgard:dht11: stop the driver" << std::endl;

    asgard::unregister_sensor(driver, source_id, temperature_sensor_id);
    asgard::unregister_sensor(driver, source_id, humidity_sensor_id);
    asgard::unregister_source(driver, source_id);

    // Unlink the client socket
    unlink(client_socket_path);

    // Close the socket
    close(driver.socket_fd);
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
        asgard::send_data(driver, source_id, humidity_sensor_id, dht11_dat[0]);
        asgard::send_data(driver, source_id, temperature_sensor_id, dht11_dat[2]);
    }
}

} //end of namespace

int main(){
    //Run the wiringPi setup (as root)
    wiringPiSetup();

    //Drop root privileges and run as pi:pi again
    if(!asgard::revoke_root()){
       std::cout << "asgard:dht11: unable to revoke root privileges, exiting..." << std::endl;
       return 1;
    }

    // Open the connection
    if(!asgard::open_driver_connection(driver, client_socket_path, server_socket_path)){
        return 1;
    }

    //Register signals for "proper" shutdown
    signal(SIGTERM, terminate);
    signal(SIGINT, terminate);

    // Register the source and sensors
    source_id = asgard::register_source(driver, "dht11");
    temperature_sensor_id = asgard::register_sensor(driver, source_id, "TEMPERATURE", "local");
    humidity_sensor_id = asgard::register_sensor(driver, source_id, "HUMIDITY", "local");

    //Read data continuously
    while (1){
        read_data();
        delay(delay_ms);
    }

    stop();

    return 0;
}
