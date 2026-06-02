cmake_minimum_required(VERSION 3.21)
project(FallHook VERSION 0.1.0 LANGUAGES CXX RC)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(COMMONLIBF4_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../CommonLibF4" CACHE PATH "Path to CommonLibF4")
set(COMMONLIB_SHARED_DIR "${COMMONLIBF4_DIR}/lib/commonlib-shared")
set(FALLHOOK_DEPLOY_PLUGIN_DIR "${CMAKE_BINARY_DIR}/deploy/F4SE/Plugins" CACHE PATH "Folder that receives FallHook.dll after build")

if(NOT EXISTS "${COMMONLIBF4_DIR}/include/F4SE/F4SE.h")
    message(FATAL_ERROR "CommonLibF4 headers were not found at ${COMMONLIBF4_DIR}")
endif()
if(NOT EXISTS "${COMMONLIB_SHARED_DIR}/include/REX/REX.h")
    message(FATAL_ERROR "commonlib-shared headers were not found at ${COMMONLIB_SHARED_DIR}")
endif()

find_package(spdlog CONFIG REQUIRED)
find_package(ZLIB REQUIRED)

function(fallhook_apply_common_options target)
    target_compile_features(${target} PUBLIC cxx_std_23)
    target_compile_definitions(${target} PUBLIC WIN32_LEAN_AND_MEAN NOMINMAX)
    if(MSVC)
        target_compile_options(${target} PUBLIC
            /EHsc
            /bigobj
            /permissive-
            /Zc:preprocessor
            /Zc:templateScope
            /external:W0
            /wd4200
            /wd4201
            /wd4324
        )
    endif()
endfunction()

file(GLOB_RECURSE COMMONLIB_SHARED_SOURCES CONFIGURE_DEPENDS
    "${COMMONLIB_SHARED_DIR}/src/*.cpp"
)

add_library(commonlib-shared STATIC ${COMMONLIB_SHARED_SOURCES})
fallhook_apply_common_options(commonlib-shared)
target_include_directories(commonlib-shared PUBLIC
    "${COMMONLIB_SHARED_DIR}/include"
)
target_link_libraries(commonlib-shared PUBLIC
    spdlog::spdlog
    advapi32
    bcrypt
    d3d11
    d3dcompiler
    dbghelp
    dxgi
    ole32
    shell32
    user32
    version
    ws2_32
)
target_precompile_headers(commonlib-shared PRIVATE
    "${COMMONLIB_SHARED_DIR}/src/REX/PCH.h"
)

file(GLOB_RECURSE COMMONLIBF4_SOURCES CONFIGURE_DEPENDS
    "${COMMONLIBF4_DIR}/src/*.cpp"
)

add_library(commonlibf4 STATIC ${COMMONLIBF4_SOURCES})
fallhook_apply_common_options(commonlibf4)
target_include_directories(commonlibf4 PUBLIC
    "${COMMONLIBF4_DIR}/include"
)
target_link_libraries(commonlibf4 PUBLIC commonlib-shared)
target_precompile_headers(commonlibf4 PRIVATE
    "${COMMONLIBF4_DIR}/include/F4SE/Impl/PCH.h"
)

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Plugin.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/include/Plugin.h"
    @ONLY
)
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Version.rc.in"
    "${CMAKE_CURRENT_BINARY_DIR}/FallHook.rc"
    @ONLY
)

set(FALLHOOK_CORE_SOURCES
    src/ConstApplyMap.cpp
    src/InstanceNamingRuleIdentity.cpp
    src/MessageIconFormatter.cpp
    src/PluginEdidIndex.cpp
    src/PluginEdidIndexInnr.cpp
    src/PluginEdidIndexLookup.cpp
    src/RuntimeResolution.cpp
    src/SourceFreeTranslationKey.cpp
    src/TextNormalization.cpp
    src/TranslationCatalog.cpp
    src/TranslationPipeline.cpp
    src/TranslationPipelineCache.cpp
    src/TranslationPreparedData.cpp
    src/TxtTranslationParser.cpp
    src/XmlLoadOrder.cpp
    src/XmlTranslationMapping.cpp
    src/XmlTranslationParser.cpp
)

add_library(FallHookCore STATIC ${FALLHOOK_CORE_SOURCES})
fallhook_apply_common_options(FallHookCore)
target_include_directories(FallHookCore PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
target_link_libraries(FallHookCore PUBLIC
    ZLIB::ZLIB
    Normaliz.lib
    Shlwapi.lib
    XmlLite.lib
)

set(FALLHOOK_SHARED_PLUGIN_SOURCES
    src/main.cpp
    src/RuntimeActivityWatch.cpp
    src/RuntimeApplySettings.cpp
    src/RuntimeHookWatch.cpp
    src/RuntimeInGameTextDictionary.cpp
    src/RuntimeInGameTextLog.cpp
    src/RuntimeInGameTextSettings.cpp
    src/RuntimeLoadWatchdog.cpp
    src/RuntimeMemorySafety.cpp
    src/RuntimePrologueHook.cpp
    src/RuntimeSaveLoadGapTrace.cpp
    src/RuntimeVersionDispatcher.cpp
    src/RuntimeWatchdogSettings.cpp
)

set(FALLHOOK_RUNTIME_110163_SOURCES
    src/110163/RuntimeActivationLookContext.cpp
    src/110163/RuntimeActivationContextHook.cpp
    src/110163/RuntimeActivationTextTranslations.cpp
    src/110163/RuntimeActorValueAnamFixer.cpp
    src/110163/RuntimeApplyTrace.cpp
    src/110163/RuntimeCatalogProvider.cpp
    src/110163/RuntimeConstApplyCache.cpp
    src/110163/RuntimeDescriptionHook.cpp
    src/110163/RuntimeDescriptionTranslations.cpp
    src/110163/RuntimeDialogueButtonHook.cpp
    src/110163/RuntimeDialogueChoiceContext.cpp
    src/110163/RuntimeDialogueChoiceTranslations.cpp
    src/110163/RuntimeDialogueDataApply.cpp
    src/110163/RuntimeDialogueForms.cpp
    src/110163/RuntimeDialogueResponseHook.cpp
    src/110163/RuntimeDialogueResponseTranslations.cpp
    src/110163/RuntimeFormResolver.cpp
    src/110163/RuntimeFullNameLoadHook.cpp
    src/110163/RuntimeFullNameLoadTranslations.cpp
    src/110163/RuntimeHudRolloverActionPatch.cpp
    src/110163/RuntimeHudRolloverHook.cpp
    src/110163/RuntimeInGameTextHook.cpp
    src/110163/RuntimeInGameTextHookPatch.cpp
    src/110163/RuntimeInGameTextSafeCopy.cpp
    src/110163/RuntimeInGameTextScaleformStream.cpp
    src/110163/RuntimeInstanceNamingTranslations.cpp
    src/110163/RuntimeInventoryTemplateNames.cpp
    src/110163/RuntimeLocalizedStringID.cpp
    src/110163/RuntimeModule110163.cpp
    src/110163/RuntimePerkActivateChoiceTranslations.cpp
    src/110163/RuntimePipboyLogHook.cpp
    src/110163/RuntimePipboyLogTranslations.cpp
    src/110163/RuntimePreload.cpp
    src/110163/RuntimeQuestJournalTextHook.cpp
    src/110163/RuntimeQuestLogTranslations.cpp
    src/110163/RuntimeRegionMapText.cpp
    src/110163/RuntimeTextApply.cpp
    src/110163/RuntimeTextIndexedLists.cpp
    src/110163/RuntimeTextManager.cpp
    src/110163/RuntimeTextReference.cpp
    src/110163/RuntimeTextSimpleFields.cpp
    src/110163/RuntimeTextStringAssign.cpp
    src/110163/RuntimeTextTerminal.cpp
    src/110163/RuntimeXdiDialogueOptionTranslations.cpp
    src/110163/RuntimeXdiDialogueOptionResolver.cpp
    src/110163/RuntimeXdiDialogueInfoResolver.cpp
    src/110163/RuntimeXdiDialogueMenuHook.cpp
)

set(FALLHOOK_RUNTIME_111191_SOURCES
    src/111191/RuntimeActivationContextHook.cpp
    src/111191/RuntimeActivationLookContext.cpp
    src/111191/RuntimeActivationTextTranslations.cpp
    src/111191/RuntimeActorValueAnamFixer.cpp
    src/111191/RuntimeApplyTrace.cpp
    src/111191/RuntimeCatalogProvider.cpp
    src/111191/RuntimeConstApplyCache.cpp
    src/111191/RuntimeDescriptionHook.cpp
    src/111191/RuntimeDescriptionTranslations.cpp
    src/111191/RuntimeDialogueDataApply.cpp
    src/111191/RuntimeDialogueButtonHook.cpp
    src/111191/RuntimeDialogueChoiceContext.cpp
    src/111191/RuntimeDialogueChoiceTranslations.cpp
    src/111191/RuntimeDialogueForms.cpp
    src/111191/RuntimeDialogueResponseHook.cpp
    src/111191/RuntimeDialogueResponseTranslations.cpp
    src/111191/RuntimeFormResolver.cpp
    src/111191/RuntimeFullNameLoadHook.cpp
    src/111191/RuntimeFullNameLoadTranslations.cpp
    src/111191/RuntimeHudRolloverActionPatch.cpp
    src/111191/RuntimeHudRolloverHook.cpp
    src/111191/RuntimeInGameTextHook.cpp
    src/111191/RuntimeInGameTextHookPatch.cpp
    src/111191/RuntimeInGameTextSafeCopy.cpp
    src/111191/RuntimeInGameTextScaleformStream.cpp
    src/111191/RuntimeInstanceNamingTranslations.cpp
    src/111191/RuntimeInventoryTemplateNames.cpp
    src/111191/RuntimeLocalizedStringID.cpp
    src/111191/RuntimeModule111191.cpp
    src/111191/RuntimePerkActivateChoiceTranslations.cpp
    src/111191/RuntimePipboyLogHook.cpp
    src/111191/RuntimePipboyLogTranslations.cpp
    src/111191/RuntimePreload.cpp
    src/111191/RuntimeQuestJournalTextHook.cpp
    src/111191/RuntimeQuestLogTranslations.cpp
    src/111191/RuntimeRegionMapText.cpp
    src/111191/RuntimeTextApply.cpp
    src/111191/RuntimeTextIndexedLists.cpp
    src/111191/RuntimeTextManager.cpp
    src/111191/RuntimeTextReference.cpp
    src/111191/RuntimeTextSimpleFields.cpp
    src/111191/RuntimeTextStringAssign.cpp
    src/111191/RuntimeTextTerminal.cpp
    src/111191/RuntimeXdiDialogueInfoResolver.cpp
    src/111191/RuntimeXdiDialogueMenuHook.cpp
    src/111191/RuntimeXdiDialogueOptionResolver.cpp
    src/111191/RuntimeXdiDialogueOptionTranslations.cpp
)

add_library(FallHook SHARED
    ${FALLHOOK_SHARED_PLUGIN_SOURCES}
    ${FALLHOOK_RUNTIME_110163_SOURCES}
    ${FALLHOOK_RUNTIME_111191_SOURCES}
    "${CMAKE_CURRENT_BINARY_DIR}/FallHook.rc"
)
fallhook_apply_common_options(FallHook)
target_include_directories(FallHook PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_BINARY_DIR}/include"
)
target_link_libraries(FallHook PRIVATE
    FallHookCore
    commonlibf4
    spdlog::spdlog
)
target_precompile_headers(FallHook PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include/PCH.h"
)

add_custom_command(TARGET FallHook POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${FALLHOOK_DEPLOY_PLUGIN_DIR}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:FallHook>"
        "${FALLHOOK_DEPLOY_PLUGIN_DIR}/FallHook.dll"
    VERBATIM
)

include(CTest)
if(BUILD_TESTING)
    add_executable(FallHookCoreTests
        tests/ConstApplyMapTests.cpp
        tests/FallHookCoreTests.cpp
        tests/FallHookTestSupport.cpp
        tests/PluginEdidIndexTests.cpp
        tests/TranslationCatalogPipelineTests.cpp
        tests/TranslationMappingTests.cpp
        tests/TranslationParserTests.cpp
        tests/TranslationTextHelperTests.cpp
    )
    fallhook_apply_common_options(FallHookCoreTests)
    target_link_libraries(FallHookCoreTests PRIVATE
        FallHookCore
    )
    add_test(NAME FallHookCoreTests COMMAND FallHookCoreTests)
endif()

if(MSVC_VERSION GREATER_EQUAL 1936 AND MSVC_IDE)
    file(CONFIGURE OUTPUT "${CMAKE_BINARY_DIR}/Directory.Build.props" CONTENT [==[
    <Project>
      <ItemDefinitionGroup>
        <ClCompile>
          <BuildStlModules>false</BuildStlModules>
        </ClCompile>
      </ItemDefinitionGroup>
    </Project>
    ]==] @ONLY)
endif()
