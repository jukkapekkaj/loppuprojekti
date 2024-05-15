#include <iostream>
#include <cstdlib>
#include <string>
#include <cstring>
#include <cctype>
#include <chrono>
#include <mqtt/async_client.h>
#include <thread>
#include "serialib.h"
#include <mutex>

#define SERIAL_PORT "/dev/ttyACM0"

const std::string SERVER_ADDRESS("mqtt://localhost:1883");
const std::string CLIENT_ID("test_client");
const std::string TOPIC("test");

const int	QOS = 1;
const int	N_RETRY_ATTEMPTS = 5;

std::mutex mutex;

/**
 * Send command to microcontroller using serial ports. 
 * @param command Only commands "0" and "1" are sent
 * @return Function does not return anything.
*/
void send_command(std::string command){
    std::lock_guard<std::mutex> lock(mutex);
    serialib serial;
    // Connection to serial port
    char return_code = serial.openDevice(SERIAL_PORT, 115200);

    if (return_code != 1){
        std::cout << "Failed to open serial connection" << std::endl;
        return;
    }
    printf ("Successful connection to %s\n",SERIAL_PORT);

    if(command == "1"){
        serial.writeChar('1');
    }
    else if(command == "0"){
        serial.writeChar('0');
    }

    serial.closeDevice();
}

/**
 * Local callback & listener class for use with the client connection.
 * This is primarily intended to receive messages, but it will also monitor
 * the connection to the broker. If the connection is lost, it will attempt
 * to restore the connection and re-subscribe to the topic.
 */
class callback : public virtual mqtt::callback,
                 public virtual mqtt::iaction_listener

{
    // Counter for the number of connection retries
    int nretry_;
    // The MQTT client
    mqtt::async_client& cli_;
    // Options to use if we need to reconnect
    mqtt::connect_options& connOpts_;
    // An action listener to display the result of actions.
    //action_listener subListener_;

    // This deomonstrates manually reconnecting to the broker by calling
    // connect() again. This is a possibility for an application that keeps
    // a copy of it's original connect_options, or if the app wants to
    // reconnect with different options.
    // Another way this can be done manually, if using the same options, is
    // to just call the async_client::reconnect() method.
    void reconnect() {
        std::this_thread::sleep_for(std::chrono::milliseconds(2500));
        try {
            cli_.connect(connOpts_, nullptr, *this);
        }
        catch (const mqtt::exception& exc) {
            std::cerr << "Error: " << exc.what() << std::endl;
            exit(1);
        }
    }

    // Re-connection failure
    void on_failure(const mqtt::token& tok) override {
        std::cout << "Connection attempt failed" << std::endl;
        if (++nretry_ > N_RETRY_ATTEMPTS)
            exit(1);
        reconnect();
    }

    // (Re)connection success
    // Either this or connected() can be used for callbacks.
    void on_success(const mqtt::token& tok) override {}

    // (Re)connection success
    void connected(const std::string& cause) override {
        std::cout << "\nConnection success" << std::endl;
        std::cout << "\nSubscribing to topic '" << TOPIC << "'\n"
                  << "\tfor client " << CLIENT_ID
                  << " using QoS" << QOS << "\n"
                  << "\nPress Q<Enter> to quit\n" << std::endl;

        //li_.subscribe(TOPIC, QOS, nullptr, subListener_);
        cli_.subscribe(TOPIC, QOS);
    }

    // Callback for when the connection is lost.
    // This will initiate the attempt to manually reconnect.
    void connection_lost(const std::string& cause) override {
        std::cout << "\nConnection lost" << std::endl;
        if (!cause.empty())
            std::cout << "\tcause: " << cause << std::endl;

        std::cout << "Reconnecting..." << std::endl;
        nretry_ = 0;
        reconnect();
    }

    // Callback for when a message arrives.
    void message_arrived(mqtt::const_message_ptr msg) override {
        std::cout << "Message arrived" << std::endl;
        std::cout << "\ttopic: '" << msg->get_topic() << "'" << std::endl;
        std::cout << "\tpayload: '" << msg->to_string() << "'\n" << std::endl;
        std::thread send(send_command, msg->to_string());
        send.join();
    }

    void delivery_complete(mqtt::delivery_token_ptr token) override {}

public:
    callback(mqtt::async_client& cli, mqtt::connect_options& connOpts)
            : nretry_(0), cli_(cli), connOpts_(connOpts) {}
};


int main(int argc, char* argv[])
{

    mqtt::async_client cli(SERVER_ADDRESS, CLIENT_ID);

    mqtt::connect_options connOpts;
    connOpts.set_clean_session(false);

    // Install the callback(s) before connecting.
    callback cb(cli, connOpts);
    cli.set_callback(cb);

    // Start the connection.
    // When completed, the callback will subscribe to topic.

    try {
        std::cout << "Connecting to the MQTT server..." << std::flush;
        cli.connect(connOpts, nullptr, cb);
    }
    catch (const mqtt::exception& exc) {
        std::cerr << "\nERROR: Unable to connect to MQTT server: '"
                  << SERVER_ADDRESS << "'" << exc << std::endl;
        return 1;
    }

    // Just block till user tells us to quit.

    while (std::tolower(std::cin.get()) != 'q')
        ;

    // Disconnect

    try {
        std::cout << "\nDisconnecting from the MQTT server..." << std::flush;
        cli.disconnect()->wait();
        std::cout << "OK" << std::endl;
    }
    catch (const mqtt::exception& exc) {
        std::cerr << exc << std::endl;
        return 1;
    }

    return 0;
}