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
void JudgmentOutputSend(TapNote tn, int iRow, int iTrack, TapNoteScore tns, float fTapNoteOffset, uint8_t playerNum) {
    //pack that data into a struct
        //The ANSI C struct equals operator shoooouulld do what I want here. I think.
    struct JudgmentOutputFrame frame;
    frame.tn = tn;
    frame.iRow = iRow;
    frame.iTrack = iTrack;
    frame.tns = tns;
    frame.fTapNoteOffset = fTapNoteOffset;
    frame.playerNum = playerNum;

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
        LuaHelpers::ReportScriptError("!!START OF NEW JUDGMENT!!");
        LuaHelpers::ReportScriptErrorFmt("Tap Note Type: %d", out.tn.type);
        LuaHelpers::ReportScriptErrorFmt("Tap Note Subtype: %d", out.tn.subType);
        LuaHelpers::ReportScriptErrorFmt("Tap Note Score(s): %d, %d", out.tn.result.tns, out.tns);//make sure these match. They do
        LuaHelpers::ReportScriptErrorFmt("Player Number: %d", out.tn.pn); //Wrong. Always 3...
        LuaHelpers::ReportScriptErrorFmt("Player Number but real: %d", out.playerNum);
        LuaHelpers::ReportScriptErrorFmt("iRow: %d", out.iRow); //To be frank, I do not know what this is
        LuaHelpers::ReportScriptErrorFmt("iTrack: %d", out.iTrack); //Arrow/column (left, down, up, right)
        LuaHelpers::ReportScriptErrorFmt("fTapNoteOffset: %f", out.fTapNoteOffset);
    }


    return 0;
}