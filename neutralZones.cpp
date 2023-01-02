/*
 * Copyright (C) 2022 Vladimir "allejo" Jimenez
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <set>

#include "bzfsAPI.h"
#include "plugin_utils.h"

class NeutralZone : public bz_CustomZoneObject
{
public:
    NeutralZone() : bz_CustomZoneObject()
    {
    }

    std::vector<bz_eTeamType> teams;
    std::vector<bz_ApiString> flags;

    bool hasDisallowedFlag(const int flagID)
    {
        auto flag = bz_getFlagName(flagID);

        return std::find(flags.begin(), flags.end(), flag) != flags.end();
    }

    bool targetsTeam(bz_eTeamType team)
    {
        return std::find(teams.begin(), teams.begin(), team) != teams.end();
    }
};

class NeutralZones : public bz_Plugin, public bz_CustomMapObjectHandler
{
public:
    virtual const char* Name();
    virtual void Init(const char* config);
    virtual void Cleanup();
    virtual void Event(bz_EventData* eventData);
    virtual bool MapObject(bz_ApiString object, bz_CustomMapObjectInfo* data);

private:
    std::vector<NeutralZone> neutralZones;
};

BZ_PLUGIN(NeutralZones)

const char* NeutralZones::Name()
{
    return "Neutral Zones";
}

void NeutralZones::Init(const char* /* config */)
{
    Register(bz_ePlayerUpdateEvent);

    bz_registerCustomMapObject("NEUTRALZONE", this);
}

void NeutralZones::Cleanup()
{
    Flush();

    bz_removeCustomMapObject("NEUTRALZONE");
}

void NeutralZones::Event(bz_EventData* eventData)
{
    switch (eventData->eventType)
    {
        case bz_ePlayerUpdateEvent:
        {
            bz_PlayerUpdateEventData_V1* data = (bz_PlayerUpdateEventData_V1*)eventData;
            bz_BasePlayerRecord* pr = bz_getPlayerByIndex(data->playerID);

            // If a player doesn't have a flag, then there's no need to do anything
            if (pr->currentFlagID < 0)
            {
                bz_freePlayerRecord(pr);
                return;
            }

            for (auto zone : neutralZones)
            {
                if (zone.pointInZone(data->state.pos) && zone.targetsTeam(pr->team) && zone.hasDisallowedFlag(pr->currentFlagID))
                {
                    bz_removePlayerFlag(data->playerID);
                    bz_sendTextMessagef(BZ_SERVER, data->playerID, "You've entered a neutral zone, your %s has been removed.", pr->currentFlag.c_str());

                    break;
                }
            }

            bz_freePlayerRecord(pr);
        }
        break;

        default:
            break;
    }
}

bool NeutralZones::MapObject(bz_ApiString object, bz_CustomMapObjectInfo* data)
{
    // Note, this value will be in uppercase
    if (!data || object != "NEUTRALZONE")
    {
        return false;
    }

    NeutralZone neutralZone;
    neutralZone.handleDefaultOptions(data);

    bz_APIStringList *nubs = bz_newStringList();

    for (unsigned int i = 0; i < data->data.size(); i++)
    {
        std::string line = data->data.get(i);
        nubs->clear();

        nubs->tokenize(line.c_str(), " ", 0, true);

        if (nubs->size() > 0)
        {
            std::string key = bz_toupper(nubs->get(0).c_str());

            if (key == "TEAM")
            {
                neutralZone.teams.push_back((bz_eTeamType)atoi(nubs->get(1).c_str()));
            }
            else if (key == "FLAG")
            {
                neutralZone.flags.push_back(nubs->get(1));
            }
        }
    }

    bz_deleteStringList(nubs);

    neutralZones.push_back(neutralZone);

    return true;
}
