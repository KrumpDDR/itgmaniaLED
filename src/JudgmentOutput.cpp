#include "JudgmentOutput.h"
#include "LuaManager.h"

extern "C" {
#include "serialib.h"
}


//"global" variable that is set when the thread is started.
    //Yes I do realize how stupid it is to use a nonatomic variable for something like this.
bool m_JudgmentOutputShutdown = true;

RageThread m_JudgmentOutputThread;
RageEvent* m_JudgmentOutputMutex = nullptr;
std::queue<JudgmentOutputFrame> m_JudgmentOutputQueue;

static int JudgmentOutputThread_Main(void* p);



serialib p1Serial;
serialib p2Serial;



//Start the thread for doing the serial mumbo jumbo.
void JudgmentOutputInit() {
    //Do not create the thread again if it's already created
    if(!m_JudgmentOutputShutdown) return;

    m_JudgmentOutputShutdown = false;

    //LuaHelpers::ReportScriptError("!!MAKING THREAD!!");
    
    //Prepare thread items and make it
    if(m_JudgmentOutputMutex == nullptr) {
        m_JudgmentOutputMutex = new RageEvent("JudgementOutputMutex");
    }
    m_JudgmentOutputThread.SetName("JudgmentOutput thread");
    m_JudgmentOutputThread.~RageThread();
    m_JudgmentOutputThread.Create(JudgmentOutputThread_Main, nullptr);
}

//Stop the thread
void JudgmentOutputShutdown() {
    //Do not shut down the thread again if it's already shut down
    if(m_JudgmentOutputShutdown) return;
    m_JudgmentOutputShutdown = true;

    
    //LuaHelpers::ReportScriptError("!!STOPPING THREAD!!");    

    if (m_JudgmentOutputMutex != nullptr) {
        m_JudgmentOutputMutex->Lock();
        m_JudgmentOutputMutex->Signal();
        m_JudgmentOutputMutex->Unlock();
    }
}

//form the structure and add to the queue, to be processed by the thread.
void JudgmentOutputSend(TapNote tn, int iRow, int iTrack, TapNoteScore tns, float fTapNoteOffset, uint8_t playerNum, uint8_t styleType, uint8_t msgType) {
    //pack that data into a struct
        //The ANSI C struct equals operator shoooouulld do what I want here. I think.
    struct JudgmentOutputFrame frame;
    frame.tn = tn;
    frame.iRow = iRow;
    frame.iTrack = iTrack;
    frame.tns = tns;
    frame.fTapNoteOffset = fTapNoteOffset;
    frame.playerNum = playerNum;
    frame.msgType = msgType;
    frame.styleType = styleType;

    //Put the frame on the queue for the thread to read
        //I don't actually know anything about the C++ std queue, so I'm
            //really hoping here that it will copy the struct and not make a reference.
            //Because this stack is about to get nuked.
    m_JudgmentOutputMutex->Lock();
    m_JudgmentOutputQueue.push(frame);
    m_JudgmentOutputMutex->Signal();
    m_JudgmentOutputMutex->Unlock();
}


//judgment output function main thread
static int JudgmentOutputThread_Main(void* p) {
    char p1Port[20] = "\\\\.\\COM0\0\0\0";
    char p2Port[20] = "\\\\.\\COM0\0\0\0";

    //Setup serial Junk
    //Search for COM ports
    uint8_t p1Discovered = 0, p2Discovered = 0;
    for(uint8_t com = 1; com < 100 && !(p1Discovered&&p2Discovered); com++) {
        serialib tempSerial;
        char comPort[20] = "\\\\.\\COMX\0\0\0";
        if(com < 10)
            comPort[7] = '0' + com;
        else {
            comPort[7]  = '0' + (com/10);
            comPort[8] = '0' + (com%10);
        }
        //printf("%s\r\n", comPort);
        uint8_t connectStatus = tempSerial.openDevice(comPort, 115200);
        if(connectStatus == 1) { //if COM can be opened
            //LuaHelpers::ReportScriptErrorFmt("Managed to open COM%d", com);
            //check if it's an LED device
            for(uint8_t i = 0; i < 5; i++) {
                uint8_t serialMessage[8];
                serialMessage[0] = 'S';
                serialMessage[1] = 2; //request player number
                serialMessage[7] = 'E';
                tempSerial.writeBytes(serialMessage, 8);
                //Response format will is 2 byes. 'G' and player number
                uint8_t rb = 0;
                uint8_t res = tempSerial.readChar((char*)&rb,200);
                /*if(res == 1)
                    LuaHelpers::ReportScriptErrorFmt("Read byte 0 from serial: %d", rb);*/
                if(rb == 'G') {
                    res = tempSerial.readChar((char*)&rb,200);
                    /*if(res == 1)
                        LuaHelpers::ReportScriptErrorFmt("Read byte 1 from serial: %d", rb);*/
                    if(rb == 0) { //reply of player 1
                        memcpy(p1Port, comPort, 20);
                        p1Discovered = 1;
                        //LuaHelpers::ReportScriptError("Player 1 Serial Discovered");
                    }
                    else if(rb == 1) { //reply of player 2
                        memcpy(p2Port, comPort, 20);
                        p2Discovered = 1;
                        //LuaHelpers::ReportScriptError("Player 2 Serial Discovered");
                    }
                    else //something bad happened if flow gets here
                        LuaHelpers::ReportScriptError("Invalid serial search response!");
                
                    break;
                }
            }
            if(tempSerial.isDeviceOpen())
                tempSerial.closeDevice();
        }
    }


    //LuaHelpers::ReportScriptError("Initializing serial ports");
    uint8_t p1Connected = 0;
    if(p1Discovered) {
        p1Connected = p1Serial.openDevice(p1Port, 115200);
        if(p1Connected != 1)
            LuaHelpers::ReportScriptErrorFmt("FAILED TO OPEN SERIAL PORT FOR P1: %d", p1Connected);
    }

    uint8_t p2Connected = 0;
    if(p2Discovered) {
        p2Connected = p2Serial.openDevice(p2Port, 115200);
        if(p2Connected != 1)
            LuaHelpers::ReportScriptErrorFmt("FAILED TO OPEN SERIAL PORT FOR P2: %d", p2Connected);
    }



    while (!m_JudgmentOutputShutdown) {
        m_JudgmentOutputMutex->Lock();

    //spin here until we get a judgment
        while (m_JudgmentOutputQueue.empty() && !m_JudgmentOutputShutdown) {
            m_JudgmentOutputMutex->Wait();
        }

        if (m_JudgmentOutputShutdown) {
            m_JudgmentOutputMutex->Unlock();
            break;
        }

        //pop the queue like a stack
        JudgmentOutputFrame out = m_JudgmentOutputQueue.front();
        m_JudgmentOutputQueue.pop();

        m_JudgmentOutputMutex->Unlock();

        //Do serial packing and output here.
        //Temp debug just printing crap
        /*LuaHelpers::ReportScriptError("!!START OF NEW JUDGMENT!!");
        LuaHelpers::ReportScriptErrorFmt("Tap Note Type: %d", out.tn.type);
        LuaHelpers::ReportScriptErrorFmt("Tap Note Subtype: %d", out.tn.subType);
        LuaHelpers::ReportScriptErrorFmt("Tap Note Score(s): %d, %d", out.tn.result.tns, out.tns);//make sure these match. They do
        LuaHelpers::ReportScriptErrorFmt("Player Number: %d", out.tn.pn); //Wrong. Always 3...
        LuaHelpers::ReportScriptErrorFmt("Player Number but real: %d", out.playerNum);
        LuaHelpers::ReportScriptErrorFmt("iRow: %d", out.iRow); //To be frank, I do not know what this is
        LuaHelpers::ReportScriptErrorFmt("iTrack: %d", out.iTrack); //Arrow/column (left, down, up, right)
        LuaHelpers::ReportScriptErrorFmt("fTapNoteOffset: %f", out.fTapNoteOffset);*/


        //Time for a format for the serial messages with the judgment info
            //Byte 0 - start byte ('S')
            //Byte 1 - message type
                //0 - Judgment (gives info on a judgment, to be used for lighting the associated panel)
                //1 - Downbeat (Signifies that a downbeat just happened. I'm not sure I will actually use this.)
                //2 - Request player number. Used to discover devices so I don't need to hardcode COM port numbers
            //Remaining bytes are only currently used for Judgments (stuff bytes (reserved) still for other messages)
            //Byte 2 - Tap note type
            //Byte 3 - Tab note subtype
            //Byte 4 - Tap note score
            //Byte 5 - Player number (but the real one)
            //Byte 6 - Arrow/column (iTrack)
            //Byte 7 - End Byte ('E')
        
        uint8_t serialMessage[8];
        serialMessage[0] = 'S';
        serialMessage[1] = out.msgType;
        serialMessage[7] = 'E';

        if(out.msgType == 0) { //if judgment message
            serialMessage[2] = out.tn.type;
            serialMessage[3] = out.tn.subType;
            serialMessage[4] = out.tns;
            serialMessage[5] = out.playerNum;
            serialMessage[6] = out.iTrack%4;
        }
        
        
        //Logic for which device a message should be sent to
            //For context, my pads are each four panels and cannot communicate to one another.
            //I parse messages to figure out which message goes to which pad
            //Assume left pad is always player 1, as long as it's not doubles
        uint8_t sendLeft = 0, sendRight = 0;
        //send to both pads if it's a downbeat
        if(out.msgType == 1) {
            sendLeft = 1;
            sendRight = 1;
        }
        //send to left pad if it is player 1 and styles are seperate or if iTrack<4 in double
        else if ((out.playerNum == PLAYER_1 && (out.styleType == StyleType_OnePlayerOneSide || out.styleType == StyleType_TwoPlayersTwoSides))
                    || ((out.styleType == StyleType_TwoPlayersSharedSides || out.styleType == StyleType_TwoPlayersTwoSides || out.styleType == StyleType_OnePlayerTwoSides) && out.iTrack < 4)) {
            sendLeft = 1;
        }
        //send to right pad if it is player 2 and styles are seperate or if iTrack >=4 in double
        else if ((out.playerNum == PLAYER_2 && (out.styleType == StyleType_OnePlayerOneSide || out.styleType == StyleType_TwoPlayersTwoSides))
                    || ((out.styleType == StyleType_TwoPlayersSharedSides || out.styleType == StyleType_TwoPlayersTwoSides || out.styleType == StyleType_OnePlayerTwoSides) && out.iTrack >= 4)) {
            sendRight = 1;
        }
        else { //we should not get here...
            LuaHelpers::ReportScriptError("Your code does not work! Unsent judgment message!");
        }


        //Send the serial messages
        if(sendLeft && p1Connected == 1) {
            p1Serial.writeBytes(serialMessage, 8);
        }
        if(sendRight && p2Connected == 1) {
            p2Serial.writeBytes(serialMessage, 8);
        }

    }

    //Shutdown serial junk
    //LuaHelpers::ReportScriptError("Closing serial ports");
    if(p1Connected == 1) {
        p1Serial.closeDevice();
    }
    if(p2Connected == 1) {
        p2Serial.closeDevice();
    }


    return 0;
}