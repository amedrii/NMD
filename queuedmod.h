#pragma once

#include "core.h"

// A queue item may identify an entire mod or pin one exact file. Different
// required files from the same mod must remain separate queue items.
struct QueuedMod
{
    ModLink link;
    QString name;
    QString filename;
    QString reason;
    QString notes;
    QString state = "Waiting to check";
    QString gameId;
    qint64 size = 0;
    bool resolved = false;
    bool downloaded = false;
};
