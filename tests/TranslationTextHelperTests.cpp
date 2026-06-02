// AI CONTEXT: Tests destination-text normalization and MESG control-markup preservation.
// Depends on MessageIconFormatter and UTF-8 normalization modules.
// Runtime scope is Fallout 4 1.10.163 text data semantics without loading the game.
// Version-specific logic: Fallout 4 1.10.163 only; no alternate runtime branches.
// Source-free policy: validates AGENTS-approved MESG markup preservation exception, not source-text lookup.
// Exception note: this test protects approved <> and [] control-markup preservation paths.
#include "FallHookTestSupport.h"
#include "MessageIconFormatter.h"
#include "TextNormalization.h"

void testTextHelpers()
{
	const auto normalized = TextNormalization::NormalizeUtf8("Cafe\xCC\x81");
	FallHookTestSupport::require(normalized == "Caf\xC3\xA9", "UTF-8 NFC normalization mismatch");

	const auto formatted = MessageIconFormatter::ApplyRuntimeControlMarkup(
		"Press <font face='$Controller_Buttons_Inverted'>A</font>.",
		"Press [Accept].");
	FallHookTestSupport::require(formatted.text == "Press <font face='$Controller_Buttons_Inverted'>A</font>.", "MESG icon markup preservation mismatch");
	FallHookTestSupport::require(formatted.replaced == 1, "MESG icon markup replacement count mismatch");

	const auto descFormatted = MessageIconFormatter::ApplyRuntimeControlMarkup(
		"Move between options with [Left] and [Right]. Press [Accept:16].",
		"Move with [TranslatedLeft] and [TranslatedRight]. Press [TranslatedAccept:16].");
	FallHookTestSupport::require(
		descFormatted.text == "Move with [Left] and [Right]. Press [Accept:16].",
		"MESG DESC bracket control token preservation mismatch");
	FallHookTestSupport::require(descFormatted.replaced == 3, "MESG DESC bracket control replacement count mismatch");

	const auto mixedFormatted = MessageIconFormatter::ApplyRuntimeControlMarkup(
		"Use <Alias=PlayerName> with <font face='$Controller_Buttons_Inverted'>X</font> and [Cancel:8].",
		"Use [Name] with [Build] and [No].");
	FallHookTestSupport::require(
		mixedFormatted.text == "Use <Alias=PlayerName> with <font face='$Controller_Buttons_Inverted'>X</font> and [Cancel:8].",
		"MESG mixed angle/bracket markup preservation mismatch");
	FallHookTestSupport::require(mixedFormatted.replaced == 3, "MESG mixed markup replacement count mismatch");

	const auto idPrefixFormatted = MessageIconFormatter::ApplyRuntimeControlMarkup(
		"<ID=00032168>Press [Accept].",
		"Press [TranslatedAccept].");
	FallHookTestSupport::require(idPrefixFormatted.text == "Press [Accept].", "localized string ID prefix leaked into MESG markup");
	FallHookTestSupport::require(idPrefixFormatted.replaced == 1, "localized string ID prefix changed replacement count");

	const auto fewerTranslatedTokens = MessageIconFormatter::ApplyRuntimeControlMarkup(
		"Choose [Left] or [Right].",
		"Chon [Trai].");
	FallHookTestSupport::require(
		fewerTranslatedTokens.text == "Chon [Left].",
		"MESG bracket token must use original text even when translated token count is low");
	FallHookTestSupport::require(fewerTranslatedTokens.replaced == 1, "low-count bracket replacement mismatch");

	const auto extraTranslatedTokens = MessageIconFormatter::ApplyRuntimeControlMarkup(
		"Press [Accept].",
		"Nhan [Dong y] <Alias=Bad>.");
	FallHookTestSupport::require(
		extraTranslatedTokens.text == "Nhan [Accept] .",
		"MESG extra translated control tokens must not survive");
	FallHookTestSupport::require(extraTranslatedTokens.replaced == 2, "extra-token replacement count mismatch");

	const auto richBookText = MessageIconFormatter::ApplyRuntimeControlMarkup(
		"<font face='$HandwrittenFont'>Original body.</font>",
		"<font face='$HandwrittenFont'>Noi dung dich.</font>");
	FallHookTestSupport::require(
		richBookText.text == "<font face='$HandwrittenFont'>Noi dung dich.</font>",
		"BOOK rich text font body must not be replaced by original runtime text");
	FallHookTestSupport::require(richBookText.replaced == 0, "BOOK rich text must not count as control replacement");

	const auto literalAngleText = MessageIconFormatter::ApplyRuntimeControlMarkup(
		"<Error>",
		"<Loi>");
	FallHookTestSupport::require(literalAngleText.text == "<Loi>", "literal angle text must stay translated");
	FallHookTestSupport::require(literalAngleText.replaced == 0, "literal angle text must not count as control replacement");

	const auto gameSettingPrompt = MessageIconFormatter::ApplyRuntimeControlMarkup(
		"Hold [TogglePOV] to open Workshop menu.",
		"Giu [Doi goc nhin] de mo menu Xuong thiet ke.");
	FallHookTestSupport::require(
		gameSettingPrompt.text == "Giu [TogglePOV] de mo menu Xuong thiet ke.",
		"GMST prompt bracket token must stay from original runtime text");
	FallHookTestSupport::require(gameSettingPrompt.replaced == 1, "GMST prompt token replacement count mismatch");
}
