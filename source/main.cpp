#include "c4d.h"
#include <string.h>

Bool RegisterStlImporter();


Bool PluginStart(void)
{
    if (!RegisterStlImporter()) return FALSE;
    return TRUE;
}

void PluginEnd(void)
{	
}

Bool PluginMessage(LONG id, void *data)
{
	//use the following lines to set a plugin priority
	//
	switch (id)
	{
		case C4DPL_INIT_SYS:
			if (!resource.Init()) return FALSE; // don't start plugin without resource

		break;
	}

	return FALSE;
}