#ifndef STATENAV_H
#define STATENAV_H

enum StateTarget
{
   GAME_SELECTION,
   GAME_CREATION,
   START_CONFIRM,
   REGISTER_FOR,
   SECRET_DISPLAY,
   IN_GAME,
};

void gotoState(enum StateTarget target, const char* parameter);
void receiveState(const char* data, unsigned size);
void refreshScoreboardIfNecessary();
void showScoreboard();

#endif
