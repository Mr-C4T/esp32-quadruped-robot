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
#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>

#define SDA 14
#define SCL 15

// Buffer pour stocker la commande reçue
char incomingCommand[250];
String commandString;
String lastCommand = "";

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

// Fonction de callback pour la réception des données
void OnDataRecv(const esp_now_recv_info *recv_info, const uint8_t *incomingData, int len) {
    memcpy(incomingCommand, incomingData, len);
    incomingCommand[len] = '\0';  // Assurer que la chaîne est terminée
    commandString = String(incomingCommand);
}

void setup() {
    Serial.begin(115200);
    
    Wire.begin(SDA, SCL);
    
    // Configurer le GPIO de la LED
    pinMode(ledPin, OUTPUT);
    digitalWrite(ledPin, LOW);

    // Mettre l'ESP32 en mode station
    WiFi.mode(WIFI_STA);

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

// Fonction pour déplacer les servomoteurs vers une position spécifique avec une animation fluide
void moveServo(uint8_t servo, int newPos) {
    pwm.writeMicroseconds(servo, map(newPos, 0, 180, USMIN, USMAX));
}

void stopmove() {

  if (goMode == true){
      goMode = false;  // Activer le mode "go"
         //ANTI-SETUP:
            for (int i = 45; i > 15 ; i--) {
              moveServo(0, home_joint0-i);  //back
              moveServo(6, home_joint6+i);  //front
              delay(microDelay);
            }
            for (int i = 15; i > 0 ; i--) {
              moveServo(0, home_joint0-i);  //back
              moveServo(4, home_joint4+i);  //mid-back
              moveServo(2, home_joint2-i);  //mid-front
              moveServo(6, home_joint6+i);  //front
              delay(microDelay);
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
      delay(microDelay);
    }
    for (int i = 0; i < 60 ; i++) {
      moveServo(0, home_joint0-15+i);  //front
      delay(microDelay);
    }
    for (int i = 30; i > 0 ; i--) {
      moveServo(1,home_joint1+(i*2));  //down
      delay(microDelay);
    }
    delay(microDelay);
    
//P2:
    for (int i = 0; i < 30 ; i++) {
      moveServo(5,home_joint5+(i*2));  //up
      moveServo(0, home_joint0+45-i);  //mid-front
      moveServo(4, home_joint4+45-i);  //front
      moveServo(2, home_joint2+15+i);  //back
      moveServo(6, home_joint6+15-i);  //mid-back
      delay(microDelay);
    }
    for (int i = 0; i < 60 ; i++) {
      moveServo(4, home_joint4+15-i);  //front
      delay(microDelay);
    }
    for (int i = 30; i > 0 ; i--) {
      moveServo(5,home_joint5+(i*2));  //down
      delay(microDelay);    
    }
    delay(microDelay);


//P3:
    for (int i = 0; i < 30 ; i++) {
      moveServo(3,home_joint3-(i*2));  //up
      moveServo(0, home_joint0+15-i);  //mid-back
      moveServo(4, home_joint4-45+i);  //mid-front
      moveServo(2, home_joint2+45-i);  //front
      moveServo(6, home_joint6-15-i);  //back
      delay(microDelay);
    }
    for (int i = 0; i < 60 ; i++) {
      moveServo(2, home_joint2+15-i);  //front
      delay(microDelay);
    }
    for (int i = 30; i > 0 ; i--) {
      moveServo(3,home_joint3-(i*2));  //down
      delay(microDelay);    
    }
    delay(microDelay);

 //P4:
    for (int i = 0; i < 30 ; i++) {
      moveServo(7,home_joint7-(i*2));  //up
      moveServo(0, home_joint0-15-i);  //back
      moveServo(4, home_joint4-15+i);  //mid-back
      moveServo(2, home_joint2-45+i);  //mid-front
      moveServo(6, home_joint6-45+i);  //front
      delay(microDelay);
    }
    for (int i = 0; i < 60 ; i++) {
      moveServo(6, home_joint6-15+i);  //front
      delay(microDelay);
    }
    for (int i = 30; i > 0 ; i--) {
      moveServo(7,home_joint7-(i*2));  //down
      delay(microDelay); }
       
    delay(microDelay); 
    }

void loop() {
    // Vérifier si une nouvelle commande a été reçue
    if (commandString != lastCommand) {
        lastCommand = commandString;
        Serial.print("Nouvelle commande reçue: ");
        Serial.println(commandString);

        if (commandString == "test") { ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
            stopmove();
            digitalWrite(ledPin, HIGH);
            for (int i = 0; i < 5; i++) {
                delay(200);
                digitalWrite(ledPin, LOW);
                delay(200);
                digitalWrite(ledPin, HIGH);
            }
            digitalWrite(ledPin, LOW);
            delay(microDelay); 
            commandString = "";
            

        } else if (commandString == "right") {     ///////////////////////////////////////////////////////////////////////////////////////////////////////////////

            stopmove();

            for (int i = 0; i < 45 ; i++) {
              moveServo(0,home_joint0+i);
              moveServo(2,home_joint2+i);
              moveServo(4,home_joint4+i);
              moveServo(6,home_joint6+i);
              delay(microDelay);
            }         

            delay(microDelay); 

            //P1
            for (int i = 0; i < 60 ; i++) {
              moveServo(1,home_joint1+(i));
              delay(microDelay); } //up

            for (int i = 45; i > 0 ; i--) {
              moveServo(0,home_joint0+i);
              delay(microDelay);}

            for (int i = 60; i > 0 ; i--) {
              moveServo(1,home_joint1+(i)); 
              delay(microDelay); }

            delay(microDelay); 

            //P2: 
            for (int i = 0; i < 60 ; i++) {
              moveServo(5,home_joint5+(i));               
              delay(microDelay); } //up

            for (int i = 45; i > 0 ; i--) {
              moveServo(4,home_joint4+i);
              delay(microDelay);}

            for (int i = 60; i > 0 ; i--) {
              moveServo(5,home_joint5+(i)); 
              delay(microDelay); }

            delay(microDelay); 

            //P3:
            for (int i = 0; i < 60 ; i++) {
              moveServo(3,home_joint3-(i));   
              delay(microDelay); } //up

            for (int i = 45; i > 0 ; i--) {
              moveServo(2,home_joint2+i);
              delay(microDelay);}

            for (int i = 60; i > 0 ; i--) {
              moveServo(3,home_joint3-(i)); 
              delay(microDelay); }

            delay(microDelay); 

            //P4:
            for (int i = 0; i < 60 ; i++) {
              moveServo(7,home_joint7-(i)); 
              delay(microDelay); } //up

            for (int i = 45; i > 0 ; i--) {
              moveServo(6,home_joint6+i);
              delay(microDelay);}

            for (int i = 60; i > 0 ; i--) {
              moveServo(7,home_joint7-(i)); 
              delay(microDelay); }


            delay(microDelay); 
            commandString = "";

            } else if (commandString == "left") {     ///////////////////////////////////////////////////////////////////////////////////////////////////////////////

            stopmove();

            for (int i = 0; i < 45 ; i++) {
              moveServo(0,home_joint0-i);
              moveServo(2,home_joint2-i);
              moveServo(4,home_joint4-i);
              moveServo(6,home_joint6-i);
              delay(microDelay);
            }         

            delay(microDelay); 

            //P1
            for (int i = 0; i < 60 ; i++) {
              moveServo(1,home_joint1+(i));
              delay(microDelay); } //up

            for (int i = 45; i > 0 ; i--) {
              moveServo(0,home_joint0-i);
              delay(microDelay);}

            for (int i = 60; i > 0 ; i--) {
              moveServo(1,home_joint1+(i)); 
              delay(microDelay); }

            delay(microDelay); 

            //P2: 
            for (int i = 0; i < 60 ; i++) {
              moveServo(5,home_joint5+(i));               
              delay(microDelay); } //up

            for (int i = 45; i > 0 ; i--) {
              moveServo(4,home_joint4-i);
              delay(microDelay);}

            for (int i = 60; i > 0 ; i--) {
              moveServo(5,home_joint5+(i)); 
              delay(microDelay); }

            delay(microDelay); 

            //P3:
            for (int i = 0; i < 60 ; i++) {
              moveServo(3,home_joint3-(i));   
              delay(microDelay); } //up

            for (int i = 45; i > 0 ; i--) {
              moveServo(2,home_joint2-i);
              delay(microDelay);}

            for (int i = 60; i > 0 ; i--) {
              moveServo(3,home_joint3-(i)); 
              delay(microDelay); }

            delay(microDelay); 

            //P4:
            for (int i = 0; i < 60 ; i++) {
              moveServo(7,home_joint7-(i)); 
              delay(microDelay); } //up

            for (int i = 45; i > 0 ; i--) {
              moveServo(6,home_joint6-i);
              delay(microDelay);}

            for (int i = 60; i > 0 ; i--) {
              moveServo(7,home_joint7-(i)); 
              delay(microDelay); }


            delay(microDelay); 
            commandString = "";

            } else if (commandString == "dance") { ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
            stopmove();
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
            delay(microDelay); 
            
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
            delay(microDelay); 
            commandString = "";

            } else if (commandString == "squat") { ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
            stopmove();
            for (int i = 0; i < 60 ; i++) {
              moveServo(7,home_joint7-i);  //up
              moveServo(3,home_joint3-i);  //up
              moveServo(5,home_joint5+i);  //up
              moveServo(1,home_joint1+i);  //up
              delay(microDelay);
            }      
            for (int i = 60; i > 0 ; i--) {
              moveServo(7,home_joint7-i);  //down
              moveServo(3,home_joint3-i);  //down
              moveServo(5,home_joint5+i);  //up
              moveServo(1,home_joint1+i);  //up      
              delay(microDelay);    
            }  
            delay(microDelay); 
            commandString = "";          
            
            } else if (commandString == "go") { ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
            goMode = true;  // Activer le mode "go"       
            //SETUP:
            for (int i = 0; i < 15 ; i++) {
              moveServo(0, home_joint0-i);  //back
              moveServo(4, home_joint4+i);  //mid-back
              moveServo(2, home_joint2-i);  //mid-front
              moveServo(6, home_joint6+i);  //front
              delay(microDelay);
            }
            for (int i = 15; i < 45 ; i++) {
              moveServo(0, home_joint0-i);  //back
              moveServo(6, home_joint6+i);  //front
              delay(microDelay);
            }
          
            delay(microDelay); 
            commandString = "";

        } else if (commandString == "stop") { ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
            stopmove();
            digitalWrite(ledPin, LOW);

            delay(microDelay); 
            commandString = "";

        }
    }

    // Exécuter l'action continue si en mode "go"
    if (goMode) {
        digitalWrite(ledPin, HIGH);
        forward();  // Appel à la fonction forward avec un pas
        digitalWrite(ledPin, LOW);
    }
}  
