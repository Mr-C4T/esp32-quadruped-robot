/*
                                                                                      
                0                                                           0 0       
               0  00                                                       30  0      
             0  00 000000                                            9    0  00  0    
           0   0  331009000                                        000  0 03   0  00  
           0 0  333333108 000                                    0000000033333  00    
             040 0333393308 000                                000  0  3931133300     
                00 00 1333 0  00                              00  00 033330 30800     
              00 000 003      0500                          000      3337 30950       
                00  0        05 1000                      000100       3300 000       
                 000  0    009 1111000                  0083111000      04090         
            [1]    000   009 11133337800              00813331111000    090           
                     00009 11333311233750            09711133333117800000    [7]            
                       0001133111333323 00  000000  0032333113333111000               
                         004312333400045 5000000100069680003321333800                 
                           09133336  9  711110 011117  3  023331 90                   
                            006  330001111111000111111150096543000                    
                              0006111111111111111111111111180060                      
                                 011111131111111111311111110                          
                           [0]   015551111131113111111311110   [6]                     
            [FRONT]              015 5111111111111111 OI 110              [BACK]                         
                                 015551131111311131131113110                          
                           [2]   011111111111111111111111110   [4]                     
                                 011111111311113111111311110                          
                              000911131111111111113111111318000                       
                            0003358000113111300011111118008555000                     
                          000313330  0    1110 0111177 9  033331000                   
                        00873331230000530000000000008640000332133 900                 
                      0087111333312333390   00000   003233321133311 900               
             [3]    000009 1111133311200             0033113333111100000    [5]             
                  00     009 1111133000               09 33311113000    00            
                00 000     009 111500                  009 1111000    05 000          
               00 7033       091790                      009 100        0  000        
              8  0  3333      3900                         0080      8000 0  00       
             0100  00 390 0  000                             00  003333300  0         
             0031   100 0  000                                 000000300  36 00       
            0   33339 0 0000                                    06 9 13  7733  00     
          00 00  39 00 000                                        000350 333  00 0    
            0  00 00                                                0 0000  00  0     
              0 000                                                       00 00       
               0                                                            00        

                          
*/

#include <esp_now.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>

#define SDA 14
#define SCL 15

// Command strings are only changed in loop(), never in the ESP-NOW callback.
String commandString;
String lastCommand = "";

// T-Deck ESP-NOW protocol (must match the transmitter exactly).
#define WIFI_CHANNEL 6
#define PACKET_MAGIC 0x5444
#define PROTOCOL_VERSION 1
#define FAILSAFE_MS 500

enum FrameType : uint8_t {
  FRAME_COMMAND = 1,
  FRAME_HEARTBEAT = 2
};

enum RoverCommand : uint8_t {
  CMD_STOP = 0,
  CMD_FORWARD = 1,
  CMD_BACKWARD = 2,
  CMD_LEFT = 3,
  CMD_RIGHT = 4,
  CMD_DANCE = 5,
  CMD_SQUAT = 6
};

struct __attribute__((packed)) RoverPacket {
  uint16_t magic;
  uint8_t version;
  uint8_t frameType;
  uint8_t command;
  uint8_t reserved;
  uint16_t sequence;
};

// Shared between ESP-NOW callback and loop(). 32-bit values are atomic on ESP32.
volatile int pendingCommand = -1;
volatile uint32_t lastPacketMs = 0;
volatile bool linkSeen = false;

// Set when a new command or link timeout interrupts a movement.
volatile bool abortMotion = false;
portMUX_TYPE commandMux = portMUX_INITIALIZER_UNLOCKED;
bool backwardMode = false;

// Last commanded logical positions, used for a gentle STOP recovery.
int servoPosition[8] = {
  90, 40, 90, 140, 90, 40, 90, 140
};

// Définir le GPIO pour la LED
const int ledPin = 33;

// Called this way, it uses the default address 0x40
Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver();

#define USMIN  500 // This is the rounded 'minimum' microsecond length based on the minimum pulse of 150
#define USMAX  2500 // This is the rounded 'maximum' microsecond length based on the maximum pulse of 600
#define SERVO_FREQ 50 // Analog servos run at ~50 Hz updates

const int microDelay = 10;

const int home_joint0 = 90;
const int home_joint1 = 40;
const int home_joint2 = 90;
const int home_joint3 = 140;
const int home_joint4 = 90;
const int home_joint5 = 40;
const int home_joint6 = 90;
const int home_joint7 = 140;


bool goMode = false;

// ESP-NOW callback: validate and copy only a small command value.
// Do not use String, Serial, delays, or servo functions in this callback.
// The signature differs between ESP32 Arduino core 2.x and 3.x.
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
void OnDataRecv(const esp_now_recv_info_t *info, const uint8_t *incomingData, int len) {
#else
void OnDataRecv(const uint8_t *mac, const uint8_t *incomingData, int len) {
#endif

    if (len != sizeof(RoverPacket)) {
        return;
    }

    RoverPacket packet;
    memcpy(&packet, incomingData, sizeof(packet));

    if (packet.magic != PACKET_MAGIC ||
        packet.version != PROTOCOL_VERSION) {
        return;
    }

    if (packet.frameType != FRAME_COMMAND &&
        packet.frameType != FRAME_HEARTBEAT) {
        return;
    }

    if (packet.command > CMD_SQUAT) {
        return;
    }

    lastPacketMs = millis();
    linkSeen = true;

    // Only execute commands from command frames.
    // Heartbeats keep the connection alive.
    if (packet.frameType == FRAME_COMMAND) {
        pendingCommand = packet.command;
        abortMotion = true;
    }
}


void setup() {
    Serial.begin(115200);
    
    Wire.begin(SDA, SCL);
    
    // Configurer le GPIO de la LED
    pinMode(ledPin, OUTPUT);
    digitalWrite(ledPin, LOW);

    // Mettre l'ESP32 en mode station and use the T-Deck channel.
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    esp_wifi_set_channel(WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE);

    // Print this board's MAC so it can be pasted into the remote's RECEIVER_MAC
    Serial.print("Rover MAC address: ");
    Serial.println(WiFi.macAddress());

    // Initialiser ESP-NOW
    if (esp_now_init() != ESP_OK) {
        Serial.println("Erreur lors de l'initialisation d'ESP-NOW");
        return;
    }
    // Enregistrer la fonction de callback de réception
    esp_now_register_recv_cb(OnDataRecv);
    // Setup commande = test 
    commandString = "test";

    pwm.begin();
    pwm.setOscillatorFrequency(27000000);
    pwm.setPWMFreq(SERVO_FREQ);  // Analog servos run at ~50 Hz updates
    
    delay(750);

    // Initialiser tous les servos à 90 degrés
    moveServo(0, home_joint0);
    moveServo(1, home_joint1);
    moveServo(2, home_joint2);
    moveServo(3, home_joint3);
    moveServo(4, home_joint4);
    moveServo(5, home_joint5);
    moveServo(6, home_joint6);
    moveServo(7, home_joint7);
    
    delay(750); // Attendre un peu pour que les servos atteignent la position initiale
    
    for (int i = 0; i < 30 ; i++) {
      moveServo(0,home_joint0+i);
      moveServo(2,home_joint2+i);
      moveServo(4,home_joint4+i);
      moveServo(6,home_joint6+i);
      delay(microDelay);
    }
    delay(microDelay);
    
    for (int i = 30; i > 0 ; i--) {
      moveServo(0,home_joint0+i);
      moveServo(2,home_joint2+i);
      moveServo(4,home_joint4+i);
      moveServo(6,home_joint6+i);
      delay(microDelay);
    }
    delay(microDelay); // Attendre un peu pour que les servos atteignent la position initiale
    
    for (int i = 0; i < 30 ; i++) {
      moveServo(0,home_joint0-i);
      moveServo(2,home_joint2-i);
      moveServo(4,home_joint4-i);
      moveServo(6,home_joint6-i);
      delay(microDelay);
    }
    delay(microDelay);
    
    for (int i = 30; i > 0 ; i--) {
      moveServo(0,home_joint0-i);
      moveServo(2,home_joint2-i);
      moveServo(4,home_joint4-i);
      moveServo(6,home_joint6-i);
      delay(microDelay);
    }
    delay(microDelay); // Attendre un peu pour que les servos atteignent la position initiale

}

// Check for a new command or a lost remote link during long servo routines.
bool motionMustAbort() {
    bool shouldAbort;
    uint32_t now = millis();

    portENTER_CRITICAL(&commandMux);

    if (pendingCommand >= 0) {
        abortMotion = true;
    }

    if (linkSeen && (uint32_t)(now - lastPacketMs) > FAILSAFE_MS) {
        // Queue one STOP and mark the link as timed out.
        pendingCommand = CMD_STOP;
        abortMotion = true;
        linkSeen = false;
    }

    shouldAbort = abortMotion;
    portEXIT_CRITICAL(&commandMux);

    return shouldAbort;
}

// Interruptible replacement for delay() inside movement routines.
void motionDelay(uint32_t durationMs) {
    uint32_t started = millis();
    while ((uint32_t)(millis() - started) < durationMs) {
        if (motionMustAbort()) return;
        delay(1);
    }
}

// Move one servo. The reverse gait mirrors the four horizontal leg servos
// around their calibrated home positions; lift servos are unchanged.
void moveServo(uint8_t servo, int newPos) {
    if (motionMustAbort()) return;

    if (backwardMode) {
        switch (servo) {
            case 0: newPos = 2 * home_joint0 - newPos; break;
            case 2: newPos = 2 * home_joint2 - newPos; break;
            case 4: newPos = 2 * home_joint4 - newPos; break;
            case 6: newPos = 2 * home_joint6 - newPos; break;
        }
    }

    newPos = constrain(newPos, 0, 180);
    servoPosition[servo] = newPos;
    pwm.writeMicroseconds(servo, map(newPos, 0, 180, USMIN, USMAX));
}

void stopmove() {
  backwardMode = false;

  if (goMode) {
    goMode = false;

    // Original anti-setup recovery from the walking posture.
    for (int i = 45; i > 15; i--) {
      moveServo(0, home_joint0-i);
      moveServo(6, home_joint6+i);
      motionDelay(microDelay);
    }
    for (int i = 15; i > 0; i--) {
      moveServo(0, home_joint0-i);
      moveServo(4, home_joint4+i);
      moveServo(2, home_joint2-i);
      moveServo(6, home_joint6+i);
      motionDelay(microDelay);
    }
  } else {
    // A STOP during dance/squat/turn must also recover to the home pose.
    const int home[8] = {
      home_joint0, home_joint1, home_joint2, home_joint3,
      home_joint4, home_joint5, home_joint6, home_joint7
    };
    int start[8];
    for (int s = 0; s < 8; s++) start[s] = servoPosition[s];

    for (int step = 1; step <= 30; step++) {
      for (int s = 0; s < 8; s++) {
        int target = start[s] + ((home[s] - start[s]) * step) / 30;
        moveServo(s, target);
      }
      motionDelay(microDelay);
    }
  }
}
        
// Fonction pour exécuter le mouvement "forward"
void forward() {
  
//LEG-ORDERS:
//  0-1, 4-5, 2-3, 6-7

//BACK: 0=-45, 2=+45, 4=+45, 6=-45
//FRONT 0=+45, 2=-45, 4=-45, 6=+45
//UP:   1=+45, 3=-45, 5=+45, 7=-45
//DOWN: 1=-45, 3=+45, 5=-45, 7=+45

//P1:
    for (int i = 0; i < 30 ; i++) {
      moveServo(1,home_joint1+(i*2));  //up
      moveServo(0, home_joint0-45+i);  //front
      moveServo(4, home_joint4+15+i);  //back
      moveServo(2, home_joint2-15+i);  //mid-back
      moveServo(6, home_joint6+45-i);  //mid-front
      motionDelay(microDelay);
    }
    for (int i = 0; i < 60 ; i++) {
      moveServo(0, home_joint0-15+i);  //front
      motionDelay(microDelay);
    }
    for (int i = 30; i > 0 ; i--) {
      moveServo(1,home_joint1+(i*2));  //down
      motionDelay(microDelay);
    }
    motionDelay(microDelay);
    
//P2:
    for (int i = 0; i < 30 ; i++) {
      moveServo(5,home_joint5+(i*2));  //up
      moveServo(0, home_joint0+45-i);  //mid-front
      moveServo(4, home_joint4+45-i);  //front
      moveServo(2, home_joint2+15+i);  //back
      moveServo(6, home_joint6+15-i);  //mid-back
      motionDelay(microDelay);
    }
    for (int i = 0; i < 60 ; i++) {
      moveServo(4, home_joint4+15-i);  //front
      motionDelay(microDelay);
    }
    for (int i = 30; i > 0 ; i--) {
      moveServo(5,home_joint5+(i*2));  //down
      motionDelay(microDelay);    
    }
    motionDelay(microDelay);


//P3:
    for (int i = 0; i < 30 ; i++) {
      moveServo(3,home_joint3-(i*2));  //up
      moveServo(0, home_joint0+15-i);  //mid-back
      moveServo(4, home_joint4-45+i);  //mid-front
      moveServo(2, home_joint2+45-i);  //front
      moveServo(6, home_joint6-15-i);  //back
      motionDelay(microDelay);
    }
    for (int i = 0; i < 60 ; i++) {
      moveServo(2, home_joint2+15-i);  //front
      motionDelay(microDelay);
    }
    for (int i = 30; i > 0 ; i--) {
      moveServo(3,home_joint3-(i*2));  //down
      motionDelay(microDelay);    
    }
    motionDelay(microDelay);

 //P4:
    for (int i = 0; i < 30 ; i++) {
      moveServo(7,home_joint7-(i*2));  //up
      moveServo(0, home_joint0-15-i);  //back
      moveServo(4, home_joint4-15+i);  //mid-back
      moveServo(2, home_joint2-45+i);  //mid-front
      moveServo(6, home_joint6-45+i);  //front
      motionDelay(microDelay);
    }
    for (int i = 0; i < 60 ; i++) {
      moveServo(6, home_joint6-15+i);  //front
      motionDelay(microDelay);
    }
    for (int i = 30; i > 0 ; i--) {
      moveServo(7,home_joint7-(i*2));  //down
      motionDelay(microDelay); }
       
    motionDelay(microDelay); 
    }

// Reverse walking uses the same four-phase gait with mirrored horizontal
// servo travel. Keep backwardMode enabled for the whole gait cycle.
void backward() {
    backwardMode = true;
    forward();
}

void loop() {
    // Transfer the latest ESP-NOW command into the existing command handler.
    // This is deliberately done here, not inside the ESP-NOW callback.
    int received = -1;
    portENTER_CRITICAL(&commandMux);
    received = pendingCommand;
    pendingCommand = -1;
    if (received >= 0) abortMotion = false;
    portEXIT_CRITICAL(&commandMux);

    if (received >= 0) {
        switch (received) {
            case CMD_STOP:     commandString = "stop"; break;
            case CMD_FORWARD:  commandString = "go"; break;
            case CMD_BACKWARD: commandString = "backward"; break;
            case CMD_LEFT:     commandString = "left"; break;
            case CMD_RIGHT:    commandString = "right"; break;
            case CMD_DANCE:    commandString = "dance"; break;
            case CMD_SQUAT:    commandString = "squat"; break;
        }
    }

    // Vérifier si une nouvelle commande a été reçue
    if (commandString != lastCommand) {
        lastCommand = commandString;
        Serial.print("Nouvelle commande reçue: ");
        Serial.println(commandString);

        if (commandString == "test") { ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
            stopmove();
            digitalWrite(ledPin, HIGH);
            for (int i = 0; i < 5; i++) {
                motionDelay(200);
                digitalWrite(ledPin, LOW);
                motionDelay(200);
                digitalWrite(ledPin, HIGH);
            }
            digitalWrite(ledPin, LOW);
            motionDelay(microDelay); 
            commandString = "";
            

        } else if (commandString == "right") {     ///////////////////////////////////////////////////////////////////////////////////////////////////////////////

            stopmove();

            for (int i = 0; i < 45 ; i++) {
              moveServo(0,home_joint0+i);
              moveServo(2,home_joint2+i);
              moveServo(4,home_joint4+i);
              moveServo(6,home_joint6+i);
              motionDelay(microDelay);
            }         

            motionDelay(microDelay); 

            //P1
            for (int i = 0; i < 60 ; i++) {
              moveServo(1,home_joint1+(i));
              motionDelay(microDelay); } //up

            for (int i = 45; i > 0 ; i--) {
              moveServo(0,home_joint0+i);
              motionDelay(microDelay);}

            for (int i = 60; i > 0 ; i--) {
              moveServo(1,home_joint1+(i)); 
              motionDelay(microDelay); }

            motionDelay(microDelay); 

            //P2: 
            for (int i = 0; i < 60 ; i++) {
              moveServo(5,home_joint5+(i));               
              motionDelay(microDelay); } //up

            for (int i = 45; i > 0 ; i--) {
              moveServo(4,home_joint4+i);
              motionDelay(microDelay);}

            for (int i = 60; i > 0 ; i--) {
              moveServo(5,home_joint5+(i)); 
              motionDelay(microDelay); }

            motionDelay(microDelay); 

            //P3:
            for (int i = 0; i < 60 ; i++) {
              moveServo(3,home_joint3-(i));   
              motionDelay(microDelay); } //up

            for (int i = 45; i > 0 ; i--) {
              moveServo(2,home_joint2+i);
              motionDelay(microDelay);}

            for (int i = 60; i > 0 ; i--) {
              moveServo(3,home_joint3-(i)); 
              motionDelay(microDelay); }

            motionDelay(microDelay); 

            //P4:
            for (int i = 0; i < 60 ; i++) {
              moveServo(7,home_joint7-(i)); 
              motionDelay(microDelay); } //up

            for (int i = 45; i > 0 ; i--) {
              moveServo(6,home_joint6+i);
              motionDelay(microDelay);}

            for (int i = 60; i > 0 ; i--) {
              moveServo(7,home_joint7-(i)); 
              motionDelay(microDelay); }


            motionDelay(microDelay); 
            commandString = "";

            } else if (commandString == "left") {     ///////////////////////////////////////////////////////////////////////////////////////////////////////////////

            stopmove();

            for (int i = 0; i < 45 ; i++) {
              moveServo(0,home_joint0-i);
              moveServo(2,home_joint2-i);
              moveServo(4,home_joint4-i);
              moveServo(6,home_joint6-i);
              motionDelay(microDelay);
            }         

            motionDelay(microDelay); 

            //P1
            for (int i = 0; i < 60 ; i++) {
              moveServo(1,home_joint1+(i));
              motionDelay(microDelay); } //up

            for (int i = 45; i > 0 ; i--) {
              moveServo(0,home_joint0-i);
              motionDelay(microDelay);}

            for (int i = 60; i > 0 ; i--) {
              moveServo(1,home_joint1+(i)); 
              motionDelay(microDelay); }

            motionDelay(microDelay); 

            //P2: 
            for (int i = 0; i < 60 ; i++) {
              moveServo(5,home_joint5+(i));               
              motionDelay(microDelay); } //up

            for (int i = 45; i > 0 ; i--) {
              moveServo(4,home_joint4-i);
              motionDelay(microDelay);}

            for (int i = 60; i > 0 ; i--) {
              moveServo(5,home_joint5+(i)); 
              motionDelay(microDelay); }

            motionDelay(microDelay); 

            //P3:
            for (int i = 0; i < 60 ; i++) {
              moveServo(3,home_joint3-(i));   
              motionDelay(microDelay); } //up

            for (int i = 45; i > 0 ; i--) {
              moveServo(2,home_joint2-i);
              motionDelay(microDelay);}

            for (int i = 60; i > 0 ; i--) {
              moveServo(3,home_joint3-(i)); 
              motionDelay(microDelay); }

            motionDelay(microDelay); 

            //P4:
            for (int i = 0; i < 60 ; i++) {
              moveServo(7,home_joint7-(i)); 
              motionDelay(microDelay); } //up

            for (int i = 45; i > 0 ; i--) {
              moveServo(6,home_joint6-i);
              motionDelay(microDelay);}

            for (int i = 60; i > 0 ; i--) {
              moveServo(7,home_joint7-(i)); 
              motionDelay(microDelay); }


            motionDelay(microDelay); 
            commandString = "";

            } else if (commandString == "dance") { ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
            stopmove();
            for (int i = 0; i < 30 ; i++) {
              moveServo(0,home_joint0+i);
              moveServo(2,home_joint2+i);
              moveServo(4,home_joint4+i);
              moveServo(6,home_joint6+i);
              motionDelay(microDelay);
            }
            motionDelay(microDelay);
            
            for (int i = 30; i > 0 ; i--) {
              moveServo(0,home_joint0+i);
              moveServo(2,home_joint2+i);
              moveServo(4,home_joint4+i);
              moveServo(6,home_joint6+i);
              motionDelay(microDelay);
            }
            motionDelay(microDelay); 
            
            for (int i = 0; i < 30 ; i++) {
              moveServo(0,home_joint0-i);
              moveServo(2,home_joint2-i);
              moveServo(4,home_joint4-i);
              moveServo(6,home_joint6-i);
              motionDelay(microDelay);
            }
            motionDelay(microDelay);
            
            for (int i = 30; i > 0 ; i--) {
              moveServo(0,home_joint0-i);
              moveServo(2,home_joint2-i);
              moveServo(4,home_joint4-i);
              moveServo(6,home_joint6-i);
              motionDelay(microDelay);
            }
            motionDelay(microDelay); 
            commandString = "";

            } else if (commandString == "squat") { ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
            stopmove();
            for (int i = 0; i < 60 ; i++) {
              moveServo(7,home_joint7-i);  //up
              moveServo(3,home_joint3-i);  //up
              moveServo(5,home_joint5+i);  //up
              moveServo(1,home_joint1+i);  //up
              motionDelay(microDelay);
            }      
            for (int i = 60; i > 0 ; i--) {
              moveServo(7,home_joint7-i);  //down
              moveServo(3,home_joint3-i);  //down
              moveServo(5,home_joint5+i);  //up
              moveServo(1,home_joint1+i);  //up      
              motionDelay(microDelay);    
            }  
            motionDelay(microDelay); 
            commandString = "";          
            
            } else if (commandString == "go") { ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
            stopmove();
            backwardMode = false;
            goMode = true;
            //SETUP:
            for (int i = 0; i < 15 ; i++) {
              moveServo(0, home_joint0-i);  //back
              moveServo(4, home_joint4+i);  //mid-back
              moveServo(2, home_joint2-i);  //mid-front
              moveServo(6, home_joint6+i);  //front
              motionDelay(microDelay);
            }
            for (int i = 15; i < 45 ; i++) {
              moveServo(0, home_joint0-i);  //back
              moveServo(6, home_joint6+i);  //front
              motionDelay(microDelay);
            }
          
            motionDelay(microDelay); 
            commandString = "";

        } else if (commandString == "backward") {
            stopmove();
            backwardMode = true;
            goMode = true;

            // Start from the same walking posture, mirrored by moveServo().
            for (int i = 0; i < 15; i++) {
              moveServo(0, home_joint0-i);
              moveServo(4, home_joint4+i);
              moveServo(2, home_joint2-i);
              moveServo(6, home_joint6+i);
              motionDelay(microDelay);
            }
            for (int i = 15; i < 45; i++) {
              moveServo(0, home_joint0-i);
              moveServo(6, home_joint6+i);
              motionDelay(microDelay);
            }
            commandString = "";

        } else if (commandString == "stop") { ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
            backwardMode = false;
            stopmove();
            digitalWrite(ledPin, LOW);

            motionDelay(microDelay); 
            commandString = "";

        }
    }

    // Exécuter l'action continue si en mode "go"
    if (goMode) {
        digitalWrite(ledPin, HIGH);
        if (backwardMode) {
            backward();
        } else {
            forward();
        }
        digitalWrite(ledPin, LOW);
    }
}
