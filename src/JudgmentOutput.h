#ifndef JudgmentOutput_H
#define JudgmentOutput_H

#include <queue>
#include <string>
#include <vector>

#include "Preference.h"
#include "RageThreads.h"
#include "RageTimer.h"
#include "GameConstantsAndTypes.h"
#include "NoteTypes.h"


struct JudgmentOutputFrame {
    TapNote tn; //All the properties of the tap note
    int iRow;
    int iTrack;
    TapNoteScore tns;
    float fTapNoteOffset;
    uint8_t playerNum; //player number from Player class directly. Value in tn seems wrong.
    uint8_t msgType; //0 = Judgment, 1 = Downbeat
    uint8_t styleType;
};


extern bool m_JudgmentOutputShutdown;
extern RageThread m_JudgmentOutputThread;
extern RageEvent* m_JudgmentOutputMutex;
extern std::queue<JudgmentOutputFrame> m_JudgmentOutputQueue;

//spawn the thread
extern void JudgmentOutputInit(void); //executed in Player constructor, for better or for worse
//kill the thread
extern void JudgmentOutputShutdown(void); //executed in Player deconstructor
//signal the thread with a new judgment event
extern void JudgmentOutputSend(TapNote tn, int iRow, int iTrack, TapNoteScore tns, float fTapNoteOffset, uint8_t playerNum, uint8_t styleType, uint8_t msgType = 0); //executed in Player::SetJudgment

#endif //JudgmentOutput_H
