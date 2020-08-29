#pragma once

#include <bakkesmod/wrappers/GameEvent/ServerWrapper.h>
#include "CarState.h"
#include "BallState.h"

class GameState
{
public:
    GameState();

    explicit GameState(ServerWrapper &serverWrapper);

    void applyTo(ServerWrapper &serverWrapper) const;

    void render(const std::string &tag);

public:
    CarState carState;
    BallState ballState;
};