/*
  NEXTION TFT related code, based on EasyNextion library by Seithan / Athanasios Seitanis (https://github.com/Seithan/EasyNextionLibrary)
  The trigger(s) functions at the end are called by the Nextion library on event (buttons, page change).

  Completely reworked and simplified. These functions only update Nextion variables. The display is then updated localy by the
  Nextion itself.

  Remove every usages of String in order to avoid duplication, fragmentation and random crashes.
  Note usage of snprintf_P which uses a fmt string that resides in program memory.
*/
#include <Arduino.h>
#include "Config.h"
#include "PoolMaster.h"
#include "EasyNextionLibrary.h"

#ifdef NEXTION_V2

#define GLOBAL  "globals" // Name of Nextion page to store global variables

static volatile int CurrentPage = 0;
static volatile bool TFT_ON = true;           // display status

static char temp[32];
static unsigned long LastAction = 0; // Last action time done on TFT. Go to sleep after TFT_SLEEP
static char HourBuffer[9];
static char DateBuffer[11];

// Variables used to refresh buttons only on status change
static bool TFT_Automode = false;
static bool TFT_Filt = false;
static bool TFT_Robot = false;
static bool TFT_R0 = false;
static bool TFT_R1 = false;
static bool TFT_Winter = false;
static bool TFT_Electro = false; // ajout
static bool TFT_Electro_Mode = false; // ajout
static bool TFT_pHPIDEnabled = false; // ajout
static bool TFT_OrpPIDEnabled = false; // ajout

//Nextion TFT object. Choose which ever Serial port
//you wish to connect to (not "Serial" which is used for debug), here Serial2 UART
static EasyNex myNex(Serial2);

// Functions prototypes
void InitTFT(void);
void ResetTFT(void);
void UpdateTFT(void);
void UpdateWiFi(bool);

//Reset TFT at start of controller - Change transmission rate to 115200 bauds on both side (Nextion then ESP)
//could have been not in HMI file, but it is good to know that after reset the Nextion goes back to 9600 bauds
void ResetTFT()
{
  myNex.begin(115200);
  myNex.writeStr("sleep=0");
  myNex.writeStr(F("rest"));
  myNex.writeStr(F("wup=9")); // Exit from sleep on page 9 Loading
  delay(1000);
}

void InitTFT()
{
  myNex.writeNum(F(GLOBAL".vaMode.val"), storage.AutoMode);
  myNex.writeNum(F(GLOBAL".vaElectrolyse.val"), storage.ElectrolyseMode);
  myNex.writeNum(F(GLOBAL".vaFilt.val"), 0);
  myNex.writeNum(F(GLOBAL".vaRobot.val"), 0);
  myNex.writeNum(F(GLOBAL".vaR0.val"), 0);
  myNex.writeNum(F(GLOBAL".vaR1.val"), 0);
  myNex.writeNum(F(GLOBAL".vaR2.val"), 0);
  myNex.writeStr(F(GLOBAL".vaMCFW.txt"), FIRMW);
  myNex.writeStr(F(GLOBAL".vaTFTFW.txt"), TFT_FIRMW);
}

void UpdateWiFi(bool wifi){
  if(wifi){
    snprintf_P(temp,sizeof(temp),PSTR("WiFi: %s"),WiFi.SSID().c_str());
    myNex.writeStr(F(GLOBAL".vaSSID.txt"),temp);
    snprintf_P(temp,sizeof(temp),PSTR("IP: %s"),WiFi.localIP().toString());
    myNex.writeStr(F(GLOBAL".vaIP.txt"),temp);
  } else
  {
    myNex.writeStr(F(GLOBAL".vaSSID.txt"),"Not connected");
    myNex.writeStr(F(GLOBAL".vaIP.txt"),"");
  } 
}

//Function to update TFT display
//update the global variables of the TFT + the widgets of the active page
//call this function at least every second to ensure fluid display
void UpdateTFT()
{
  // Has any button been touched? If yes, one of the trigger routines
  // will fire
  myNex.NextionListen();  

  // Updates done only if TFT ON, useless (and not taken into account) if not
  if(TFT_ON)
  {
    sprintf(HourBuffer, PSTR("%02d:%02d:%02d"), hour(), minute(), second());
    myNex.writeStr(F(GLOBAL".vaTime.txt"),HourBuffer);
    sprintf(DateBuffer, PSTR("%02d/%02d/%04d"), day(), month(), year());
    myNex.writeStr(F(GLOBAL".vaDate.txt"),DateBuffer);
    myNex.writeNum(F(GLOBAL".vaNetW.val"),MQTTConnection ? 1:0);
    snprintf_P(temp,sizeof(temp),PSTR("%02d/%02dh"),storage.FiltrationStart,storage.FiltrationStop);
    myNex.writeStr(F(GLOBAL".vaStaSto.txt"),temp);

    // Do not update status during 1s after a change to allow the system to reflect it in real life
    if ((unsigned long)(millis() - LastAction) > 1000)
    {
      // Update all bistable switches
      if(TFT_Automode != storage.AutoMode) 
      { myNex.writeNum(F(GLOBAL".vaMode.val"),storage.AutoMode);
        TFT_Automode = storage.AutoMode;
      }
      if(TFT_Filt != FiltrationPump.IsRunning())
      { TFT_Filt = FiltrationPump.IsRunning();
        myNex.writeNum(F(GLOBAL".vaFilt.val"), TFT_Filt);
      }
      if(TFT_Robot != RobotPump.IsRunning())
      { TFT_Robot = RobotPump.IsRunning();
        myNex.writeNum(F(GLOBAL".vaRobot.val"), TFT_Robot);
      }
      if(TFT_R0 != RELAYR0.IsActive())
      { TFT_R0 = RELAYR0.IsActive();
        myNex.writeNum(F(GLOBAL".vaR0.val"), TFT_R0);
      }
      if(TFT_R1 != RELAYR1.IsActive())
      { TFT_R1 = RELAYR1.IsActive();
        myNex.writeNum(F(GLOBAL".vaR1.val"), TFT_R1);
      }
      if(TFT_Winter != storage.WinterMode) 
      { myNex.writeNum(F(GLOBAL".vaWinter.val"),storage.WinterMode);
        TFT_Winter = storage.WinterMode;
      }  
      if(TFT_Electro != OrpProd.IsRunning())
      { TFT_Electro = OrpProd.IsRunning();
        myNex.writeNum(F(GLOBAL".vaElectroOn.val"), TFT_Electro);
      } 
      if(TFT_Electro_Mode != storage.ElectrolyseMode)
      { TFT_Electro_Mode = storage.ElectrolyseMode;
        myNex.writeNum(F(GLOBAL".vaElectrolyse.val"), TFT_Electro_Mode);
      } 
      if(TFT_pHPIDEnabled != storage.pHPIDEnabled)
      { TFT_pHPIDEnabled = storage.pHPIDEnabled;
        myNex.writeNum(F(GLOBAL".vapHPIDEnable.val"), TFT_R0);
      }
      if(TFT_OrpPIDEnabled != storage.OrpPIDEnabled)
      { TFT_OrpPIDEnabled = storage.OrpPIDEnabled;
        myNex.writeNum(F(GLOBAL".vaOrpPIDEnable.val"), TFT_R0);
      }
    }

    myNex.writeNum(F(GLOBAL".vapHLevel.val"),PhPump.TankLevel() ? 0:1);      
    myNex.writeNum(F(GLOBAL".vaChlLevel.val"),ChlPump.TankLevel() ? 0:1);
    myNex.writeNum(F(GLOBAL".vaPSIErr.val"),PSIError ? 1:0);
    myNex.writeNum(F(GLOBAL".vaChlUTErr.val"),ChlPump.UpTimeError ? 1:0);
    myNex.writeNum(F(GLOBAL".vapHUTErr.val"),PhPump.UpTimeError ? 1:0);  

//    if(CurrentPage==0)
//    {
      snprintf_P(temp,sizeof(temp),PSTR("%4.2f"),storage.PhValue);      
      myNex.writeStr(F(GLOBAL".vapH.txt"),temp);
      snprintf_P(temp,sizeof(temp),PSTR("%3.0f"),storage.OrpValue);      
      myNex.writeStr(F(GLOBAL".vaOrp.txt"),temp);
      snprintf_P(temp,sizeof(temp),PSTR("%3.1f"),storage.Ph_SetPoint);
      myNex.writeStr(F(GLOBAL".vapHSP.txt"),temp);
      snprintf_P(temp,sizeof(temp),PSTR("%3.0f"),storage.Orp_SetPoint);      
      myNex.writeStr(F(GLOBAL".vaOrpSP.txt"),temp);
      snprintf_P(temp,sizeof(temp),PSTR("%4.1f"),storage.TempValue); 
      myNex.writeStr(F("pageHome.vaWT.txt"),temp);
      snprintf_P(temp,sizeof(temp),PSTR("%4.1f"),storage.TempExternal);      
      myNex.writeStr(F("pageHome.vaAT.txt"),temp);
      snprintf_P(temp,sizeof(temp),PSTR("%4.2f"),storage.PSIValue);   
      myNex.writeStr(F("pageHome.vaPSI.txt"),temp);
      //snprintf_P(temp,sizeof(temp),PSTR("%d%% / %4.1fmin"),(int)round(PhPump.GetTankFill()),float(PhPump.UpTime)/1000./60.);
      snprintf_P(temp,sizeof(temp),PSTR("%4.1fmn"),float(PhPump.UpTime)/1000./60.);
      myNex.writeStr(F("pageHome.vapHTk.txt"),temp);
      //snprintf_P(temp,sizeof(temp),PSTR("%d%% / %4.1fmin"),(int)round(ChlPump.GetTankFill()),float(ChlPump.UpTime)/1000./60.);
      snprintf_P(temp,sizeof(temp),PSTR("%4.1fmn"),float(ChlPump.UpTime)/1000./60.);
      myNex.writeStr(F("pageHome.vaChlTk.txt"),temp);

      myNex.writeNum(F("pageHome.vapHGauge.val"), (int)(round(PhPump.GetTankFill())));
      myNex.writeNum(F("pageHome.vaChlGauge.val"), (int)(round(ChlPump.GetTankFill())));
      myNex.writeNum(F("pageHome.vapHPID.val"), (PhPID.GetMode() == AUTOMATIC)? 1 : 0);
      myNex.writeNum(F("pageHome.vaOrpPID.val"), (OrpPID.GetMode() == AUTOMATIC)? 1 : 0);
      myNex.writeNum(F("pageHome.vapHInject.val"), (PhPump.IsRunning())? 1 : 0);
      myNex.writeNum(F("pageHome.vaChlInject.val"), (ChlPump.IsRunning())? 1 : 0);

      if(abs(storage.PhValue-storage.Ph_SetPoint) <= 0.1) 
        myNex.writeNum(F("pageHome.vapHErr.val"),0);
      if(abs(storage.PhValue-storage.Ph_SetPoint) > 0.1 && abs(storage.PhValue-storage.Ph_SetPoint) <= 0.2)  
        myNex.writeNum(F("pageHome.vapHErr.val"),1);
      if(abs(storage.PhValue-storage.Ph_SetPoint) > 0.2)  
        myNex.writeNum(F("pageHome.vapHErr.val"),2);
      if(abs(storage.OrpValue-storage.Orp_SetPoint) <= 20.) 
        myNex.writeNum(F("pageHome.vaOrpErr.val"),0);
      if(abs(storage.OrpValue-storage.Orp_SetPoint) > 20. && abs(storage.OrpValue-storage.Orp_SetPoint) <= 40.)  
        myNex.writeNum(F("pageHome.vaOrpErr.val"),1);
      if(abs(storage.OrpValue-storage.Orp_SetPoint) > 40.)  
        myNex.writeNum(F("pageHome.vaOrpErr.val"),2);        
//    }

    if(CurrentPage == 1)
    { // All updates for page 1 are page independant at the beginning of this section
    } 

    if(CurrentPage == 2) 
    {  UpdateWiFi(WiFi.status() == WL_CONNECTED);
      myNex.writeStr(F(GLOBAL".vaMCFW.txt"),FIRMW);
      myNex.writeStr(F(GLOBAL".vaTFTFW.txt"),TFT_FIRMW);
      /*snprintf_P(temp,sizeof(temp),PSTR("%4.2f"),storage.PhValue);      
      myNex.writeStr(F(GLOBAL".vapH.txt"),temp);
      snprintf_P(temp,sizeof(temp),PSTR("%3.0f"),storage.OrpValue);      
      myNex.writeStr(F(GLOBAL".vaOrp.txt"),temp);
      snprintf_P(temp,sizeof(temp),PSTR("(%3.1f)"),storage.Ph_SetPoint);
      myNex.writeStr(F(GLOBAL".vapHSP.txt"),temp);
      snprintf_P(temp,sizeof(temp),PSTR("(%3.0f)"),storage.Orp_SetPoint);      
      myNex.writeStr(F(GLOBAL".vaOrpSP.txt"),temp);*/

      myNex.writeNum(F(GLOBAL".vaElectroSec.val"), storage.SecureElectro);
      myNex.writeNum(F(GLOBAL".vaElectroDelay.val"), storage.DelayElectro);
    }  

    //put TFT in sleep mode with wake up on touch and force page 0 load to trigger an event
    if((unsigned long)(millis() - LastAction) >= TFT_SLEEP && TFT_ON && CurrentPage !=4)
    {
      myNex.writeStr(F("thup=1"));
      myNex.writeStr(F("wup=9"));     // Wake up on page 9 Loading
      myNex.writeStr(F("sleep=1"));
      TFT_ON = false;
    }
  }  
}

//Page 0 has finished loading
//printh 23 02 54 01
void trigger1()
{
  CurrentPage = 0;
  if(!TFT_ON)
  {
    UpdateWiFi(WiFi.status() == WL_CONNECTED);
    TFT_ON = true;  
  }
  LastAction = millis();
}

//Page 1 has finished loading
//printh 23 02 54 02
void trigger2()
{
  CurrentPage = 1;
  LastAction = millis();
}

//Page 2 has finished loading
//printh 23 02 54 03
void trigger3()
{
  CurrentPage = 2;
  LastAction = millis();  
}

//Page 3 has finished loading
//printh 23 02 54 04
//void trigger4()
//{
//  CurrentPage = 3;
//  LastAction = millis();
//}

//MODE button was toggled
//printh 23 02 54 05
void trigger5()
{
  char Cmd[100] = "{\"Mode\":1}";
  if(storage.AutoMode) Cmd[8] = '0';
  xQueueSendToBack(queueIn,&Cmd,0);
  LastAction = millis();
}

//FILT button was toggled
//printh 23 02 54 06
void trigger6()
{
  char Cmd[100] = "{\"FiltPump\":1}";
  if(FiltrationPump.IsRunning()) Cmd[12] = '0';
  else TFT_Filt = true;
  xQueueSendToBack(queueIn,&Cmd,0);
  LastAction = millis();
}

//Robot button was toggled
//printh 23 02 54 07
void trigger7()
{
  char Cmd[100] = "{\"RobotPump\":1}";
  if(RobotPump.IsRunning()) Cmd[13] = '0';
  else TFT_Robot = true;
  xQueueSendToBack(queueIn,&Cmd,0);
  LastAction = millis();
}

//Relay 0 button was toggled
//printh 23 02 54 08
void trigger8()
{
  char Cmd[100] = "{\"Relay\":[0,1]}";
  if(RELAYR0.IsActive()) Cmd[12] = '0';
  else TFT_R0 = true; 
  xQueueSendToBack(queueIn,&Cmd,0);
  LastAction = millis();
}

//Relay 1 button was toggled
//printh 23 02 54 09
void trigger9()
{
  char Cmd[100] = "{\"Relay\":[1,1]}";
  if (RELAYR1.IsActive()) Cmd[12] = '0';
  else TFT_R1 = true;
  xQueueSendToBack(queueIn, &Cmd, 0);
  LastAction = millis();
}

//Winter button was toggled
//printh 23 02 54 0A
void trigger10()
{
  char Cmd[100] = "{\"Winter\":1}";
  if(storage.WinterMode) Cmd[10] = '0';
  else TFT_Winter = true;
  xQueueSendToBack(queueIn,&Cmd,0);
  LastAction = millis();
}

//Probe calibration completed or new pH, Orp or Water Temp setpoints or New tank
//printh 23 02 54 0B
void trigger11()
{
  char Cmd[100] = "";
  strcpy(Cmd,myNex.readStr(F(GLOBAL".vaCommand.txt")).c_str());
  xQueueSendToBack(queueIn,&Cmd,0);
  Debug.print(DBG_VERBOSE,"Nextion cal page command: %s",Cmd);
  LastAction = millis();
}

//Clear Errors button pressed
//printh 23 02 54 0C
void trigger12()
{
  char Cmd[100] = "{\"Clear\":1}";
  xQueueSendToBack(queueIn,&Cmd,0);
  LastAction = millis();
}

//pH PID button pressed
//printh 23 02 54 0D
void trigger13()
{
  char Cmd[100] = "{\"PhPID\":1}";
  if(PhPID.GetMode() == 1) Cmd[9] = '0';
  xQueueSendToBack(queueIn,&Cmd,0);
  LastAction = millis();
}

//Orp PID button pressed
//printh 23 02 54 0E
void trigger14()
{
  char Cmd[100] = "{\"OrpPID\":1}";
  if(OrpPID.GetMode() == 1) Cmd[10] = '0';
  xQueueSendToBack(queueIn,&Cmd,0);
  LastAction = millis();
}

// Electrolyse option switched
// printh 23 02 54 0F
void trigger15()
{
  char Cmd[100] = "{\"Electrolyse\":1}";
  if (OrpProd.IsRunning())  Cmd[15] = '0';
  else TFT_Electro = true;
  xQueueSendToBack(queueIn, &Cmd, 0);
  LastAction = millis();
}

//  pH Operation Mode option switched (normal or PID)
// printh 23 02 54 10
void trigger16()
{
  char Cmd[100] = "{\"PhPIDEnabled\":1}";
  if (storage.pHPIDEnabled) Cmd[16] = '0';
  else TFT_pHPIDEnabled = true;
  xQueueSendToBack(queueIn, &Cmd, 0);
  LastAction = millis();
}

//  Orp Operating Mode option switched (normal or PID)
// printh 23 02 54 11
void trigger17()
{
  char Cmd[100] = "{\"OrpPIDEnabled\":1}";
  if (storage.OrpPIDEnabled) Cmd[17] = '0';
  else TFT_OrpPIDEnabled = true;
  xQueueSendToBack(queueIn, &Cmd, 0);
  LastAction = millis();
}

//  Electrolyse Operating Mode option switched (Electrolyser or not)
// printh 23 02 54 12
void trigger18()
{
  char Cmd[100] = "{\"ElectrolyseMode\":1}";
  if (storage.ElectrolyseMode) Cmd[19] = '0';
  xQueueSendToBack(queueIn, &Cmd, 0);
  LastAction = millis();
}

#endif