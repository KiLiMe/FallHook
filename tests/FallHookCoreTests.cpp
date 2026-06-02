// AI CONTEXT: Test runner for source-free FallHook core unit modules.
// Depends on test case functions compiled into the FallHookCoreTests target.
// Runtime scope is Fallout 4 1.10.163 data semantics without loading the game.
// Version-specific logic: Fallout 4 1.10.163 only; no alternate runtime branches.
// Source-free policy: keeps orchestration only; source-free behavior lives in module tests.
void testMapping();
void testRuntimeResolution();
void testLoadOrder();
void testXmlParser();
void testTxtParser();
void testTextHelpers();
void testSourceFreeKey();
void testTranslationCatalog();
void testTranslationPipeline();
void testPluginEdidIndex();
void testConstApplyMap();

int main()
{
	testMapping();
	testRuntimeResolution();
	testLoadOrder();
	testXmlParser();
	testTxtParser();
	testTextHelpers();
	testSourceFreeKey();
	testTranslationCatalog();
	testTranslationPipeline();
	testPluginEdidIndex();
	testConstApplyMap();
	return 0;
}
