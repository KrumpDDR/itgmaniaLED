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
extern void JudgmentOutputSend(TapNote tn, int iRow, int iTrack, TapNoteScore tns, float fTapNoteOffset, uint8_t playerNum); //executed in Player::SetJudgment

#endif //JudgmentOutput_H
