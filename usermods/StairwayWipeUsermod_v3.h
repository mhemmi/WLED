#include "wled.h"

/*
 * Usermods allow you to add own functionality to WLED more easily
 * See: https://github.com/Aircoookie/WLED/wiki/Add-own-functionality
 * 
 * This is Stairway-Wipe as a v2 usermod.
 * 
 * Using this usermod:
 * 1. Copy the usermod into the sketch folder (same folder as wled00.ino)
 * 2. Register the usermod by adding #include "stairway-wipe-usermod-v2.h" in the top and registerUsermod(new StairwayWipeUsermod()) in the bottom of usermods_list.cpp
 * 
 * 
 * Marvin modification:
 * 1. If WLED boots: nothing should happen (done via preset after booting, but has to be tested)
 * 2. if PIR detects motion: wipe white with full brightness in
 * 3. if partymode on and PIR detects motion: wipe in with current color [but if someone restart WLED go to mode 2]
 * 4. in every case: wipe out in that state as you wiped in
 * 
 */

class StairwayWipeUsermod : public Usermod {
  private:
    //Private class members. You can declare variables and functions only accessible to your usermod here
    unsigned long lastTime = 0;
    byte wipeState = 0; //0: inactive 1: wiping 2: solid
    unsigned long timeStaticStart = 0;
    uint16_t previousUserVar0 = 0;
    const uint8_t PIRsensorPinTop = 13;
    const uint8_t PIRsensorPinBottom = 16;
    byte PIRstatus = LOW;
    uint8_t bottomSegment = 10;
    uint8_t topSegment = 11;
    int late = 19;
    uint16_t maximumLeds = 150; //maxmium amount of leds
    bool wiped = false; //: t: already dimmed f: we have to dimm the leds


//comment this out if you want the turn off effect to be just fading out instead of reverse wipe
#define STAIRCASE_WIPE_OFF
  public:
    void setup(){
      //initialize pir sensors
      pinMode(PIRsensorPinTop,INPUT); //D7
      pinMode(PIRsensorPinBottom,INPUT); //D8
    }

    void loop() {
      if(nightlightDelayMins == 42){
        return;
      }
      time_t time = now();
      int hr = hour(localTime);
      if(hr >= late){ //if it is late activate nightlight
        if(wipeState == 0 && !wiped){ //check if wiping is inactive and we run for 1st time      
          effectCurrent = FX_MODE_STATIC;
          //default lenght of first and last step
          strip.setSegment(topSegment,maximumLeds-6,maximumLeds); //default top step
          strip.setSegment(bottomSegment,0,6); //default bottom step
          //set default color to white (255)
          strip.getSegment(topSegment).colors[0] = ((255 << 24) | ((0&0xFF) << 16) | ((0&0xFF) << 8) | ((0&0xFF)));  
          strip.getSegment(bottomSegment).colors[0] = ((255 << 24) | ((0&0xFF) << 16) | ((0&0xFF) << 8) | ((0&0xFF)));  
          //((myWhite << 24) | ((myRed&0xFF) << 16) | ((myGreen&0xFF) << 8) | ((myBlue&0xFF))); see https://github.com/Aircoookie/WLED/wiki/Add-own-functionality
          //colorUpdated(NOTIFIER_CALL_MODE_DIRECT_CHANGE);
          strip.getSegment(0).setOption(SEG_OPTION_ON,0);
          transitionDelayTemp = 10000; //10 sec
          strip.getSegment(topSegment).setOption(SEG_OPTION_ON,1);
          strip.getSegment(bottomSegment).setOption(SEG_OPTION_ON,1);
          wiped = true; //dimm enabled
          bri = 2;
          colorUpdated(NOTIFIER_CALL_MODE_DIRECT_CHANGE);
        }
      }

    if(digitalRead(PIRsensorPinBottom) == HIGH){ //bottom sensor detect movement
      //userVar0 = 1; //movement on bottom detected
      //userVar1 = 100; //default value if we don't recognize a second pir movement
    }
    if(digitalRead(PIRsensorPinTop) == HIGH){ //check movement on top
      //userVar0 = 2; //movement on top detected
      //userVar1 = 100; //default value if we don't recognize a second pir movement
    }

  //userVar0 (U0 in HTTP API):
  //has to be set to 1 if movement is detected on the PIR that is the same side of the staircase as the ESP8266
  //has to be set to 2 if movement is detected on the PIR that is the opposite side
  //can be set to 0 if no movement is detected. Otherwise LEDs will turn off after a configurable timeout (userVar1 seconds)
  //curl "http://wled2.local/win&U0=2&U1=5&SX=200&W=255"

  if (userVar0 > 0)
  {
    wiped = false;
    if ((previousUserVar0 == 1 && userVar0 == 2) || (previousUserVar0 == 2 && userVar0 == 1)) wipeState = 3; //turn off if other PIR triggered
    previousUserVar0 = userVar0;
    
    if (wipeState == 0) {
      strip.getSegment(0).setOption(SEG_OPTION_ON,1);
      strip.setSegment(bottomSegment,0,0);
      strip.setSegment(topSegment,0,0);
      startWipe(); //pir detects movement
      wipeState = 1; //we are wiping
    } else if (wipeState == 1) { //wiping
      uint32_t cycleTime = 360 + (255 - effectSpeed)*75; //this is how long one wipe takes (minus 25 ms to make sure we switch in time)
      if (millis() + strip.timebase > (cycleTime - 25)) { //wipe complete
        effectCurrent = FX_MODE_STATIC;
        timeStaticStart = millis();
        colorUpdated(NOTIFIER_CALL_MODE_NOTIFICATION);
        wipeState = 2;
      }
    } else if (wipeState == 2) { //static
      if (userVar1 > 0) //if U1 is not set, the light will stay on until second PIR or external command is triggered
      {
        if (millis() - timeStaticStart > userVar1*1000) wipeState = 3;
      }
    } else if (wipeState == 3) { //switch to wipe off
      #ifdef STAIRCASE_WIPE_OFF
      effectCurrent = FX_MODE_COLOR_WIPE;
      strip.timebase = 360 + (255 - effectSpeed)*75 - millis(); //make sure wipe starts fully lit
      colorUpdated(NOTIFIER_CALL_MODE_NOTIFICATION);
      wipeState = 4;
      #else
      turnOff();
      #endif
    } else { //wiping off
      if (millis() + strip.timebase > (725 + (255 - effectSpeed)*150)) turnOff(); //wipe complete
    }
  } else {
    wipeState = 0; //reset for next time
    if (previousUserVar0) {
      #ifdef STAIRCASE_WIPE_OFF
      userVar0 = previousUserVar0;
      wipeState = 3;
      #else
      turnOff();
      #endif
    }
    previousUserVar0 = 0;
  }
}
    
    uint16_t getId()
    {
      return USERMOD_ID_EXAMPLE;
    }

    void startWipe()
    {
    //bri = briLast; //turn on
    bri = 255; //we need maximum brightness /Mavin modification
    transitionDelayTemp = 100; //no transition
    effectCurrent = FX_MODE_COLOR_WIPE;
    resetTimebase(); //make sure wipe starts from beginning

    //set wipe direction
    WS2812FX::Segment& seg = strip.getSegment(0);
    bool doReverse = (userVar0 == 2);
    seg.setOption(1, doReverse);
    effectSpeed = 188; //marvin modification
    col[0] = 0; //red 0
    col[1] = 0; //green 0
    col[2] = 0; //blue 0
    col[3] = 255; //white 100%
    colorUpdated(NOTIFIER_CALL_MODE_NOTIFICATION);
    }

    void turnOff()
    {
    #ifdef STAIRCASE_WIPE_OFF
    transitionDelayTemp = 0; //turn off immediately after wipe completed
    #else
    transitionDelayTemp = 4000; //fade out slowly
    #endif
    bri = 0;
    colorUpdated(NOTIFIER_CALL_MODE_NOTIFICATION);
    wipeState = 0;
    userVar0 = 0;
    previousUserVar0 = 0;
    }

   //More methods can be added in the future, this example will then be extended.
   //Your usermod will remain compatible as it does not need to implement all methods from the Usermod base class!
};
