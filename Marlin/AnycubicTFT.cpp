/*
  AnycubicTFT.cpp  --- Support for Anycubic i3 Mega TFT
  Created by Christian Hopp on 09.12.17.
    
  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.
        
  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.
                
  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/


#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include "Arduino.h"

#include "MarlinConfig.h"
#include "cardreader.h"
#include "planner.h"
#include "temperature.h"
#include "language.h"
#include "stepper.h"
#include "serial.h"

#ifdef ANYCUBIC_TFT_MODEL
#include "AnycubicTFT.h"
#include "AnycubicSerial.h"

char _conv[8];

char *itostr2(const uint8_t &x)
{
  //sprintf(conv,"%5.1f",x);
  int xx=x;
  _conv[0]=(xx/10)%10+'0';
  _conv[1]=(xx)%10+'0';
  _conv[2]=0;
  return _conv;
}

AnycubicTFTClass::AnycubicTFTClass() {
}

void AnycubicTFTClass::Setup() {
  AnycubicSerial.begin(115200);
  //ANYCUBIC_SERIAL_START();
  ANYCUBIC_SERIAL_PROTOCOLPGM("J17"); // J17 Main board reset
  ANYCUBIC_SERIAL_ENTER();
  delay(10);
  ANYCUBIC_SERIAL_PROTOCOLPGM("J12"); // J12 Ready
  ANYCUBIC_SERIAL_ENTER();
  
#if ENABLED(ANYCUBIC_FILAMENT_RUNOUT_SENSOR)
  pinMode(FIL_RUNOUT_PIN,INPUT);
  WRITE(FIL_RUNOUT_PIN,HIGH);
  if(READ(FIL_RUNOUT_PIN)==true)
  {
    ANYCUBIC_SERIAL_PROTOCOLPGM("J15"); //J15 FILAMENT LACK
    ANYCUBIC_SERIAL_ENTER();
#ifdef ANYCUBIC_TFT_DEBUG
    SERIAL_ECHOLNPGM("TFT Serial Debug: Filament runout... J15");
#endif
  }
#endif
}

void AnycubicTFTClass::WriteOutageEEPromData() {
  int pos=E2END-256;
  
}

void AnycubicTFTClass::ReadOutageEEPromData() {
  int pos=E2END-256;

}


void AnycubicTFTClass::StartPrint(){
  if (TFTstate==ANYCUBIC_TFT_STATE_SDPAUSE) {
#ifndef ADVANCED_PAUSE_FEATURE
    enqueue_and_echo_commands_P(PSTR("G91\nG1 Z-10 F240\nG90"));
#endif
  }
  starttime=millis();
  card.startFileprint();
  TFTstate=ANYCUBIC_TFT_STATE_SDPRINT;
}

void AnycubicTFTClass::PausePrint(){
  card.pauseSDPrint();
  TFTstate=ANYCUBIC_TFT_STATE_SDPAUSE_REQ;
  
  if(FilamentTestStatus) {
    ANYCUBIC_SERIAL_PROTOCOLPGM("J05");// J05 pausing
    ANYCUBIC_SERIAL_ENTER();
#ifdef ANYCUBIC_TFT_DEBUG
    SERIAL_ECHOLNPGM("TFT Serial Debug: SD print paused... J05");
#endif
  } else {
    // Pause because of "out of filament"
    ANYCUBIC_SERIAL_PROTOCOLPGM("J23"); //J23 FILAMENT LACK with the prompt box don't disappear
    ANYCUBIC_SERIAL_ENTER();
#ifdef ANYCUBIC_TFT_DEBUG
    SERIAL_ECHOLNPGM("TFT Serial Debug: Filament runout while printing... J23");
#endif
  }
}

void AnycubicTFTClass::StopPrint(){
  card.stopSDPrint();
  TFTstate=ANYCUBIC_TFT_STATE_SDSTOP_REQ;
}


float AnycubicTFTClass::CodeValue()
{
  return (strtod(&TFTcmdbuffer[TFTbufindr][TFTstrchr_pointer - TFTcmdbuffer[TFTbufindr] + 1], NULL));
}

bool AnycubicTFTClass::CodeSeen(char code)
{
  TFTstrchr_pointer = strchr(TFTcmdbuffer[TFTbufindr], code);
  return (TFTstrchr_pointer != NULL);  //Return True if a character was found
}

uint16_t AnycubicTFTClass::GetFileNr()
{
  
  if(card.cardOK)
  {
    return card.getnrfilenames();
  }
  return 0;
}

void AnycubicTFTClass::Ls()
{
  if(card.cardOK)
  {
    uint8_t cnt=0;
    for(cnt=0; cnt<card.getnrfilenames(); cnt++){
      card.getfilename(cnt);
      
      if((MyFileNrCnt-filenumber)<4)
      {
        if(fileoutputcnt<MyFileNrCnt-filenumber)
        {
          ANYCUBIC_SERIAL_PROTOCOLLN(card.filename);
          ANYCUBIC_SERIAL_PROTOCOLLN(card.longFilename);
        }
      }
      else if((fileoutputcnt>=((MyFileNrCnt-4)-filenumber))&&(fileoutputcnt<MyFileNrCnt-filenumber))
      {
        ANYCUBIC_SERIAL_PROTOCOLLN(card.filename);
        ANYCUBIC_SERIAL_PROTOCOLLN(card.longFilename);
      }
      fileoutputcnt++;
    }
    if(fileoutputcnt>=MyFileNrCnt)
      fileoutputcnt=0;
    
  }
}

void AnycubicTFTClass::CheckSDCardChange()
{
  if (LastSDstatus != IS_SD_INSERTED)
  {
    LastSDstatus = IS_SD_INSERTED;
    
    if (LastSDstatus)
    {
      MyFileNrCnt=GetFileNr();
      ANYCUBIC_SERIAL_PROTOCOLPGM("J00"); // J01 SD Card inserted
      ANYCUBIC_SERIAL_ENTER();
#ifdef ANYCUBIC_TFT_DEBUG
      SERIAL_ECHOLNPGM("TFT Serial Debug: SD card inserted... J00");
#endif
    }
    else
    {
      ANYCUBIC_SERIAL_PROTOCOLPGM("J01"); // J01 SD Card removed
      ANYCUBIC_SERIAL_ENTER();
#ifdef ANYCUBIC_TFT_DEBUG
      SERIAL_ECHOLNPGM("TFT Serial Debug: SD card removed... J01");
#endif

    }
  }
}

void AnycubicTFTClass::CheckHeaterError()
{
  if ((thermalManager.degHotend(0) < 5) || (thermalManager.degHotend(0) > 290))
  {
    if (HeaterCheckCount > 60000)
    {
      HeaterCheckCount = 0;
      ANYCUBIC_SERIAL_PROTOCOLPGM("J10"); // J10 Hotend temperature abnormal
      ANYCUBIC_SERIAL_ENTER();
#ifdef ANYCUBIC_TFT_DEBUG
      SERIAL_ECHOLNPGM("TFT Serial Debug: Hotend temperature abnormal... J20");
#endif

    }
    else
    {
      HeaterCheckCount++;
    }
  }
  else
  {
    HeaterCheckCount = 0;
  }
}

void AnycubicTFTClass::StateHandler()
{
  switch (TFTstate) {
  case ANYCUBIC_TFT_STATE_IDLE:
    if(card.sdprinting){
      TFTstate=ANYCUBIC_TFT_STATE_SDPRINT;
      starttime=millis();

      // --> Send print info to display... most probably print started via gcode
    }
    break;
  case ANYCUBIC_TFT_STATE_SDPRINT:
      if(!card.sdprinting){
        // It seems that we are to printing anymore... pause or stopped?
        if (card.isFileOpen()){
          // File is still open --> paused
          TFTstate=ANYCUBIC_TFT_STATE_SDPAUSE;          
        } else {
          // File is closed --> stopped
          TFTstate=ANYCUBIC_TFT_STATE_IDLE;
          ANYCUBIC_SERIAL_PROTOCOLPGM("J14");// J14 print done
          ANYCUBIC_SERIAL_ENTER();
#ifdef ANYCUBIC_TFT_DEBUG
          SERIAL_ECHOLNPGM("TFT Serial Debug: SD print done... J14");
#endif
        }
      }
      break;
  case ANYCUBIC_TFT_STATE_SDPAUSE:
    break;
  case ANYCUBIC_TFT_STATE_SDPAUSE_OOF:
    if(!FilamentTestStatus) {
      // We got filament again
      TFTstate=ANYCUBIC_TFT_STATE_SDPAUSE;
    }
    break;
  case ANYCUBIC_TFT_STATE_SDPAUSE_REQ:
    if((!card.sdprinting) && (!planner.movesplanned())){
      // We have to wait until the sd card printing has been settled
#ifndef ADVANCED_PAUSE_FEATURE
      enqueue_and_echo_commands_P(PSTR("G91\nG1 Z10 F240\nG90"));
#endif
      if(FilamentTestStatus) {
        TFTstate=ANYCUBIC_TFT_STATE_SDPAUSE;
      } else {
        // Pause because of "out of filament"
        TFTstate=ANYCUBIC_TFT_STATE_SDPAUSE_OOF;
      }
      
      ANYCUBIC_SERIAL_PROTOCOLPGM("J18");// J18 pausing print done
      ANYCUBIC_SERIAL_ENTER();
#ifdef ANYCUBIC_TFT_DEBUG
      SERIAL_ECHOLNPGM("TFT Serial Debug: SD print paused done... J18");
#endif
      
    }
    break;
  case ANYCUBIC_TFT_STATE_SDSTOP_REQ:
    if((!card.sdprinting) && (!planner.movesplanned())){
      ANYCUBIC_SERIAL_PROTOCOLPGM("J16");// J16 stop print
      ANYCUBIC_SERIAL_ENTER();
      TFTstate=ANYCUBIC_TFT_STATE_IDLE;
#ifdef ANYCUBIC_TFT_DEBUG
      SERIAL_ECHOLNPGM("TFT Serial Debug: SD print stopped... J16");
#endif
      enqueue_and_echo_commands_P(PSTR("M84"));
    }
    break;
  default:
    break;
  }
}

void AnycubicTFTClass::FilamentRunout()
{
#if ENABLED(ANYCUBIC_FILAMENT_RUNOUT_SENSOR)
  FilamentTestStatus=READ(FIL_RUNOUT_PIN)&0xff;
  
  if(FilamentTestStatus>FilamentTestLastStatus)
  {
    FilamentRunoutCounter++;
    if(FilamentRunoutCounter>=15800)
    {
      FilamentRunoutCounter=0;
      if((card.sdprinting==true))
      {
        PausePrint();
      }
      else if((card.sdprinting==false))
      {
        ANYCUBIC_SERIAL_PROTOCOLPGM("J15"); //J15 FILAMENT LACK
        ANYCUBIC_SERIAL_ENTER();
#ifdef ANYCUBIC_TFT_DEBUG
        SERIAL_ECHOLNPGM("TFT Serial Debug: Filament runout... J15");
#endif
      }
      FilamentTestLastStatus=FilamentTestStatus;
    }
  }
  else if(FilamentTestStatus!=FilamentTestLastStatus)
  {
    FilamentRunoutCounter=0;
    FilamentTestLastStatus=FilamentTestStatus;
#ifdef ANYCUBIC_TFT_DEBUG
    SERIAL_ECHOLNPGM("TFT Serial Debug: Filament runout recovered");
#endif
  }
#endif
}

void AnycubicTFTClass::GetCommandFromTFT()
{
  char *starpos = NULL;
  while( AnycubicSerial.available() > 0  && TFTbuflen < TFTBUFSIZE)
  {
    serial3_char = AnycubicSerial.read();
    if(serial3_char == '\n' ||
       serial3_char == '\r' ||
       serial3_char == ':'  ||
       serial3_count >= (TFT_MAX_CMD_SIZE - 1) )
    {
      if(!serial3_count) { //if empty line
        return;
      }
      
      TFTcmdbuffer[TFTbufindw][serial3_count] = 0; //terminate string
      
      if((strchr(TFTcmdbuffer[TFTbufindw], 'A') != NULL)){
        int16_t a_command;
        TFTstrchr_pointer = strchr(TFTcmdbuffer[TFTbufindw], 'A');
        a_command=((int)((strtod(&TFTcmdbuffer[TFTbufindw][TFTstrchr_pointer - TFTcmdbuffer[TFTbufindw] + 1], NULL))));
#ifdef ANYCUBIC_TFT_DEBUG
        if ((a_command>7) && (a_command != 20)) // No debugging of status polls, please!
          SERIAL_ECHOLNPAIR("TFT Serial Command: ", TFTcmdbuffer[TFTbufindw]);
#endif
        
        switch(a_command){
            
          case 0: //A0 GET HOTEND TEMP
            ANYCUBIC_SERIAL_PROTOCOLPGM("A0V ");
            ANYCUBIC_SERIAL_PROTOCOL(itostr3(int(thermalManager.degHotend(0) + 0.5)));
            ANYCUBIC_SERIAL_ENTER();
            break;
          case 1: //A1  GET HOTEND TARGET TEMP
            ANYCUBIC_SERIAL_PROTOCOLPGM("A1V ");
            ANYCUBIC_SERIAL_PROTOCOL(itostr3(int(thermalManager.degTargetHotend(0) + 0.5)));
            ANYCUBIC_SERIAL_ENTER();
            break;
          case 2: //A2 GET HOTBED TEMP
            ANYCUBIC_SERIAL_PROTOCOLPGM("A2V ");
            ANYCUBIC_SERIAL_PROTOCOL(itostr3(int(thermalManager.degBed() + 0.5)));
            ANYCUBIC_SERIAL_ENTER();
            break;
          case 3: //A3 GET HOTBED TARGET TEMP
            ANYCUBIC_SERIAL_PROTOCOLPGM("A3V ");
            ANYCUBIC_SERIAL_PROTOCOL(itostr3(int(thermalManager.degTargetBed() + 0.5)));
            ANYCUBIC_SERIAL_ENTER();
            break;
            
          case 4://A4 GET FAN SPEED
          {
            unsigned int temp;
            //   temp=((fanSpeed*100)/256+1);
            temp=((fanSpeeds[0]*100)/179+1);//MAX 70%
            temp=constrain(temp,0,100);
            ANYCUBIC_SERIAL_PROTOCOLPGM("A4V ");
            ANYCUBIC_SERIAL_PROTOCOL(temp);
            ANYCUBIC_SERIAL_ENTER();
          }
            break;
          case 5:// A5 GET CURRENT COORDINATE
            ANYCUBIC_SERIAL_PROTOCOLPGM("A5V");
            ANYCUBIC_SERIAL_SPACE();
            ANYCUBIC_SERIAL_PROTOCOLPGM("X: ");
            ANYCUBIC_SERIAL_PROTOCOL(current_position[X_AXIS]);
            ANYCUBIC_SERIAL_SPACE();
            ANYCUBIC_SERIAL_PROTOCOLPGM("Y: ");
            ANYCUBIC_SERIAL_PROTOCOL(current_position[Y_AXIS]);
            ANYCUBIC_SERIAL_SPACE();
            ANYCUBIC_SERIAL_PROTOCOLPGM("Z: ");
            ANYCUBIC_SERIAL_PROTOCOL(current_position[Z_AXIS]);
            ANYCUBIC_SERIAL_SPACE();
            ANYCUBIC_SERIAL_ENTER();
            break;
          case 6: //A6 GET SD CARD PRINTING STATUS
            if(card.sdprinting){
              ANYCUBIC_SERIAL_PROTOCOLPGM("A6V ");
              if(card.cardOK)
              {
                ANYCUBIC_SERIAL_PROTOCOL(itostr3(card.percentDone()));
              }
              else
              {
                ANYCUBIC_SERIAL_PROTOCOLPGM("J02");
              }
            }
            else
              ANYCUBIC_SERIAL_PROTOCOLPGM("A6V ---");
            ANYCUBIC_SERIAL_ENTER();
            break;
          case 7://A7 GET PRINTING TIME
          {
            ANYCUBIC_SERIAL_PROTOCOLPGM("A7V ");
            if(starttime != 0) // print time
            {
              uint16_t time = millis()/60000 - starttime/60000;
              ANYCUBIC_SERIAL_PROTOCOL(itostr2(time/60));
              ANYCUBIC_SERIAL_SPACE();
              ANYCUBIC_SERIAL_PROTOCOLPGM("H");
              ANYCUBIC_SERIAL_SPACE();
              ANYCUBIC_SERIAL_PROTOCOL(itostr2(time%60));
              ANYCUBIC_SERIAL_SPACE();
              ANYCUBIC_SERIAL_PROTOCOLPGM("M");
            }else{
              ANYCUBIC_SERIAL_SPACE();
              ANYCUBIC_SERIAL_PROTOCOLPGM("999:999");
            }
            ANYCUBIC_SERIAL_ENTER();
            
            break;
          }
          case 8: // A8 GET  SD LIST
            MyFileNrCnt=0;
            if(!IS_SD_INSERTED)
            {
              ANYCUBIC_SERIAL_PROTOCOLPGM("J02");
              ANYCUBIC_SERIAL_ENTER();
            }
            else
            {
              MyFileNrCnt=GetFileNr();
              
              if(CodeSeen('S'))
                filenumber=CodeValue();
              
              ANYCUBIC_SERIAL_PROTOCOLPGM("FN "); // Filelist start
              ANYCUBIC_SERIAL_ENTER();
              Ls();
              ANYCUBIC_SERIAL_PROTOCOLPGM("END"); // Filelist stop
              ANYCUBIC_SERIAL_ENTER();
            }
            break;
          case 9: // A9 pause sd print
            if(card.sdprinting)
            {
              PausePrint();
            }
            else
            {
              StopPrint();
            }
            break;
          case 10: // A10 resume sd print
            if((TFTstate==ANYCUBIC_TFT_STATE_SDPAUSE) || (TFTstate==ANYCUBIC_TFT_STATE_SDOUTAGE))
            {
              StartPrint();
              ANYCUBIC_SERIAL_PROTOCOLPGM("J04");// J04 printing form sd card now
              ANYCUBIC_SERIAL_ENTER();
#ifdef ANYCUBIC_TFT_DEBUG
              SERIAL_ECHOLNPGM("TFT Serial Debug: SD print started... J04");
#endif
            }
            break;
          case 11: // A11 STOP SD PRINT
            if((card.sdprinting) || (TFTstate==ANYCUBIC_TFT_STATE_SDOUTAGE))
            {
              StopPrint();
            }
            break;
          case 12: // A12 kill
            ANYCUBIC_SERIAL_PROTOCOLPGM("J11"); // J11 Kill
            ANYCUBIC_SERIAL_ENTER();
#ifdef ANYCUBIC_TFT_DEBUG
            SERIAL_ECHOLNPGM("TFT Serial Debug: Kill command... J11");
#endif
            kill(PSTR(MSG_KILLED));
            break;
          case 13: // A13 SELECTION FILE
            if((!planner.movesplanned()) && (TFTstate!=ANYCUBIC_TFT_STATE_SDPAUSE) && (TFTstate!=ANYCUBIC_TFT_STATE_SDOUTAGE))
            {
              starpos = (strchr(TFTstrchr_pointer + 4,'*'));
              if(starpos!=NULL)
                *(starpos-1)='\0';
              card.openFile(TFTstrchr_pointer + 4,true);
              if (card.isFileOpen()) {
                ANYCUBIC_SERIAL_PROTOCOLPGM("J20"); // J20 Open successful
                ANYCUBIC_SERIAL_ENTER();
#ifdef ANYCUBIC_TFT_DEBUG
                SERIAL_ECHOLNPGM("TFT Serial Debug: File open successful... J20");
#endif
              } else {
                ANYCUBIC_SERIAL_PROTOCOLPGM("J21"); // J21 Open failed
                ANYCUBIC_SERIAL_ENTER();
#ifdef ANYCUBIC_TFT_DEBUG
                SERIAL_ECHOLNPGM("TFT Serial Debug: File open failed... J21");
#endif
              }
              ANYCUBIC_SERIAL_ENTER();
            }
            break;
          case 14: // A14 START PRINTING
            if((!planner.movesplanned()) && (TFTstate!=ANYCUBIC_TFT_STATE_SDPAUSE) && (TFTstate!=ANYCUBIC_TFT_STATE_SDOUTAGE) && (card.isFileOpen()))
            {
              StartPrint();
              ANYCUBIC_SERIAL_PROTOCOLPGM("J06"); // J06 hotend heating
              ANYCUBIC_SERIAL_ENTER();
#ifdef ANYCUBIC_TFT_DEBUG
              SERIAL_ECHOLNPGM("TFT Serial Debug: Hotend heating... J06");
#endif
            }
            break;
          case 15: // A15 RESUMING FROM OUTAGE
            //                    			if((!planner.movesplanned())&&(!TFTresumingflag))
            //                          {
            //                                if(card.cardOK)
            //                                FlagResumFromOutage=true;
            //                                ResumingFlag=1;
            //                                card.startFileprint();
            //                                starttime=millis();
            //                                ANYCUBIC_SERIAL_SUCC_START;
            //                          }
            //                          ANYCUBIC_SERIAL_ENTER();
            break;
          case 16: // A16 set hotend temp
          {
            unsigned int tempvalue;
            if(CodeSeen('S'))
            {
              tempvalue=constrain(CodeValue(),0,275);
              thermalManager.setTargetHotend(tempvalue,0);
            }
            else if((CodeSeen('C'))&&(!planner.movesplanned()))
            {
              if((READ(Z_TEST)==0)) enqueue_and_echo_commands_P(PSTR("G1 Z10")); //RASE Z AXIS
              tempvalue=constrain(CodeValue(),0,275);
              thermalManager.setTargetHotend(tempvalue,0);
            }
          }
            //  ANYCUBIC_SERIAL_ENTER();
            break;
          case 17:// A17 set heated bed temp
          {
            unsigned int tempbed;
            if(CodeSeen('S')){tempbed=constrain(CodeValue(),0,150);
              thermalManager.setTargetBed(tempbed);
            }
          }
            //  ANYCUBIC_SERIAL_ENTER();
            break;
          case 18:// A18 set fan speed
            unsigned int temp;
            if (CodeSeen('S'))
            {
              temp=(CodeValue()*255/100);
              temp=constrain(temp,0,255);
              fanSpeeds[0]=temp;
            }
            else fanSpeeds[0]=255;
            ANYCUBIC_SERIAL_ENTER();
            break;
          case 19: // A19 stop stepper drivers
            if((!planner.movesplanned())&&(!card.sdprinting))
            {
              //                            quickStop();
              disable_X();
              disable_Y();
              disable_Z();
              disable_E0();
            }
            ANYCUBIC_SERIAL_ENTER();
            break;
          case 20:// A20 read printing speed
          {
            
            //                              if(CodeSeen('S')){
            //                              feedmultiply=constrain(CodeValue(),40,999);}
            //                              else{
            //                                  ANYCUBIC_SERIAL_PROTOCOLPGM("A20V ");
            //                                  ANYCUBIC_SERIAL_PROTOCOL(feedmultiply);
            //                                  ANYCUBIC_SERIAL_ENTER();
            //                              }
          }
            break;
          case 21: // A21 all home
            if((!planner.movesplanned()) && (TFTstate!=ANYCUBIC_TFT_STATE_SDPAUSE) && (TFTstate!=ANYCUBIC_TFT_STATE_SDOUTAGE))
            {
              if(CodeSeen('X')||CodeSeen('Y')||CodeSeen('Z'))
              {
                if(CodeSeen('X'))enqueue_and_echo_commands_P(PSTR("G28 X"));
                if(CodeSeen('Y')) enqueue_and_echo_commands_P(PSTR("G28 Y"));
                if(CodeSeen('Z')) enqueue_and_echo_commands_P(PSTR("G28 Z"));
              }
              else if(CodeSeen('C'))enqueue_and_echo_commands_P(PSTR("G28"));
            }
            break;
          case 22: // A22 move X/Y/Z or extrude
            if((!planner.movesplanned()) && (TFTstate!=ANYCUBIC_TFT_STATE_SDPAUSE) && (TFTstate!=ANYCUBIC_TFT_STATE_SDOUTAGE))
            {
              float coorvalue;
              unsigned int movespeed=0;
              char value[30];
              if(CodeSeen('F')) // Set feedrate
                movespeed = CodeValue();
              
              enqueue_and_echo_commands_P(PSTR("G91"));  // relative coordinates
              
              if(CodeSeen('X')) // Move in X direction
              {
                coorvalue=CodeValue();
                if((coorvalue<=0.2)&&coorvalue>0){sprintf_P(value,PSTR("G1 X0.1F%i"),movespeed);}
                else if((coorvalue<=-0.1)&&coorvalue>-1){sprintf_P(value,PSTR("G1 X-0.1F%i"),movespeed);}
                else {sprintf_P(value,PSTR("G1 X%iF%i"),int(coorvalue),movespeed);}
                enqueue_and_echo_command(value);
              }
              else if(CodeSeen('Y')) // Move in Y direction
              {
                coorvalue=CodeValue();
                if((coorvalue<=0.2)&&coorvalue>0){sprintf_P(value,PSTR("G1 Y0.1F%i"),movespeed);}
                else if((coorvalue<=-0.1)&&coorvalue>-1){sprintf_P(value,PSTR("G1 Y-0.1F%i"),movespeed);}
                else {sprintf_P(value,PSTR("G1 Y%iF%i"),int(coorvalue),movespeed);}
                enqueue_and_echo_command(value);
              }
              else if(CodeSeen('Z')) // Move in Z direction
              {
                coorvalue=CodeValue();
                if((coorvalue<=0.2)&&coorvalue>0){sprintf_P(value,PSTR("G1 Z0.1F%i"),movespeed);}
                else if((coorvalue<=-0.1)&&coorvalue>-1){sprintf_P(value,PSTR("G1 Z-0.1F%i"),movespeed);}
                else {sprintf_P(value,PSTR("G1 Z%iF%i"),int(coorvalue),movespeed);}
                enqueue_and_echo_command(value);
              }
              else if(CodeSeen('E')) // Extrude
              {
                coorvalue=CodeValue();
                if((coorvalue<=0.2)&&coorvalue>0){sprintf_P(value,PSTR("G1 E0.1F%i"),movespeed);}
                else if((coorvalue<=-0.1)&&coorvalue>-1){sprintf_P(value,PSTR("G1 E-0.1F%i"),movespeed);}
                else {sprintf_P(value,PSTR("G1 E%iF500"),int(coorvalue)); }
                enqueue_and_echo_command(value);
              }
              enqueue_and_echo_commands_P(PSTR("G90"));  // absolute coordinates
            }
            ANYCUBIC_SERIAL_ENTER();
            break;
          case 23: // A23 preheat pla
            if((!planner.movesplanned())&& (TFTstate!=ANYCUBIC_TFT_STATE_SDPAUSE) && (TFTstate!=ANYCUBIC_TFT_STATE_SDOUTAGE))
            {
              if((READ(Z_TEST)==0)) enqueue_and_echo_commands_P(PSTR("G1 Z10")); // RAISE Z AXIS
              thermalManager.setTargetBed(50);
              thermalManager.setTargetHotend(200, 0);
              ANYCUBIC_SERIAL_SUCC_START;
              ANYCUBIC_SERIAL_ENTER();
            }
            break;
          case 24:// A24 preheat abs
            if((!planner.movesplanned()) && (TFTstate!=ANYCUBIC_TFT_STATE_SDPAUSE) && (TFTstate!=ANYCUBIC_TFT_STATE_SDOUTAGE))
            {
              if((READ(Z_TEST)==0)) enqueue_and_echo_commands_P(PSTR("G1 Z10")); //RAISE Z AXIS
              thermalManager.setTargetBed(80);
              thermalManager.setTargetHotend(240, 0);
              
              ANYCUBIC_SERIAL_SUCC_START;
              ANYCUBIC_SERIAL_ENTER();
            }
            break;
          case 25: // A25 cool down
            if((!planner.movesplanned())&& (TFTstate!=ANYCUBIC_TFT_STATE_SDPAUSE) && (TFTstate!=ANYCUBIC_TFT_STATE_SDOUTAGE))
            {
              thermalManager.setTargetHotend(0,0);
              thermalManager.setTargetBed(0);
              ANYCUBIC_SERIAL_PROTOCOLPGM("J12"); // J12 cool down
              ANYCUBIC_SERIAL_ENTER();
#ifdef ANYCUBIC_TFT_DEBUG
              SERIAL_ECHOLNPGM("TFT Serial Debug: Cooling down... J12");
#endif
            }
            break;
          case 26: // A26 refresh SD
            card.initsd();
            if(!IS_SD_INSERTED)
            {
              ANYCUBIC_SERIAL_PROTOCOLPGM("J02"); // J02 SD Card initilized
              ANYCUBIC_SERIAL_ENTER();
#ifdef ANYCUBIC_TFT_DEBUG
              SERIAL_ECHOLNPGM("TFT Serial Debug: SD card initialized... J02");
#endif
            }
            break;
#ifdef SERVO_ENDSTOPS
          case 27: // A27 servos angles  adjust
            break;
#endif
          case 28: // A28 filament test
          {
            if(CodeSeen('O'));
            else if(CodeSeen('C'));
          }
            ANYCUBIC_SERIAL_ENTER();
            break;
          case 29: // A29 Z PROBE OFFESET SET
            break;
            
          case 30: // A30 assist leveling, the original function was canceled
            if(CodeSeen('S')) {
#ifdef ANYCUBIC_TFT_DEBUG
              SERIAL_ECHOLNPGM("TFT Entering level menue...");
#endif
            } else if(CodeSeen('O')) {
#ifdef ANYCUBIC_TFT_DEBUG
              SERIAL_ECHOLNPGM("TFT Leveling started and movint to front left...");
#endif
              enqueue_and_echo_commands_P(PSTR("G91\nG1 Z10 F240\nG90\nG28\nG29\nG1 X20 Y20 F6000\nG1 Z0 F240"));
            } else if(CodeSeen('T')) {
#ifdef ANYCUBIC_TFT_DEBUG
              SERIAL_ECHOLNPGM("TFT Level checkpoint front right...");
#endif
              enqueue_and_echo_commands_P(PSTR("G1 Z5 F240\nG1 X190 Y20 F6000\nG1 Z0 F240"));
            } else if(CodeSeen('C')) {
#ifdef ANYCUBIC_TFT_DEBUG
              SERIAL_ECHOLNPGM("TFT Level checkpoint back right...");
#endif
              enqueue_and_echo_commands_P(PSTR("G1 Z5 F240\nG1 X190 Y190 F6000\nG1 Z0 F240"));
            } else if(CodeSeen('Q')) {
#ifdef ANYCUBIC_TFT_DEBUG
              SERIAL_ECHOLNPGM("TFT Level checkpoint back right...");
#endif
              enqueue_and_echo_commands_P(PSTR("G1 Z5 F240\nG1 X190 Y20 F6000\nG1 Z0 F240"));
            } else if(CodeSeen('H')) {
#ifdef ANYCUBIC_TFT_DEBUG
              SERIAL_ECHOLNPGM("TFT Level check no heating...");
#endif
              //                            enqueue_and_echo_commands_P(PSTR("... TBD ..."));
              ANYCUBIC_SERIAL_PROTOCOLPGM("J22"); // J22 Test print done
              ANYCUBIC_SERIAL_ENTER();
#ifdef ANYCUBIC_TFT_DEBUG
              SERIAL_ECHOLNPGM("TFT Serial Debug: Leveling print test done... J22");
#endif
            } else if(CodeSeen('L')) {
#ifdef ANYCUBIC_TFT_DEBUG
              SERIAL_ECHOLNPGM("TFT Level check heating...");
#endif
              //                            enqueue_and_echo_commands_P(PSTR("... TBD ..."));
              ANYCUBIC_SERIAL_PROTOCOLPGM("J22"); // J22 Test print done
              ANYCUBIC_SERIAL_ENTER();
#ifdef ANYCUBIC_TFT_DEBUG
              SERIAL_ECHOLNPGM("TFT Serial Debug: Leveling print test with heating done... J22");
#endif
            }
            ANYCUBIC_SERIAL_SUCC_START;
            ANYCUBIC_SERIAL_ENTER();
            
            break;
          case 31: // A31 zoffset
            /*
            if((!planner.movesplanned())&&(TFTstate!=ANYCUBIC_TFT_STATE_SDPAUSE) && (TFTstate!=ANYCUBIC_TFT_STATE_SDOUTAGE))
            {
              
              if((READ(Z_TEST)==0))
                z_offset_auto_test();

              if(TFTcode_seen('S')){
                ANYCUBIC_SERIAL_PROTOCOLPGM("A9V ");
                ANYCUBIC_SERIAL_PROTOCOL(int(Current_z_offset*100));
                ANYCUBIC_SERIAL_ENTER();
              }
              if(TFTcode_seen('D'))
              {
                Current_z_offset=(TFTcode_value()/100);
                SaveMyZoffset();
              }
            }
            TFT_SERIAL_ENTER();
            break;
            */
          case 32: // A32 clean leveling beep flag
            if(CodeSeen('S')) {
#ifdef ANYCUBIC_TFT_DEBUG
              SERIAL_ECHOLNPGM("TFT Level saving data...");
#endif
              enqueue_and_echo_commands_P(PSTR("M500\nM420 S1\nG1 Z10 F240\nG1 X0 Y0 F6000"));
              ANYCUBIC_SERIAL_SUCC_START;
              ANYCUBIC_SERIAL_ENTER();
            }
            break;
          case 33: // A33 get version info
          {
            ANYCUBIC_SERIAL_PROTOCOLPGM("J33 ");
            ANYCUBIC_SERIAL_PROTOCOLPGM(MSG_MY_VERSION);
            ANYCUBIC_SERIAL_ENTER();
          }
            break;
          default: break;
        }
      }
      TFTbufindw = (TFTbufindw + 1)%TFTBUFSIZE;
      TFTbuflen += 1;
      serial3_count = 0; //clear buffer
    }
    else
    {
      TFTcmdbuffer[TFTbufindw][serial3_count++] = serial3_char;
    }
  }
}

void AnycubicTFTClass::CommandScan()
{
  CheckHeaterError();
  CheckSDCardChange();
  StateHandler();
  
  if(TFTbuflen<(TFTBUFSIZE-1))
    GetCommandFromTFT();
  if(TFTbuflen)
  {
    TFTbuflen = (TFTbuflen-1);
    TFTbufindr = (TFTbufindr + 1)%TFTBUFSIZE;
  }
}

void AnycubicTFTClass::HeatingDone()
{
  ANYCUBIC_SERIAL_PROTOCOLPGM("J07"); // J07 hotend heating done
  ANYCUBIC_SERIAL_ENTER();
  if(card.sdprinting)
  {
    ANYCUBIC_SERIAL_PROTOCOLPGM("J04"); // J04 printing from sd card
    ANYCUBIC_SERIAL_ENTER();
    TFTstate=ANYCUBIC_TFT_STATE_SDPRINT;
  }
}

void AnycubicTFTClass::HotbedHeatingDone()
{
  ANYCUBIC_SERIAL_PROTOCOLPGM("J09"); // J09 hotbed heating done
  ANYCUBIC_SERIAL_ENTER();
}


AnycubicTFTClass AnycubicTFT;
#endif
