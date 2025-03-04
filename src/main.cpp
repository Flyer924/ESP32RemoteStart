/*******************************************************************************
 * Author: Trace Houser, Rui Santos
 *
 * Date: 2025/01/18
 *
 * Description:
 *      This is some simple code to remotely start my computer using an ESP32.
 *      The initial code I used was provided by Rui Santos at the link below
 *      and has been heavily modified.
 *
 *      In total, I'm using 1 ESP32, 1 Relay, and some wires. How the program works
 *      is it connects to your wifi (configurable under "Program Settings"),
 *      hosts a simple server with a toggle button, connects the relay
 *      when the toggle button is clicked.
 *
 * Links:
 *      Code from Rui Santos:
 *          https://randomnerdtutorials.com/esp32-web-server-arduino-ide/
 ******************************************************************************/

/*******************************************************************************
 * Imports
 ******************************************************************************/

#include <WiFi.h>
#include <env.h>

/*******************************************************************************
 * Program Settings
 ******************************************************************************/

// Replace with your network credentials
const char *ssid = ESP32_WIFI_SSID;
const char *password = ESP32_WIFI_PASSWORD;
// Define server timeout in milliseconds (example: 2000ms = 2s)
const long timeoutTime = 2000;

/*******************************************************************************
 * Program Global Internal Variables
 ******************************************************************************/
// Set web server port number to 80
WiFiServer server(80);
// Variable to store the HTTP request
String header;
// Current time
unsigned long currentTime = millis();
// Previous time
unsigned long previousTime = 0;

/*******************************************************************************
 * Program Pins
 ******************************************************************************/
// This pin sends the toggle signal to the relay
const int TOGGLE_PIN = 15;

// Circuit 1 (connected to the PC Power button)
// This is for detecting when the PC power button is pushed
// This pin is an INPUT pin detecting the amount of power
// When the button is pushed, it completes the circuit which sends the power from
// EXTRA_VCC to this pin (indicating we need to toggle the computer)
const int BUTTON_INPUT = 4;
// This pin is a part of the circuit
const int EXTRA_VCC = 2;

/*******************************************************************************
 * Function Declarations
 ******************************************************************************/
void sendCSS(WiFiClient &client);
void processHeaderRequest(String &header);

/*******************************************************************************
 * Initial Setup
 ******************************************************************************/
void setup()
{
    Serial.begin(115200);
    // Connect to Wi-Fi network with SSID and password
#if defined ESP32_WIFI_SSID
#if defined ESP32_WIFI_PASSWORD
    Serial.print("Connecting to ");
    Serial.println(ssid);
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
    }
    // Print local IP address and start web server
    Serial.println("");
    Serial.println("WiFi connected.");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
    server.begin();
#else
    Serial.println("No ESP32_WIFI_PASSWORD environment variable")
#endif
#else
    Serial.println("No ESP32_WIFI_SSID environment variable")
#endif

    // Pin setup last because the ExtraVCC pin is also the LED pin (physical indicator
    // of when the setup function is done)
    //  setup pins
    pinMode(TOGGLE_PIN, OUTPUT);
    pinMode(BUTTON_INPUT, INPUT);
    pinMode(EXTRA_VCC, OUTPUT);
    // Set degault states of pins
    digitalWrite(TOGGLE_PIN, LOW);
    digitalWrite(EXTRA_VCC, HIGH);
}

/*******************************************************************************
 * Main Loop
 ******************************************************************************/
void loop()
{
    // Check for Physical Button press
    // check if button is being pressed
    int button_state = digitalRead(BUTTON_INPUT);
    int input_state = digitalRead(TOGGLE_PIN);
    // if button pressed and toggle off, turn it on
    if (button_state == HIGH && input_state == LOW)
    {
        digitalWrite(TOGGLE_PIN, HIGH);
    }
    // if button not pressed and toggle on, turn it off
    else if (button_state == LOW && input_state == HIGH)
    {
        digitalWrite(TOGGLE_PIN, LOW);
    }
    // else if the button is not pressed and it is off, do nothing
    // else if the button is pressed and it in on, do nothing

    // Check for Server Usage
    //  Listen for incoming clients
    WiFiClient client = server.available();
    // Check if a client has connected
    if (!client)
    {
        return;
    }
    currentTime = millis();
    previousTime = currentTime;
    Serial.println("New Client."); // print a message out in the serial port
    String currentLine = "";       // make a String to hold incoming data from the client
    while (client.connected() && currentTime - previousTime <= timeoutTime)
    { // loop while the client's connected
        currentTime = millis();
        // Check if there's bytes to read from the client
        if (!client.available())
        {
            continue;
        }
        char c = client.read(); // read a byte, then
        Serial.write(c);        // print it out the serial monitor
        header += c;
        if (c == '\n')
        {
            // if the byte is a newline character
            // if the current line is blank, you got two newline characters in a row.
            // that's the end of the client HTTP request, so send a response:
            if (currentLine.length() == 0)
            {
                // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
                // and a content-type so the client knows what's coming, then a blank line:
                client.println("HTTP/1.1 200 OK");
                client.println("Content-type:text/html");
                client.println("Connection: close");
                client.println();

                processHeaderRequest(header);

                // Display the HTML web page
                client.println("<!DOCTYPE html><html>");
                client.println("<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
                client.println("<link rel=\"icon\" href=\"data:,\">");
                sendCSS(client);

                // Web Page Heading
                client.println("<body><h1>ESP32 Web Server</h1>");

                // Display current state, and ON/OFF buttons for GPIO 26
                client.println("<p>PC Control</p>");
                // If the togglePinState is on, it displays the ON button
                client.println("<p><a href=\"/15/toggle\"><button class=\"button button-off\">Toggle PC</button></a></p>");
                client.println("</body></html>");

                // The HTTP response ends with another blank line
                client.println();
                // Break out of the while loop
                break;
            }
            else
            { // if you got a newline, then clear currentLine
                currentLine = "";
            }
        }
        else if (c != '\r')
        {                     // if you got anything else but a carriage return character,
            currentLine += c; // add it to the end of the currentLine
        }
    }
    // Clear the header variable
    header = "";
    // Close the connection
    client.stop();
    Serial.println("Client disconnected.");
    Serial.println("");
}

/*******************************************************************************
 * Extra Functions
 ******************************************************************************/
void sendCSS(WiFiClient &client)
{
    // CSS to style the on/off buttons
    // Feel free to change the background-color and font-size attributes to fit your preferences
    client.println("<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}");
    client.println(".button { border: none; color: white; padding: 16px 40px;");
    client.println("text-decoration: none; font-size: 30px; margin: 2px; cursor: pointer;}");
    client.println(".button-on { background-color: #4CAF50;}");
    client.println(".button-off {background-color: #555555;}</style></head>");
}

void processHeaderRequest(String &header)
{
    // if on request, turn PC on
    if (header.indexOf("GET /15/toggle") >= 0)
    {
        Serial.println("Toggling Relay for 500 ms");
        digitalWrite(TOGGLE_PIN, HIGH);
        delay(500);
        digitalWrite(TOGGLE_PIN, LOW);
    }
}