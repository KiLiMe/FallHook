// AI CONTEXT: Builds source-free translation records from parsed XML files.
// Depends on source-free key helpers, plugin indexes, XML parser data, and XML load-order sorting.
// Runtime assumptions: version-neutral Fallout 4 translation identity.
// Version-specific logic: none; runtime modules own executable-specific behavior.
// Source-free policy: catalog keys and payloads ignore Source; sID is first-class identity.
#pragma once

#include "PluginEdidIndex.h"
#include "Shared.h"
#include "SourceFreeTranslationKey.h"
#include "XmlLoadOrder.h"
#include "XmlTranslationParser.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

struct TranslationCatalogFile
{
	XmlTranslationFile file;
	std::uint32_t pluginPriority{ 0 };
	const PluginEdidIndex* pluginIndex{ nullptr };
};

struct TranslationCatalogRecord
{
	std::string key;
	SourceFreeTranslationData data;
	std::string pluginName;
	std::string recordSignature;
};

struct TranslationCatalogBuildResult
{
	std::vector<TranslationCatalogRecord> records;
	std::size_t totalEntries{ 0 };
	std::size_t acceptedEntries{ 0 };
	std::size_t overwrittenEntries{ 0 };
	std::size_t skippedUnknownType{ 0 };
	std::size_t skippedWithoutIdentity{ 0 };
	std::size_t skippedEmptyDest{ 0 };
	std::size_t pluginStringIDIndexes{ 0 };
};

namespace TranslationCatalog
{
	TranslationCatalogBuildResult Build(std::span<const TranslationCatalogFile> files, XmlLoadOrder::Mode mode);
}
