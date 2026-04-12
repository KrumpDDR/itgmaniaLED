#include "JudgmentOutput.h"
#include "LuaManager.h"


//"global" variable that is set when the thread is started.
    //Yes I do realize how stupid it is to use a nonatomic variable for something like this.
bool m_JudgmentOutputShutdown = true;

RageThread m_JudgmentOutputThread;
RageEvent* m_JudgmentOutputMutex = nullptr;
std::queue<JudgmentOutputFrame> m_JudgmentOutputQueue;

static int JudgmentOutputThread_Main(void* p);


//Start the thread for doing the serial mumbo jumbo.
void JudgmentOutputInit() {
    //Do not create the thread again if it's already created
    if(!m_JudgmentOutputShutdown) return;

    m_JudgmentOutputShutdown = false;

    LuaHelpers::ReportScriptError("!!MAKING THREAD!!");
    
    //Prepare thread items and make it
    m_JudgmentOutputMutex = new RageEvent("JudgementOutputMutex");
    m_JudgmentOutputThread.SetName("JudgmentOutput thread");
    m_JudgmentOutputThread.Create(JudgmentOutputThread_Main, nullptr);
}

//Stop the thread
void JudgmentOutputShutdown() {
    //Do not shut down the thread again if it's already shut down
    if(!m_JudgmentOutputShutdown) return;

    
    LuaHelpers::ReportScriptError("!!STOPPING THREAD!!");    

    m_JudgmentOutputShutdown = true;

    
    if (m_JudgmentOutputMutex != nullptr) {
        m_JudgmentOutputMutex->Lock();
        m_JudgmentOutputMutex->Signal();
        m_JudgmentOutputMutex->Unlock();
    }
}

//form the structure and add to the queue, to be processed by the thread.
void JudgmentOutputSend(TapNote tn, int iRow, int iTrack, TapNoteScore tns, float fTapNoteOffset, uint8_t playerNum, uint8_t styleType, uint8_t msgType = 0) {
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

    //Setup serial Junk


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
            //Remaining bytes are only currently used for Judgments (stuff bytes still for Downbeat messages)
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
                    || ((out.styleType == StyleType_TwoPlayersSharedSides || out.styleType == StyleType_TwoPlayersTwoSides) && out.iTrack < 4)) {
            sendLeft = 1;
        }
        //send to right pad if it is player 2 and styles are seperate or if iTrack >=4 in double
        else if ((out.playerNum == PLAYER_2 && (out.styleType == StyleType_OnePlayerOneSide || out.styleType == StyleType_TwoPlayersTwoSides))
                    || ((out.styleType == StyleType_TwoPlayersSharedSides || out.styleType == StyleType_TwoPlayersTwoSides) && out.iTrack >= 4)) {
            sendRight = 1;
        }
        else { //we should not get here...
            LuaHelpers::ReportScriptError("Your code does not work! Unsent judgment message!");
        }


        //Send the serial messages
        if(sendLeft) {

        }
        if(sendRight) {
            
        }

    }

    //Shutdown serial junk

    return 0;
}