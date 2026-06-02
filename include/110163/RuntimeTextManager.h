// AI CONTEXT: Runtime manager for boot-time source-free text mutation and save-load refresh.
// Depends on RuntimePreload catalog output and CommonLibF4 from the plugin target.
// Runtime scope is Fallout 4 1.10.163 boot data apply plus cheap save-load map readiness.
// Version-specific logic: Fallout 4 1.10.163 only; no alternate runtime branches.
// Source-free policy: applies catalog identity records directly; no Source text fallback.
#pragma once

namespace RuntimeTextManager
{
	void ApplyBootCatalogOnce();
	void RefreshAfterSaveLoad();
}
