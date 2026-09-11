// AI CONTEXT: Declares 1.11.240 XDI DialogueMenu option translation support.
// Depends on F4SE Scaleform registration and the XDI INFO option translation map.
// Runtime scope is Fallout 4 1.11.240 with the optional XDI DialogueMenu.swf integration.
// Version-specific logic: implementation owns 1.11.240 Scaleform hook installation.
// Source-free policy: resolves dialogue text from INFO form identity; visible menu text is never a lookup key.
#pragma once

namespace Runtime111240::RuntimeXdiDialogueMenuHook
{
	void Install();
}
