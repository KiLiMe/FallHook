// AI CONTEXT: Record-signature mapping for source-free translation targets.
// Depends on XmlTranslationMapping declarations and Shared translation types.
// Runtime assumptions: version-neutral Fallout 4 xTranslator records.
// Version-specific logic: none; runtime modules own executable-specific behavior.
// Source-free policy: only REC signatures and bracket form IDs are interpreted.
#include "XmlTranslationMapping.h"

#include <charconv>
#include <cctype>
#include <format>

namespace
{
	std::string trim(std::string_view value)
	{
		const auto begin = value.find_first_not_of(" \t\r\n");
		if (begin == std::string_view::npos)
		{
			return {};
		}

		const auto end = value.find_last_not_of(" \t\r\n");
		return std::string{ value.substr(begin, end - begin + 1) };
	}

	std::string upper(std::string value)
	{
		for (auto& ch : value)
		{
			ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
		}
		return value;
	}

	std::optional<std::uint32_t> parseHexFormID(std::string_view value)
	{
		std::uint32_t formID = 0;
		const auto result = std::from_chars(value.data(), value.data() + value.size(), formID, 16);
		if (result.ec != std::errc{} || result.ptr != value.data() + value.size())
		{
			return std::nullopt;
		}

		return formID;
	}

	bool matches(std::string_view value, std::initializer_list<std::string_view> expected)
	{
		for (const auto candidate : expected)
		{
			if (value == candidate)
			{
				return true;
			}
		}

		return false;
	}
}

namespace XmlTranslationMapping
{
	std::string NormalizeSignature(std::string_view rec)
	{
		const auto pos = rec.find(':');
		if (pos == std::string_view::npos)
		{
			return {};
		}

		auto record = upper(trim(rec.substr(0, pos)));
		auto field = upper(trim(rec.substr(pos + 1)));
		if (record.size() != 4 || field.size() != 4)
		{
			return {};
		}

		return std::format("{} {}", record, field);
	}

	TranslationType GetTranslationType(std::string_view formType)
	{
		if (formType == "LCTN FULL")
		{
			return TranslationType::kLocationFullName;
		}
		if (matches(formType, {
			"ACTI FULL", "ALCH FULL", "AMMO FULL", "APPA FULL", "ARMO FULL", "AVIF FULL", "BOOK FULL",
			"CELL FULL", "CONT FULL", "CLAS FULL", "DOOR FULL", "ENCH FULL", "EXPL FULL", "FACT FULL", "FLOR FULL",
			"FLST FULL", "FURN FULL", "HAZD FULL", "INGR FULL", "KEYM FULL", "KYWD FULL", "LIGH FULL", "MESG FULL",
			"MGEF FULL", "MISC FULL", "PERK FULL", "PROJ FULL", "QUST FULL", "RACE FULL", "SCRL FULL",
			"SHOU FULL", "SLGM FULL", "SPEL FULL", "TACT FULL", "TREE FULL", "WATR FULL", "WEAP FULL",
			"WOOP FULL", "WRLD FULL"
		}))
		{
			return TranslationType::kFullName;
		}
		if (formType == "LSCR DESC")
		{
			return TranslationType::kLoadScreenDescription;
		}
		if (formType == "MGEF DNAM")
		{
			return TranslationType::kMagicDescription;
		}
		if (formType == "ALCH DNAM")
		{
			return TranslationType::kAlchemyAddictionName;
		}
		if (formType == "AMMO ONAM")
		{
			return TranslationType::kAmmoShortDescription;
		}
		if (formType == "AVIF ANAM")
		{
			return TranslationType::kActorValueAbbreviation;
		}
		if (formType == "NPC_ SHRT")
		{
			return TranslationType::kShortName;
		}
		if (formType == "REGN RDMP")
		{
			return TranslationType::kRegion;
		}
		if (formType == "WOOP TNAM")
		{
			return TranslationType::kWordOfPower;
		}
		if (formType == "MESG ITXT")
		{
			return TranslationType::kButtonText1;
		}
		if (formType == "PERK EPF2")
		{
			return TranslationType::kButtonText2;
		}
		if (formType == "QUST NNAM")
		{
			return TranslationType::kQuestObjective;
		}
		if (formType == "PERK EPFD")
		{
			return TranslationType::kPerkVerb;
		}
		if (matches(formType, { "ACTI ATTX", "ACTI RNAM", "FLOR ATTX", "FLOR RNAM", "FURN ATTX", "NPC_ ATTX" }))
		{
			return TranslationType::kActivationText;
		}
		if (formType == "DOOR ONAM")
		{
			return TranslationType::kDoorAlternateOpenText;
		}
		if (formType == "DOOR CNAM")
		{
			return TranslationType::kDoorAlternateCloseText;
		}
		if (formType == "BPTD BPTN")
		{
			return TranslationType::kBodyPartName;
		}
		if (formType == "FACT MNAM")
		{
			return TranslationType::kFactionMaleRank;
		}
		if (formType == "FACT FNAM")
		{
			return TranslationType::kFactionFemaleRank;
		}
		if (matches(formType, {
			"DIAL FULL", "ALCH DESC", "AMMO DESC", "ARMO DESC", "AVIF DESC", "BOOK DESC", "COBJ DESC", "MESG DESC",
			"OMOD DESC", "PERK DESC", "RACE DESC", "SCRL DESC", "SHOU DESC", "SPEL DESC", "WEAP DESC", "COLL DESC"
		}))
		{
			return TranslationType::kRuntime1;
		}
		if (formType == "GMST DATA")
		{
			return TranslationType::kGameSetting;
		}
		if (formType == "REFR FULL")
		{
			return TranslationType::kReference;
		}
		if (formType == "QUST CNAM")
		{
			return TranslationType::kRuntimeLegacy;
		}
		if (formType == "NPC_ FULL")
		{
			return TranslationType::kNpcFullName;
		}
		if (matches(formType, { "BOOK CNAM", "INFO RNAM" }))
		{
			return TranslationType::kRuntime2;
		}
		if (formType == "INFO NAM1")
		{
			return TranslationType::kRuntimeIndex;
		}
		if (formType == "INNR WNAM")
		{
			return TranslationType::kInstanceNamePart;
		}
		if (formType == "LVLI ONAM")
		{
			return TranslationType::kLeveledListOverrideName;
		}
		if (formType == "MESG NNAM")
		{
			return TranslationType::kMessageShortName;
		}
		if (formType == "RACE FMRN")
		{
			return TranslationType::kRaceMorphRegionName;
		}
		if (formType == "RACE MPPN")
		{
			return TranslationType::kRaceMorphPresetName;
		}
		if (formType == "RACE TTGP")
		{
			return TranslationType::kRaceTintGroupName;
		}
		if (formType == "TERM UNAM")
		{
			return TranslationType::kTerminalResultText;
		}
		if (formType == "TERM BTXT")
		{
			return TranslationType::kTerminalBodyText;
		}
		if (formType == "TERM ITXT")
		{
			return TranslationType::kTerminalItemText;
		}
		if (formType == "TERM RNAM")
		{
			return TranslationType::kTerminalResponseText;
		}
		if (formType == "TERM NAM0")
		{
			return TranslationType::kTerminalHeaderText;
		}
		if (formType == "TERM WNAM")
		{
			return TranslationType::kTerminalWelcomeText;
		}
		if (formType.size() == 9 && formType.ends_with(" FULL"))
		{
			return TranslationType::kFullName;
		}

		return TranslationType::kUnknown;
	}

	std::optional<std::uint32_t> ParseBracketFormID(std::string_view edid)
	{
		if (edid.size() != 10 || edid.front() != '[' || edid.back() != ']')
		{
			return std::nullopt;
		}

		return parseHexFormID(edid.substr(1, 8));
	}
}
