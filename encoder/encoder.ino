#include "config.h"

#define DEGREE_DELIMIT 223

//Raw encoder ticks for each direction
int32_t encoder_alt = 0;
int32_t encoder_az = 0;

//Degrees, Minutes, Seconds for altitude
int16_t degrees_alt = 0;
int8_t arcminutes_alt = 0;
int8_t arcseconds_alt = 0;

//Hours, Minutes, Seconds for azimuth
int16_t hours_az = 0;
int8_t minutes_az = 0;
int8_t seconds_az = 0;

//Degrees, Minutes, Seconds for altitude of a specified object from Stellarium
int8_t degrees_alt_obj = 0;
int8_t arcminutes_alt_obj = 0;
int8_t arcseconds_alt_obj = 0;

//Hours, Minutes, Seconds for azimuth of a specified object from Stellarium
int8_t hours_az_obj = 0;
int8_t minutes_az_obj = 0;
int8_t seconds_az_obj = 0;

//Expected encoder position of the alt encoder when centered on the first guide star
double star1_alt = 0;
//Expected encoder position of the az encoder when centered on the first guide star
double star1_az = 0;

//Expected encoder position of the alt encoder when centered on the second guide star
double star2_alt = 0;
//Expected encoder position of the az encoder when centered on the second guide star
double star2_az = 0;

//Observed encoder position of alt encoder when centered on the first guide star
double star1_alt_obs = 0;
//Observed encoder position of the az encoder when centered on the first guide star
double star1_az_obs = 0;

//Observed encoder position of alt encoder when centered on the second guide star
double star2_alt_obs = 0;
//Observed encoder position of the az encoder when centered on the second guide star
double star2_az_obs = 0;

char in[15];
char tx_az[11];
char tx_alt[12];

void setup() {
  Serial.begin(9600);
  //Wait for serial to be ready
  while(!Serial);
  //Setup the encoder pins with a built in pullup resistor so no external pullups are needed
  pinMode(ENCODER_ALT_A, INPUT_PULLUP);
  pinMode(ENCODER_ALT_B, INPUT_PULLUP);
  pinMode(ENCODER_AZ_A, INPUT_PULLUP);
  pinMode(ENCODER_AZ_B, INPUT_PULLUP);
  //Jump to the a method when the corresponding pin's state changes.
  attachInterrupt(digitalPinToInterrupt(ENCODER_ALT_A), encoderAltA, CHANGE);
  attachInterrupt(digitalPinToInterrupt(ENCODER_ALT_B), encoderAltB, CHANGE);
  attachInterrupt(digitalPinToInterrupt(ENCODER_AZ_A), encoderAzA, CHANGE);
  attachInterrupt(digitalPinToInterrupt(ENCODER_AZ_B), encoderAzB, CHANGE);
  
  align();
}

//Reads a character after waiting for Serial to be available.
static inline uint8_t serialReadBlocking() {
    while (!Serial.available());
    return Serial.read();
}


uint32_t ONE_ROTATION = 2400 * RATIO_AZ;
int32_t actual_enc_az;    
//Align the scope by picking two stars, aiming the scope at them, then slewing to them in stellarium
void align() {
  uint8_t counter = 0;
  while(1) {
    //Encoder ticks to degrees, arcminutes, arcseconds for altitude measurement
    degrees_alt = encoder_alt * 360 / (RATIO_ALT * 2400.0);
    arcminutes_alt = fmod(encoder_alt * 360 / (RATIO_ALT * 2400.0) * 60,60);
    arcseconds_alt = (int) (fmod(encoder_alt * 360 / (RATIO_ALT * 2400.0) * 3600,60) + .5) % 60;

    actual_enc_az = ONE_ROTATION-encoder_az;
    //Encoder ticks to hours, minutes, seconds for azimuth measurement
    //Serial.println(encoder_az);
    uint32_t
    hours_az = actual_enc_az * 24 / (RATIO_ALT * 2400.0);
    minutes_az = fmod(actual_enc_az * 24 / (RATIO_AZ * 2400.0) * 60,60);
    seconds_az = (int) (fmod(actual_enc_az * 24 / (RATIO_AZ * 2400.0) * 3600,60) + .5) % 60;

    snprintf(tx_alt, 11, "+%02d%c%02d:%02d#", degrees_alt, DEGREE_DELIMIT, arcminutes_alt, arcseconds_alt);
    //Serial.println(tx_alt);
    
    //Wait for serial connection with computer to be available
    if(Serial.available()) {
      //Read a byte and see if it is a colon, which indicates a command
      if (serialReadBlocking()==':') {
        //Read the rest of the command, commands end with #
        int size = Serial.readBytesUntil('#', in, 14);
        in[size]='\0';
        //Tell Stellarium our azimuth when prompted
        if(!strncmp(in, "GR", 2)) {
          //Print our azimuth angle in the format of HH:MM:SS
          snprintf(tx_az, 10, "%02d:%02d:%02d#", hours_az, minutes_az, seconds_az);
          Serial.print(tx_az);
        }
        //Tell Stellarium our altitude when prompted
        else if(!strncmp(in, "GD", 2)) {
          //The tracking process will hang if we give stellarium a negative alt, so correct for that
          if(encoder_alt<0) {
            snprintf(tx_alt, 11, "+%02d%c%02d:%02d#", 0, DEGREE_DELIMIT, 0, 0);
          }
          else {
            snprintf(tx_alt, 11, "+%02d%c%02d:%02d#", degrees_alt, DEGREE_DELIMIT, arcminutes_alt, arcseconds_alt);
          }
          Serial.print(tx_alt);
        }
        else if(!strncmp(in, "Sr", 2)) {
          char hours[3];
          strncpy(hours, in+3, 2);
          hours[2]='\0';
          char *endptr;
          hours_az_obj = strtol(hours, &endptr, 10);  
          if(*endptr!='\0') {
            //If it is not null, tell Stellarium that the slew was invalid
            Serial.print("0");
            return;
          } 
          char arcmin[3];
          strncpy(arcmin, in+6, 2);
          arcmin[2]='\0';
          minutes_az_obj = strtol(arcmin, &endptr, 10);  
          if(*endptr!='\0') {
            //If it is not null, tell Stellarium that the slew was invalid
            Serial.print("0");
            return;
          }
          char arcsec[3];
          strncpy(arcsec, in+9, 2);
          arcsec[2]='\0';
          seconds_az_obj = strtol(arcsec, &endptr, 10);
          if(*endptr!='\0') {
            //If it is not null, tell Stellarium that the slew was invalid
            Serial.print("0");
            return;
          }
          //If we've reached this point, then we've properly read in all the numbers, so the slew was valid
          Serial.print("1");
          if(counter==0) {
            //Serial.println("Star 1 Az Received");
            star1_az = ((hours_az_obj) + (minutes_az_obj/60.0) + (seconds_az_obj/3600.0)) * (RATIO_ALT*2400.0)/(24);
          }
          else {
            //Serial.println("Star 2 Az Received");
            star2_az = ((hours_az_obj) + (minutes_az_obj/60.0) + (seconds_az_obj/3600.0)) * (RATIO_ALT*2400.0)/(24);
          }
        }
        //Get the altitude of the object from Stellarium, if given
        else if(!strncmp(in, "Sd", 2)) {
          //Get the sign of the altitude (should always be positive, so +)
          char sign = in[3];
          char degrees[3];
          strncpy(degrees, in+4, 2);
          degrees[2]='\0';
          char *endptr;
          degrees_alt_obj = strtol(degrees, &endptr, 10);  
          if(*endptr!='\0') {
            //If it is not null, tell Stellarium that the slew was invalid
            Serial.print("0");
            return;
          }
          char arcmin[3];
          strncpy(arcmin, in+7, 2);
          arcmin[2]='\0';
          arcminutes_alt_obj = strtol(arcmin, &endptr, 10);  
          if(*endptr!='\0') {
            //If it is not null, tell Stellarium that the slew was invalid
            Serial.print("0");
            return;
          }
          char arcsec[3];
          strncpy(arcsec, in+10, 2);
          arcsec[2]='\0';
          arcseconds_alt_obj = strtol(arcsec, &endptr, 10);
          if(*endptr!='\0') {
            //If it is not null, tell Stellarium that the slew was invalid
            Serial.print("0");
            return;
          }
          //If we've reached this point, then we've properly read in all the numbers, so the slew was valid
          Serial.print("1");
          if(counter==0) {
            //Serial.println("Star 1 Noted");
            star1_alt = ((degrees_alt_obj) + (arcminutes_alt_obj/60.0) + (arcseconds_alt_obj/3600.0)) * (RATIO_ALT*2400.0)/(360);
            counter++;
          }
          else {
            star2_alt = ((degrees_alt_obj) + (arcminutes_alt_obj/60.0) + (arcseconds_alt_obj/3600.0)) * (RATIO_ALT*2400.0)/(360);
            encoder_az = star2_az;
            encoder_alt = star2_alt;
            //Serial.println("Align complete!");
            return;
          }
        }
        //Tell Stellarium that the slew is possible (object is above horizon) when prompted
        else if(!strncmp(in, "MS", 2)) {
          Serial.print("0");
        }
      }
    }
  }
}

void loop() {
  //Encoder ticks to degrees, arcminutes, arcseconds for altitude measurement
  degrees_alt = encoder_alt * 360 / (RATIO_ALT * 2400.0);
  arcminutes_alt = fmod(encoder_alt * 360 / (RATIO_ALT * 2400.0) * 60,60);
  arcseconds_alt = (int) (fmod(encoder_alt * 360 / (RATIO_ALT * 2400.0) * 3600,60) + .5) % 60;

  //Encoder ticks to hours, minutes, seconds for azimuth measurement
  hours_az = encoder_az * 24 / (RATIO_ALT * 2400.0);
  minutes_az = fmod(encoder_az * 24 / (RATIO_AZ * 2400.0) * 60,60);
  seconds_az = (int) (fmod(encoder_az * 24 / (RATIO_AZ * 2400.0) * 3600,60) + .5) % 60;
  
  //Wait for serial connection with computer to be available
  if(Serial.available()) {
    //Read a byte and see if it is a colon, which indicates a command
    if (serialReadBlocking()==':') {
      //Read the rest of the command, commands end with #
      int size = Serial.readBytesUntil('#', in, 14);
      in[size]='\0';
      //Tell Stellarium our azimuth when prompted
      if(!strncmp(in, "GR", 2)) {
        //Print our azimuth angle in the format of HH:MM:SS
        snprintf(tx_az, 10, "%02d:%02d:%02d#", hours_az, minutes_az, seconds_az);
        Serial.print(tx_az);
      }
      //Tell Stellarium our altitude when prompted
      else if(!strncmp(in, "GD", 2)) {
        snprintf(tx_alt, 11, "+%02d%c%02d:%02d#", degrees_alt, DEGREE_DELIMIT, arcminutes_alt, arcseconds_alt);
        Serial.print(tx_alt);
      }
      //Get the azimuth of the object from Stellarium, if given
      else if(!strncmp(in, "Sr", 2)) {
        char hours[3];
        strncpy(hours, in+3, 2);
        hours[2]='\0';
        char *endptr;
        hours_az_obj = strtol(hours, &endptr, 10);  
        if(*endptr!='\0') {
          //If it is not null, tell Stellarium that the slew was invalid
          Serial.print("0");
          return;
        }
        char arcmin[3];
        strncpy(arcmin, in+6, 2);
        arcmin[2]='\0';
        minutes_az_obj = strtol(arcmin, &endptr, 10);  
        if(*endptr!='\0') {
          //If it is not null, tell Stellarium that the slew was invalid
          Serial.print("0");
          return;
        }
        char arcsec[3];
        strncpy(arcsec, in+9, 2);
        arcsec[2]='\0';
        seconds_az_obj = strtol(arcsec, &endptr, 10);
        if(*endptr!='\0') {
          //If it is not null, tell Stellarium that the slew was invalid
          Serial.print("0");
          return;
        }
        //If we've reached this point, then we've properly read in all the numbers, so the slew was valid
        Serial.print("1");
      }
      //Get the altitude of the object from Stellarium, if given
      else if(!strncmp(in, "Sd", 2)) {
        //Get the sign of the altitude (should always be positive, so +)
        char sign = in[3];
        char degrees[3];
        strncpy(degrees, in+4, 2);
        degrees[2]='\0';
        char *endptr;
        degrees_alt_obj = strtol(degrees, &endptr, 10);  
        if(*endptr!='\0') {
          //If it is not null, tell Stellarium that the slew was invalid
          Serial.print("0");
          return;
        }
        char arcmin[3];
        strncpy(arcmin, in+7, 2);
        arcmin[2]='\0';
        arcminutes_alt_obj = strtol(arcmin, &endptr, 10);  
        if(*endptr!='\0') {
          //If it is not null, tell Stellarium that the slew was invalid
          Serial.print("0");
          return;
        }
        char arcsec[3];
        strncpy(arcsec, in+10, 2);
        arcsec[2]='\0';
        arcseconds_alt_obj = strtol(arcsec, &endptr, 10);
        if(*endptr!='\0') {
          //If it is not null, tell Stellarium that the slew was invalid
          Serial.print("0");
          return;
        }
        //If we've reached this point, then we've properly read in all the numbers, so the slew was valid
        Serial.print("1");
      }
      //Tell Stellarium that the slew is possible (object is above horizon) when prompted
      else if(!strncmp(in, "MS", 2)) {
        Serial.print("0");
      }
    }
  }
}

//Method for pin change of ENCODER_ALT_A
void encoderAltA() {
  encoder_alt += digitalRead(ENCODER_ALT_A) == digitalRead(ENCODER_ALT_B) ? -1 : 1;
}

//Method for pin change of ENCODER_ALT_B
void encoderAltB() {
  encoder_alt += digitalRead(ENCODER_ALT_A) != digitalRead(ENCODER_ALT_B) ? -1 : 1;
}

//Method for pin change of ENCODER_AZ_A
void encoderAzA() {
  encoder_az += digitalRead(ENCODER_AZ_A) == digitalRead(ENCODER_AZ_B) ? -1 : 1;
}

//Method for pin change of ENCODER_AZ_B
void encoderAzB() {
  encoder_az += digitalRead(ENCODER_AZ_A) != digitalRead(ENCODER_AZ_B) ? -1 : 1;
}
