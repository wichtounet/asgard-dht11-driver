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

const std::size_t gpio_pin = 24;
const std::size_t max_timings = 85;

int dht11_dat[5] = { 0, 0, 0, 0, 0 };

void read_data(){
    uint8_t laststate   = HIGH;

    dht11_dat[0] = dht11_dat[1] = dht11_dat[2] = dht11_dat[3] = dht11_dat[4] = 0;

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
        printf("Humidity = %d.%d %% Temperature = %d.%d\n", dht11_dat[0], dht11_dat[1], dht11_dat[2], dht11_dat[3]);
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

    //Read data continuously
    while (1){
        read_data();
        delay(1000);
    }

    return(0);
}
