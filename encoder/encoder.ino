#include "config.h"

#define DEGREE_DELIMIT 223

//Raw encoder ticks for each direction
int32_t encoder_alt = 0;
int32_t encoder_az = 0;

//Corrected encoder ticks for each direction
int32_t encoder_alt_corrected = 0;
int32_t encoder_az_corrected = 0;

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

//Expected encoder position of the alt encoder when centered on the third guide star
double star3_alt = 0;
//Expected encoder position of the az encoder when centered on the third guide star
double star3_az = 0;

//Observed encoder position of alt encoder when centered on the first guide star
double star1_alt_obs = 0;
//Observed encoder position of the az encoder when centered on the first guide star
double star1_az_obs = 0;

//Observed encoder position of alt encoder when centered on the second guide star
double star2_alt_obs = 0;
//Observed encoder position of the az encoder when centered on the second guide star
double star2_az_obs = 0;

//Observed encoder position of alt encoder when centered on the third guide star
double star3_alt_obs = 0;
//Observed encoder position of the az encoder when centered on the third guide star
double star3_az_obs = 0;

double p11, p22, p33 = 1;
double p12, p13, p21, p23, p31, p32 = 0;;


char in[15];
char tx_az[11];
char tx_alt[12];

void setup() {
  Serial.begin(9600);
  Serial3.begin(9600);
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
    int32_t cur_encoder_alt = encoder_alt;
    int32_t cur_encoder_az = encoder_az;
    //Encoder ticks to degrees, arcminutes, arcseconds for altitude measurement
    degrees_alt = cur_encoder_alt * 360 / (RATIO_ALT * 2400.0);
    arcminutes_alt = fmod(cur_encoder_alt * 360 / (RATIO_ALT * 2400.0) * 60,60);
    arcseconds_alt = (int) (fmod(cur_encoder_alt * 360 / (RATIO_ALT * 2400.0) * 3600,60) + .5) % 60;

    actual_enc_az = ONE_ROTATION-cur_encoder_az;
    //Encoder ticks to hours, minutes, seconds for azimuth measurement
    //Serial.println(encoder_az);
    hours_az = actual_enc_az * 24 / (RATIO_ALT * 2400.0);
    minutes_az = fmod(actual_enc_az * 24 / (RATIO_AZ * 2400.0) * 60,60);
    seconds_az = (int) (fmod(actual_enc_az * 24 / (RATIO_AZ * 2400.0) * 3600,60) + .5) % 60;

    //snprintf(tx_alt, 11, "+%02d%c%02d:%02d#", degrees_alt, DEGREE_DELIMIT, arcminutes_alt, arcseconds_alt);
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
            continue;
          } 
          char arcmin[3];
          strncpy(arcmin, in+6, 2);
          arcmin[2]='\0';
          minutes_az_obj = strtol(arcmin, &endptr, 10);  
          if(*endptr!='\0') {
            //If it is not null, tell Stellarium that the slew was invalid
            Serial.print("0");
            continue;
          }
          char arcsec[3];
          strncpy(arcsec, in+9, 2);
          arcsec[2]='\0';
          seconds_az_obj = strtol(arcsec, &endptr, 10);
          if(*endptr!='\0') {
            //If it is not null, tell Stellarium that the slew was invalid
            Serial.print("0");
            continue;
          }
          //If we've reached this point, then we've properly read in all the numbers, so the slew was valid
          Serial.print("1");
          if(counter==0) {
            //Serial.println("Star 1 Az Received");
            star1_az = ((hours_az_obj) + (minutes_az_obj/60.0) + (seconds_az_obj/3600.0)) * (RATIO_ALT*2400.0)/(24);
            star1_az_obs = actual_enc_az;
          }
          else if(counter==1){
            //Serial.println("Star 2 Az Received");
            star2_az = ((hours_az_obj) + (minutes_az_obj/60.0) + (seconds_az_obj/3600.0)) * (RATIO_ALT*2400.0)/(24);
            star2_az_obs = actual_enc_az;
          }
          else {
            //Serial.println("Star 3 Az Received");
            star3_az = ((hours_az_obj) + (minutes_az_obj/60.0) + (seconds_az_obj/3600.0)) * (RATIO_ALT*2400.0)/(24);
            star3_az_obs = actual_enc_az;
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
            continue;
          }
          char arcmin[3];
          strncpy(arcmin, in+7, 2);
          arcmin[2]='\0';
          arcminutes_alt_obj = strtol(arcmin, &endptr, 10);  
          if(*endptr!='\0') {
            //If it is not null, tell Stellarium that the slew was invalid
            Serial.print("0");
            continue;
          }
          char arcsec[3];
          strncpy(arcsec, in+10, 2);
          arcsec[2]='\0';
          arcseconds_alt_obj = strtol(arcsec, &endptr, 10);
          if(*endptr!='\0') {
            //If it is not null, tell Stellarium that the slew was invalid
            Serial.print("0");
            continue;
          }
          //If we've reached this point, then we've properly read in all the numbers, so the slew was valid
          Serial.print("1");
          if(counter==0) {
            //Serial.println("Star 1 Noted");
            star1_alt = ((degrees_alt_obj) + (arcminutes_alt_obj/60.0) + (arcseconds_alt_obj/3600.0)) * (RATIO_ALT*2400.0)/(360);
            star1_alt_obs = cur_encoder_alt;
            Serial.println("\n" + String(star1_az) + ", " + String(star1_alt));
            counter++;
          }
          else if(counter==1){
            star2_alt = ((degrees_alt_obj) + (arcminutes_alt_obj/60.0) + (arcseconds_alt_obj/3600.0)) * (RATIO_ALT*2400.0)/(360);
            star2_alt_obs = cur_encoder_alt;
            counter++;
            //Serial.println("Star 2 Noted!");
          }
          else {
            star3_alt = ((degrees_alt_obj) + (arcminutes_alt_obj/60.0) + (arcseconds_alt_obj/3600.0)) * (RATIO_ALT*2400.0)/(360);
            star3_alt_obs = cur_encoder_alt;
            Serial.println("\n" + String(star3_az) + ", " + String(star3_alt));
            //Create the aligning matrix thing
            create_transformation_matrix();
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
  transform_encoder_readings();
  //Encoder ticks to degrees, arcminutes, arcseconds for altitude measurement
  int32_t cur_encoder_alt_corrected = encoder_alt_corrected;
  int32_t cur_encoder_az_corrected = encoder_az_corrected;
  
  degrees_alt = cur_encoder_alt_corrected * 360 / (RATIO_ALT * 2400.0);
  arcminutes_alt = fmod(cur_encoder_alt_corrected * 360 / (RATIO_ALT * 2400.0) * 60,60);
  arcseconds_alt = (int) (fmod(cur_encoder_alt_corrected * 360 / (RATIO_ALT * 2400.0) * 3600,60) + .5) % 60;

  actual_enc_az = ONE_ROTATION-cur_encoder_az_corrected;
  
  //Encoder ticks to hours, minutes, seconds for azimuth measurement
  hours_az = actual_enc_az * 24 / (RATIO_ALT * 2400.0);
  minutes_az = fmod(actual_enc_az * 24 / (RATIO_AZ * 2400.0) * 60,60);
  seconds_az = (int) (fmod(actual_enc_az * 24 / (RATIO_AZ * 2400.0) * 3600,60) + .5) % 60;

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

//Apply transformation to encoder readings before sending to stellarium
void transform_encoder_readings() {
  uint32_t actual_enc_az = ONE_ROTATION-encoder_az;
  uint32_t cur_enc_alt = encoder_alt;
  //Serial.println(String(cur_enc_alt) + " ticks alt, " + String(actual_enc_az) + " ticks az");

  p11=1;
  p12=0;
  p13=0;
  p21=0;
  p22=1;
  p23=0;
  p31=0;
  p32=0;
  p33=1;
  //Locally convert star variables to radians
  double encoder_alt_rad = cur_enc_alt/2400.0/RATIO_ALT * 2 * PI;
  double encoder_az_rad = actual_enc_az/2400.0/RATIO_AZ * 2 * PI;

  //Serial.println(String(encoder_alt_rad) + " rads alt, " + String(encoder_az_rad) + " rads az");
  
  //Generate the spherical coordinates for the current encoder readings
  double x = cos(encoder_az_rad) * cos(encoder_alt_rad);
  double y = sin(encoder_az_rad) * cos(encoder_alt_rad);
  double z = sin(encoder_alt_rad);

  //Perform the matrix hit (apply transformation)
  double corrected_x = p11*x + p12*y + p13*z;
  double corrected_y = p21*x + p22*y + p23*z;
  double corrected_z = p31*x + p32*y + p33*z;

  //Convert back to radian angles
  //Arcsine will work since z is always positive
  double corrected_alt_rad = asin(corrected_z);
  //We need to do casework to determine the right angle
  //Quadrant 1
  double corrected_az_rad;
  if (x>0 && y>0) {
    corrected_az_rad = acos(corrected_x/cos(corrected_alt_rad));
  }
  //Quadrant 2
  else if (x<0 && y>0) {
    corrected_az_rad = acos(corrected_x/cos(corrected_alt_rad));
  }
  //Quadrant 3
  else if (x<0 && y<0) {
    corrected_az_rad = PI - asin(corrected_y/cos(corrected_alt_rad));
  }
  //Quadrant 4
  else {
    corrected_az_rad = 2 * PI + asin(corrected_y/cos(corrected_alt_rad));
  }
  
  //Serial.println(String(corrected_alt_rad) + " rads alt corrected, " + String(corrected_az_rad) + " rads az corrected");
  //Convert back to encoder ticks
  encoder_alt_corrected = corrected_alt_rad/(2 * PI) * 2400 * RATIO_ALT;
  encoder_az_corrected = corrected_az_rad/(2 * PI) * 2400 * RATIO_AZ;
}

//Create the matrix that will correct for any bad alignment (bad levelling, etc.)
void create_transformation_matrix() {
  //Locally convert star angles to radians
  double star1_alt_rad = star1_alt/2400.0/RATIO_ALT * 2 * PI;
  double star1_az_rad = star1_az/2400.0/RATIO_AZ * 2 * PI;
  
  double star2_alt_rad = star2_alt/2400.0/RATIO_ALT * 2 * PI;
  double star2_az_rad = star2_az/2400.0/RATIO_AZ * 2 * PI;

  double star3_alt_rad = star3_alt/2400.0/RATIO_ALT * 2 * PI;
  double star3_az_rad = star3_az/2400.0/RATIO_AZ * 2 * PI;
  
  double star1_alt_obs_rad = star1_alt_obs/2400.0/RATIO_ALT * 2 * PI;
  double star1_az_obs_rad = star1_az_obs/2400.0/RATIO_AZ * 2 * PI;

  double star2_alt_obs_rad = star2_alt_obs/2400.0/RATIO_ALT * 2 * PI;
  double star2_az_obs_rad = star2_az_obs/2400.0/RATIO_AZ * 2 * PI;

  double star3_alt_obs_rad = star3_alt_obs/2400.0/RATIO_ALT * 2 * PI;
  double star3_az_obs_rad = star3_az_obs/2400.0/RATIO_AZ * 2 * PI;
  
  /*Generate the spherical coordinates for each star
   * x = cos(az) * cos(alt)
   * y = sin(az) * cos(alt)
   * z = sin(alt)
   */
  double s1a = cos(star1_az_rad) * cos(star1_alt_rad);
  double s1b = sin(star1_az_rad) * cos(star1_alt_rad);
  double s1c = sin(star1_alt_rad);

  double s1d = cos(star1_az_obs_rad) * cos(star1_alt_obs_rad);
  double s1e = sin(star1_az_obs_rad) * cos(star1_alt_obs_rad);
  double s1f = sin(star1_alt_obs_rad);

  double s2a = cos(star2_az_rad) * cos(star2_alt_rad);
  double s2b = sin(star2_az_rad) * cos(star2_alt_rad);
  double s2c = sin(star2_alt_rad);

  double s2d = cos(star2_az_obs_rad) * cos(star2_alt_obs_rad);
  double s2e = sin(star2_az_obs_rad) * cos(star2_alt_obs_rad);
  double s2f = sin(star2_alt_obs_rad);

  double s3a = cos(star3_az_rad) * cos(star3_alt_rad);
  double s3b = sin(star3_az_rad) * cos(star3_alt_rad);
  double s3c = sin(star3_alt_rad);

  double s3d = cos(star3_az_obs_rad) * cos(star3_alt_obs_rad);
  double s3e = sin(star3_az_obs_rad) * cos(star3_alt_obs_rad);
  double s3f = sin(star3_alt_obs_rad);

  /*
   * From these, construct the matrix:
   * {p11, p12, p13}
   * {p21, p22, p23}
   * {p31, p32, p33}
   * Solved from the following system of equations:
   * Matrix.{s1d,s1e,s1f}=={s1a,s1b,s1c}
   * Matrix.{s2d,s2e,s2f}=={s2a,s2b,s2c}
   * Matrix.{s3d,s3e,s3f}=={s3a,s3b,s3c}
   */
   
  p11 = (s1f*s2e*s3a - s1e*s2f*s3a - s1f*s2a*s3e + s1a*s2f*s3e + s1e*s2a*s3f - s1a*s2e*s3f)/(s1f*s2e*s3d - s1e*s2f*s3d - s1f*s2d*s3e + s1d*s2f*s3e + s1e*s2d*s3f - s1d*s2e*s3f);
  p12 = (s1f*s2d*s3a - s1d*s2f*s3a - s1f*s2a*s3d + s1a*s2f*s3d + s1d*s2a*s3f - s1a*s2d*s3f)/(-(s1f*s2e*s3d) + s1e*s2f*s3d + s1f*s2d*s3e - s1d*s2f*s3e - s1e*s2d*s3f + s1d*s2e*s3f);
  p13 = (s1e*s2d*s3a - s1d*s2e*s3a - s1e*s2a*s3d + s1a*s2e*s3d + s1d*s2a*s3e - s1a*s2d*s3e)/(s1f*s2e*s3d - s1e*s2f*s3d - s1f*s2d*s3e + s1d*s2f*s3e + s1e*s2d*s3f - s1d*s2e*s3f);
  p21 = (s1f*s2e*s3b - s1e*s2f*s3b - s1f*s2b*s3e + s1b*s2f*s3e + s1e*s2b*s3f - s1b*s2e*s3f)/(s1f*s2e*s3d - s1e*s2f*s3d - s1f*s2d*s3e + s1d*s2f*s3e + s1e*s2d*s3f - s1d*s2e*s3f);
  p22 = (s1f*s2d*s3b - s1d*s2f*s3b - s1f*s2b*s3d + s1b*s2f*s3d + s1d*s2b*s3f - s1b*s2d*s3f)/(-(s1f*s2e*s3d) + s1e*s2f*s3d + s1f*s2d*s3e - s1d*s2f*s3e - s1e*s2d*s3f + s1d*s2e*s3f);
  p23 = (s1e*s2d*s3b - s1d*s2e*s3b - s1e*s2b*s3d + s1b*s2e*s3d + s1d*s2b*s3e - s1b*s2d*s3e)/(s1f*s2e*s3d - s1e*s2f*s3d - s1f*s2d*s3e + s1d*s2f*s3e + s1e*s2d*s3f - s1d*s2e*s3f);
  p31 = (s1f*s2e*s3c - s1e*s2f*s3c - s1f*s2c*s3e + s1c*s2f*s3e + s1e*s2c*s3f - s1c*s2e*s3f)/(s1f*s2e*s3d - s1e*s2f*s3d - s1f*s2d*s3e + s1d*s2f*s3e + s1e*s2d*s3f - s1d*s2e*s3f);
  p32 = (s1f*s2d*s3c - s1d*s2f*s3c - s1f*s2c*s3d + s1c*s2f*s3d + s1d*s2c*s3f - s1c*s2d*s3f)/(-(s1f*s2e*s3d) + s1e*s2f*s3d + s1f*s2d*s3e - s1d*s2f*s3e - s1e*s2d*s3f + s1d*s2e*s3f);
  p33 = (s1e*s2d*s3c - s1d*s2e*s3c - s1e*s2c*s3d + s1c*s2e*s3d + s1d*s2c*s3e - s1c*s2d*s3e)/(s1f*s2e*s3d - s1e*s2f*s3d - s1f*s2d*s3e + s1d*s2f*s3e + s1e*s2d*s3f - s1d*s2e*s3f);
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
