#include "SpaNetInterface.h"

#define BAUD_RATE 38400

#if defined(ESP8266)
    #define RX_PIN 13 //goes to rx on spanet pin5
    #define TX_PIN 15 //goes to tx on spanet pin6
    HardwareSerial spaSerial = Serial;
#elif defined(ESP32)
    #define RX_PIN 16 //goes to rx on spanet pin5
    #define TX_PIN 17 //goes to tx on spanet pin6
    HardwareSerial spaSerial = Serial2;
#endif

SpaNetInterface::SpaNetInterface(Stream &p) : port(p) {
}

SpaNetInterface::SpaNetInterface() : port(spaSerial) {
    #if defined(ESP8266)
        spaSerial.setRxBufferSize(1024);  //required for unit testing
        spaSerial.begin(BAUD_RATE);
        spaSerial.pins(TX_PIN, RX_PIN);
    #elif defined(ESP32)
        spaSerial.setRxBufferSize(1024);  //required for unit testing
        spaSerial.setTxBufferSize(1024);  //required for unit testing
        spaSerial.begin(BAUD_RATE, SERIAL_8N1, RX_PIN, TX_PIN);
    #endif
    spaSerial.setTimeout(250);
}

SpaNetInterface::~SpaNetInterface() {}




void SpaNetInterface::flushSerialReadBuffer() {
    int x = 0;

    debugD("Flushing serial stream - %i bytes in the buffer", port.available());
    while (port.available() > 0 && x++<5120) { 
        debugV("%i,",port.read());
    }
    debugD("Flushed serial stream - %i bytes in the buffer", port.available());
}


void SpaNetInterface::sendCommand(String cmd) {

    flushSerialReadBuffer();

    debugD("Sending - %s",cmd.c_str());
    port.print('\n');
    port.flush();
    delay(50); // **TODO** is this needed?
    port.printf("%s\n", cmd);
    port.flush();

    long timeout = millis() + 1000; // wait up to 1 sec for a response

    debugD("Start waiting for a response");
    while (port.available()==0 and millis()<timeout) {}
    debugD("Finish waiting");

    _resultRegistersDirty = true; // we're trying to write to the registers so we can assume that they will now be dirty
}

String SpaNetInterface::sendCommandReturnResult(String cmd) {
    sendCommand(cmd);
    String result = port.readStringUntil('\n');
    port.read(); // get rid of the trailing LF char
    debugV("Read - %s",result.c_str());
    return result;
}

bool SpaNetInterface::setRB_TP_Pump1(int mode){
    debugD("setRB_TP_Pump1 - %i",mode);
    String result = sendCommandReturnResult("S22:"+String(mode));
    if (result == "S22-OK") {
        update_RB_TP_Pump1(String(mode));
        return true;
    }
    return false;
}

bool SpaNetInterface::setRB_TP_Pump2(int mode){
    debugD("setRB_TP_Pump2 - %i",mode);
    String result = sendCommandReturnResult("S23:"+String(mode));
    if (result == "S23-OK") {
        update_RB_TP_Pump2(String(mode));
        return true;
    }
    return false;
}

bool SpaNetInterface::setRB_TP_Pump3(int mode){
    debugD("setRB_TP_Pump3 - %i",mode);
    String result = sendCommandReturnResult("S24:"+String(mode));
    if (result == "S24-OK") {
        update_RB_TP_Pump3(String(mode));
        return true;
    }
    return false;
}

bool SpaNetInterface::setRB_TP_Pump4(int mode){
    debugD("setRB_TP_Pump4 - %i",mode);
    String result = sendCommandReturnResult("S25:"+String(mode));
    if (result == "S25-OK") {
        update_RB_TP_Pump4(String(mode));
        return true;
    }
    return false;
}

bool SpaNetInterface::setRB_TP_Pump5(int mode){
    debugD("setRB_TP_Pump5 - %i",mode);
    String result = sendCommandReturnResult("S26:"+String(mode));
    if (result == "S26-OK") {
        update_RB_TP_Pump5(String(mode));
        return true;
    }
    return false;
}

bool SpaNetInterface::setHELE(int mode){
    debugD("setHELE - %i", mode);
    String result = sendCommandReturnResult("W98:"+String(mode));
    if (result == String(mode)) {
        update_HELE(String(mode));
        return true;
    }
    return false;
}


/// @brief Set the water temperature set point * 10 (380 = 38.0)
/// @param temp 
/// @return 
bool SpaNetInterface::setSTMP(int temp){
    debugD("setSTMP - %s", String(temp));
    String result = sendCommandReturnResult("W40:" + String(temp));
    if (String(temp).compareTo(result)) {
        update_STMP(result);
        return true;
    }
    return false;
}

bool SpaNetInterface::setHPMP(int mode){
    debugD("setHPMP - %i", mode);
    String result = sendCommandReturnResult(String("W99:")+mode);
    if (result.toInt() == mode) {
        update_HPMP(result);
        return true;
    }
    return false;
}

bool SpaNetInterface::setHPMP(String mode){
    debugD("setHPMP - %s", mode.c_str());
    for (int x=0; x<HPMPStrings.size(); x++) {
        if (HPMPStrings[x] == mode) {
            return setHPMP(x);
        }
    }
    return false;
}

bool SpaNetInterface::readStatus() {

    // We could just do a port.readString but this will always impose a
    // 250ms (or whatever the timeout is) delay penality.  This in turn,
    // along with the other unavoidable delays can cause the status of
    // properties to bounce in certain UI's (apple devices, home assistant, etc)

    debugD("Reading registers -");

    int field = 0;
    validStatusResponse = false;
    String statusResponseTmp = "";

    while (field < statusResponseRawMax)
    {
        statusResponseRaw[field] = port.readStringUntil(',');
        debugV("(%i,%s)",field,statusResponseRaw[field]);

        statusResponseTmp = statusResponseTmp + statusResponseRaw[field]+",";

        if (statusResponseRaw[field].isEmpty() && field > statusResponseRawMin) {
            break;
        }
        if (statusResponseRaw[field].isEmpty()) { // If we get a empty field then we've had a bad read.
            debugE("Throwing exception - null string");
            return false;
        }
        if (field == 0 && !statusResponseRaw[field].startsWith("RF:")) { // If the first field is not "RF:" stop we don't have the start of the register
            debugE("Throwing exception - field: %i, value: %s", field, statusResponseRaw[field]);
            return false;
        }

        if (statusResponseRaw[field] == "R2") R2 = field;
        if (statusResponseRaw[field] == "R3") R3 = field;
        if (statusResponseRaw[field] == "R4") R4 = field;
        if (statusResponseRaw[field] == "R5") R5 = field;
        if (statusResponseRaw[field] == "R6") R6 = field;
        if (statusResponseRaw[field] == "R7") R7 = field;
        if (statusResponseRaw[field] == "R9") R9 = field;
        if (statusResponseRaw[field] == "RA") RA = field;
        if (statusResponseRaw[field] == "RB") RB = field;
        if (statusResponseRaw[field] == "RC") RC = field;
        if (statusResponseRaw[field] == "RE") RE = field;
        if (statusResponseRaw[field] == "RG") RG = field;

        field++;
    }

    //Flush the remaining data from the buffer as the last field is meaningless
    flushSerialReadBuffer();

    statusResponse.update_Value(statusResponseTmp);

    if (field < statusResponseRawMin) {
        debugE("Throwing exception - %i fields read expecting at least %i",field, statusResponseRawMin);
        return false;
    }

    updateMeasures();
    _resultRegistersDirty = false;
    validStatusResponse = true;

    debugD("Reading registers - finish");
    return true;
}

bool SpaNetInterface::isInitialised() { 
    return _initialised; 
}


void SpaNetInterface::updateStatus() {

    flushSerialReadBuffer();

    debugD("Update status called");
    sendCommand("RF");

    _nextUpdateDue = millis() + FAILEDREADFREQUENCY;    
    if (readStatus()) {
        debugD("readStatus returned true");
        _nextUpdateDue = millis() + UPDATEFREQUENCY;
        _initialised = true;
        if (updateCallback != nullptr) { updateCallback(); }
    }
}


void SpaNetInterface::loop(){
    if ( _lastWaitMessage + 1000 < millis()) {
        debugD("Waiting...");
        _lastWaitMessage = millis();
    }

    if (_resultRegistersDirty) {
        _nextUpdateDue = millis() + 200;  // if we need to read the registers, pause a bit to see if there are more commands coming.
        _resultRegistersDirty = false;
    }

    if (millis()>_nextUpdateDue) {
        updateStatus();    
    }
}


void SpaNetInterface::setUpdateCallback(void (*f)()) {
    updateCallback = f;
}


void SpaNetInterface::clearUpdateCallback() {
    updateCallback = nullptr;
}


void SpaNetInterface::updateMeasures() {
    #pragma region R2
    update_MainsCurrent(statusResponseRaw[R2+1]);
    update_MainsVoltage(statusResponseRaw[R2+2]);
    update_CaseTemperature(statusResponseRaw[R2+3]);
    update_PortCurrent(statusResponseRaw[R2+4]);
    // Not implemented - update_SpaTime(statusResponseRaw[R2+11]+"-"+statusResponseRaw[R2+10]+"-"+statusResponseRaw[R2+9]+" "+statusResponseRaw[R2+8]+":"+statusResponseRaw[R2+7]+":"+statusResponseRaw[R2+6]);
    update_HeaterTemperature(statusResponseRaw[R2+12]);
    update_PoolTemperature(statusResponseRaw[R2+13]);
    update_WaterPresent(statusResponseRaw[R2+14]);
    update_AwakeMinutesRemaining(statusResponseRaw[R2+16]);
    update_FiltPumpRunTimeTotal(statusResponseRaw[R2+17]);
    update_FiltPumpReqMins(statusResponseRaw[R2+18]);
    update_LoadTimeOut(statusResponseRaw[R2+19]);
    update_HourMeter(statusResponseRaw[R2+20]);
    update_Relay1(statusResponseRaw[R2+21]);
    update_Relay2(statusResponseRaw[R2+22]);
    update_Relay3(statusResponseRaw[R2+23]);
    update_Relay4(statusResponseRaw[R2+24]);
    update_Relay5(statusResponseRaw[R2+25]);
    update_Relay6(statusResponseRaw[R2+26]);
    update_Relay7(statusResponseRaw[R2+27]);
    update_Relay8(statusResponseRaw[R2+28]);
    update_Relay9(statusResponseRaw[R2+29]); 
    #pragma endregion
    #pragma region R3
    update_CLMT(statusResponseRaw[R3+1]);
    update_PHSE(statusResponseRaw[R3+2]);
    update_LLM1(statusResponseRaw[R3+3]);
    update_LLM2(statusResponseRaw[R3+4]);
    update_LLM3(statusResponseRaw[R3+5]);
    update_SVER(statusResponseRaw[R3+6]);
    update_Model(statusResponseRaw[R3+7]); 
    update_SerialNo1(statusResponseRaw[R3+8]);
    update_SerialNo2(statusResponseRaw[R3+9]); 
    update_D1(statusResponseRaw[R3+10]);
    update_D2(statusResponseRaw[R3+11]);
    update_D3(statusResponseRaw[R3+12]);
    update_D4(statusResponseRaw[R3+13]);
    update_D5(statusResponseRaw[R3+14]);
    update_D6(statusResponseRaw[R3+15]);
    update_Pump(statusResponseRaw[R3+16]);
    update_LS(statusResponseRaw[R3+17]);
    update_HV(statusResponseRaw[R3+18]);
    update_SnpMR(statusResponseRaw[R3+19]);
    update_Status(statusResponseRaw[R3+20]);
    update_PrimeCount(statusResponseRaw[R3+21]);
    update_EC(statusResponseRaw[R3+22]);
    update_HAMB(statusResponseRaw[R3+23]);
    update_HCON(statusResponseRaw[R3+24]);
    // update_HV_2(statusResponseRaw[R3+25]);
    #pragma endregion
    #pragma region R4
    update_Mode(statusResponseRaw[67]);
    update_Ser1_Timer(statusResponseRaw[68]);
    update_Ser2_Timer(statusResponseRaw[69]);
    update_Ser3_Timer(statusResponseRaw[70]);
    update_HeatMode(statusResponseRaw[71]);
    update_PumpIdleTimer(statusResponseRaw[72]);
    update_PumpRunTimer(statusResponseRaw[73]);
    update_AdtPoolHys(statusResponseRaw[74]);
    update_AdtHeaterHys(statusResponseRaw[75]);
    update_Power(statusResponseRaw[76]);
    update_Power_kWh(statusResponseRaw[77]);
    update_Power_Today(statusResponseRaw[78]);
    update_Power_Yesterday(statusResponseRaw[79]);
    update_ThermalCutOut(statusResponseRaw[80]);
    update_Test_D1(statusResponseRaw[81]);
    update_Test_D2(statusResponseRaw[82]);
    update_Test_D3(statusResponseRaw[83]);
    update_ElementHeatSourceOffset(statusResponseRaw[84]);
    update_Frequency(statusResponseRaw[85]);
    update_HPHeatSourceOffset_Heat(statusResponseRaw[86]);
    update_HPHeatSourceOffset_Cool(statusResponseRaw[87]);
    update_HeatSourceOffTime(statusResponseRaw[88]);
    update_Vari_Speed(statusResponseRaw[90]);
    update_Vari_Percent(statusResponseRaw[91]);
    update_Vari_Mode(statusResponseRaw[89]);
    #pragma endregion
    #pragma region R5
    //R5
    // Unknown encoding - TouchPad2.updateValue();
    // Unknown encoding - TouchPad1.updateValue();
    //RB_TP_Blower.updateValue(statusResponseRaw[R5 + 5]);
    update_RB_TP_Sleep(statusResponseRaw[R5 + 10]);
    update_RB_TP_Ozone(statusResponseRaw[R5 + 11]);
    update_RB_TP_Heater(statusResponseRaw[R5 + 12]);
    update_RB_TP_Auto(statusResponseRaw[R5 + 13]);
    //RB_TP_Light.updateValue(statusResponseRaw[R5 + 14]);
    update_WTMP(statusResponseRaw[R5 + 15]);
    update_CleanCycle(statusResponseRaw[R5 + 16]);
    update_RB_TP_Pump1(statusResponseRaw[R5 + 18]);
    update_RB_TP_Pump2(statusResponseRaw[R5 + 19]);
    update_RB_TP_Pump3(statusResponseRaw[R5 + 20]);
    update_RB_TP_Pump4(statusResponseRaw[R5 + 21]);
    update_RB_TP_Pump5(statusResponseRaw[R5 + 22]);
    #pragma endregion
    #pragma region R6
    update_VARIValue(statusResponseRaw[R6 + 1]);
    update_LBRTValue(statusResponseRaw[R6 + 2]);
    update_CurrClr(statusResponseRaw[R6 + 3]);
    update_ColorMode(statusResponseRaw[R6 + 4]);
    update_LSPDValue(statusResponseRaw[R6 + 5]);
    update_FiltSetHrs(statusResponseRaw[R6 + 6]);
    update_FiltBlockHrs(statusResponseRaw[R6 + 7]);
    update_STMP(statusResponseRaw[R6 + 8]);
    update_L_24HOURS(statusResponseRaw[R6 + 9]);
    update_PSAV_LVL(statusResponseRaw[R6 + 10]);
    update_PSAV_BGN(statusResponseRaw[R6 + 11]);
    update_PSAV_END(statusResponseRaw[R6 + 12]);
    update_L_1SNZ_DAY(statusResponseRaw[R6 + 13]);
    update_L_2SNZ_DAY(statusResponseRaw[R6 + 14]);
    update_L_1SNZ_BGN(statusResponseRaw[R6 + 15]);
    update_L_2SNZ_BGN(statusResponseRaw[R6 + 16]);
    update_L_1SNZ_END(statusResponseRaw[R6 + 17]);
    update_L_2SNZ_END(statusResponseRaw[R6 + 18]);
    update_DefaultScrn(statusResponseRaw[R6 + 19]);
    update_TOUT(statusResponseRaw[R6 + 20]);
    update_VPMP(statusResponseRaw[R6 + 21]);
    update_HIFI(statusResponseRaw[R6 + 22]);
    update_BRND(statusResponseRaw[R6 + 23]);
    update_PRME(statusResponseRaw[R6 + 24]);
    update_ELMT(statusResponseRaw[R6 + 25]);
    update_TYPE(statusResponseRaw[R6 + 26]);
    update_GAS(statusResponseRaw[R6 + 27]);
    #pragma endregion
    #pragma region R7
    update_WCLNTime(statusResponseRaw[R7 + 1]);
    // The following 2 may be reversed
    update_TemperatureUnits(statusResponseRaw[R7 + 3]);
    update_OzoneOff(statusResponseRaw[R7 + 2]);
    update_Ozone24(statusResponseRaw[R7 + 4]);
    update_Circ24(statusResponseRaw[R7 + 6]);
    update_CJET(statusResponseRaw[R7 + 5]);
    // 0 = off, 1 = step, 2 = variable
    update_VELE(statusResponseRaw[R7 + 7]);
    //update_StartDD(statusResponseRaw[R7 + 8]);
    //update_StartMM(statusResponseRaw[R7 + 9]);
    //update_StartYY(statusResponseRaw[R7 + 10]);
    update_V_Max(statusResponseRaw[R7 + 11]);
    update_V_Min(statusResponseRaw[R7 + 12]);
    update_V_Max_24(statusResponseRaw[R7 + 13]);
    update_V_Min_24(statusResponseRaw[R7 + 14]);
    update_CurrentZero(statusResponseRaw[R7 + 15]);
    update_CurrentAdjust(statusResponseRaw[R7 + 16]);
    update_VoltageAdjust(statusResponseRaw[R7 + 17]);
    // 168 is unknown
    update_Ser1(statusResponseRaw[R7 + 19]);
    update_Ser2(statusResponseRaw[R7 + 20]);
    update_Ser3(statusResponseRaw[R7 + 21]);
    update_VMAX(statusResponseRaw[R7 + 22]);
    update_AHYS(statusResponseRaw[R7 + 23]);
    update_HUSE(statusResponseRaw[R7 + 24]);
    update_HELE(statusResponseRaw[R7 + 25]);
    update_HPMP(statusResponseRaw[R7 + 26]);
    update_PMIN(statusResponseRaw[R7 + 27]);
    update_PFLT(statusResponseRaw[R7 + 28]);
    update_PHTR(statusResponseRaw[R7 + 29]);
    update_PMAX(statusResponseRaw[R7 + 30]);
    #pragma endregion
    #pragma region R9
    update_F1_HR(statusResponseRaw[R9 + 2]);
    update_F1_Time(statusResponseRaw[R9 + 3]);
    update_F1_ER(statusResponseRaw[R9 + 4]);
    update_F1_I(statusResponseRaw[R9 + 5]);
    update_F1_V(statusResponseRaw[R9 + 6]);
    update_F1_PT(statusResponseRaw[R9 + 7]);
    update_F1_HT(statusResponseRaw[R9 + 8]);
    update_F1_CT(statusResponseRaw[R9 + 9]);
    update_F1_PU(statusResponseRaw[R9 + 10]);
    update_F1_VE(statusResponseRaw[R9 + 11]);
    update_F1_ST(statusResponseRaw[R9 + 12]);
    #pragma endregion
    #pragma region RA
    update_F2_HR(statusResponseRaw[RA + 2]);
    update_F2_Time(statusResponseRaw[RA + 3]);
    update_F2_ER(statusResponseRaw[RA + 4]);
    update_F2_I(statusResponseRaw[RA + 5]);
    update_F2_V(statusResponseRaw[RA + 6]);
    update_F2_PT(statusResponseRaw[RA + 7]);
    update_F2_HT(statusResponseRaw[RA + 8]);
    update_F2_CT(statusResponseRaw[RA + 9]);
    update_F2_PU(statusResponseRaw[RA + 10]);
    update_F2_VE(statusResponseRaw[RA + 11]);
    update_F2_ST(statusResponseRaw[RA + 12]);
    #pragma endregion
    #pragma region RB
    update_F3_HR(statusResponseRaw[RB + 2]);
    update_F3_Time(statusResponseRaw[RB + 3]);
    update_F3_ER(statusResponseRaw[RB + 4]);
    update_F3_I(statusResponseRaw[RB + 5]);
    update_F3_V(statusResponseRaw[RB + 6]);
    update_F3_PT(statusResponseRaw[RB + 7]);
    update_F3_HT(statusResponseRaw[RB + 8]);
    update_F3_CT(statusResponseRaw[RB + 9]);
    update_F3_PU(statusResponseRaw[RB + 10]);
    update_F3_VE(statusResponseRaw[RB + 11]);
    update_F3_ST(statusResponseRaw[RB + 12]);
    #pragma endregion
    #pragma region RC
    //Outlet_Heater.updateValue(statusResponseRaw[]);
    //Outlet_Circ.updateValue(statusResponseRaw[]);
    //Outlet_Sanitise.updateValue(statusResponseRaw[]);
    //Outlet_Pump1.updateValue(statusResponseRaw[]);
    //Outlet_Pump2.updateValue(statusResponseRaw[]);
    //Outlet_Pump4.updateValue(statusResponseRaw[]);
    //Outlet_Pump5.updateValue(statusResponseRaw[]);
    update_Outlet_Blower(statusResponseRaw[RC + 10]);
    #pragma endregion
    #pragma region RE
    update_HP_Present(statusResponseRaw[RE + 1]);
    //HP_FlowSwitch.updateValue(statusResponseRaw[]);
    //HP_HighSwitch.updateValue(statusResponseRaw[]);
    //HP_LowSwitch.updateValue(statusResponseRaw[]);
    //HP_CompCutOut.updateValue(statusResponseRaw[]);
    //HP_ExCutOut.updateValue(statusResponseRaw[]);
    //HP_D1.updateValue(statusResponseRaw[]);
    //HP_D2.updateValue(statusResponseRaw[]);
    //HP_D3.updateValue(statusResponseRaw[]);
    update_HP_Ambient(statusResponseRaw[RE + 10]);
    update_HP_Condensor(statusResponseRaw[RE + 11]);
    update_HP_Compressor_State(statusResponseRaw[RE + 12]);
    update_HP_Fan_State(statusResponseRaw[RE + 13]);
    update_HP_4W_Valve(statusResponseRaw[RE + 14]);
    update_HP_Heater_State(statusResponseRaw[RE + 15]);
    update_HP_State(statusResponseRaw[RE + 16]);
    update_HP_Mode(statusResponseRaw[RE + 17]);
    update_HP_Defrost_Timer(statusResponseRaw[RE + 18]);
    update_HP_Comp_Run_Timer(statusResponseRaw[RE + 19]);
    update_HP_Low_Temp_Timer(statusResponseRaw[RE + 20]);
    update_HP_Heat_Accum_Timer(statusResponseRaw[RE + 21]);
    update_HP_Sequence_Timer(statusResponseRaw[RE + 22]);
    update_HP_Warning(statusResponseRaw[RE + 23]);
    update_FrezTmr(statusResponseRaw[RE + 24]);
    update_DBGN(statusResponseRaw[RE + 25]);
    update_DEND(statusResponseRaw[RE + 26]);
    update_DCMP(statusResponseRaw[RE + 27]);
    update_DMAX(statusResponseRaw[RE + 28]);
    update_DELE(statusResponseRaw[RE + 29]);
    update_DPMP(statusResponseRaw[RE + 30]);
    //CMAX.updateValue(statusResponseRaw[]);
    //HP_Compressor.updateValue(statusResponseRaw[]);
    //HP_Pump_State.updateValue(statusResponseRaw[]);
    //HP_Status.updateValue(statusResponseRaw[]);
    #pragma endregion
    #pragma region RG
    update_Pump1InstallState(statusResponseRaw[RG + 7]);
    update_Pump2InstallState(statusResponseRaw[RG + 8]);
    update_Pump3InstallState(statusResponseRaw[RG + 9]);
    update_Pump4InstallState(statusResponseRaw[RG + 10]);
    update_Pump5InstallState(statusResponseRaw[RG + 11]);
    update_Pump1OkToRun(statusResponseRaw[RG + 1]);
    update_Pump2OkToRun(statusResponseRaw[RG + 2]);
    update_Pump3OkToRun(statusResponseRaw[RG + 3]);
    update_Pump4OkToRun(statusResponseRaw[RG + 4]);
    update_Pump5OkToRun(statusResponseRaw[RG + 5]);
    update_LockMode(statusResponseRaw[RG + 12]);
    #pragma endregion

};