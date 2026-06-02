// AI CONTEXT: Declares 1.11.191 XDI DialogueMenu option translation support.
// Depends on F4SE Scaleform registration and the XDI INFO option translation map.
// Runtime scope is Fallout 4 1.11.191 with the optional XDI DialogueMenu.swf integration.
// Version-specific logic: implementation owns 1.11.191 Scaleform hook installation.
// Source-free policy: resolves dialogue text from INFO form identity; visible menu text is never a lookup key.
#pragma once

namespace Runtime111191::RuntimeXdiDialogueMenuHook
{
	void Install();
}
