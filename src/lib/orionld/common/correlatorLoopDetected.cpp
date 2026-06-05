/*
*
* Copyright 2026 FIWARE Foundation e.V.
*
* This file is part of Orion-LD Context Broker.
*
* Orion-LD Context Broker is free software: you can redistribute it and/or
* modify it under the terms of the GNU Affero General Public License as
* published by the Free Software Foundation, either version 3 of the
* License, or (at your option) any later version.
*
* Orion-LD Context Broker is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Affero
* General Public License for more details.
*
* You should have received a copy of the GNU Affero General Public License
* along with Orion-LD Context Broker. If not, see http://www.gnu.org/licenses/.
*
* For those usages not covered by this license please contact with
* orionld at fiware dot org
*
* Author: Carsten Frey
*/
#include <string.h>                                            // strcmp

extern "C"
{
#include "kjson/KjNode.h"                                      // KjNode
#include "kjson/kjLookup.h"                                    // kjLookup
}

#include "orionld/common/orionldState.h"                       // orionldState
#include "orionld/common/correlatorGet.h"                      // correlatorGet
#include "orionld/common/correlatorLoopDetected.h"             // Own interface



// ----------------------------------------------------------------------------
//
// correlatorLoopDetected -
//
bool correlatorLoopDetected(KjNode* dbEntityP)
{
  // Only writes triggered by a custom notification can form a notification loop
  if ((orionldState.attrsFormat == NULL) || (strcmp(orionldState.attrsFormat, "custom") != 0))
    return false;

  if (dbEntityP == NULL)
    return false;

  KjNode* lastCorrelatorP = kjLookup(dbEntityP, "lastCorrelator");
  if ((lastCorrelatorP == NULL) || (lastCorrelatorP->type != KjString))
    return false;

  // Loop: the last write on this entity carried the very same correlator as this custom-notification-triggered write
  return (strcmp(lastCorrelatorP->value.s, correlatorGet()) == 0);
}
