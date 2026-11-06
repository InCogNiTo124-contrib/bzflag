cmake_minimum_required(VERSION 3.20)
project(BZFlag VERSION 2.5.3 LANGUAGES C CXX)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE "RelWithDebInfo" CACHE STRING "Choose the type of build." FORCE)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

option(BUILD_CLIENT "Build BZFlag graphical client" ON)
option(BUILD_BZADMIN "Build bzadmin administration client" ON)
option(BUILD_PLUGINS "Build BZFlag server plugins" ON)
option(BUILD_TOOLS "Build BZFlag utilities (bzwtransform, modeltool, TextTool2)" ON)
option(ENABLE_UPNP "Enable UPnP support via miniupnpc if available" ON)
option(ENABLE_WOVERRIDE "Enable warnings for missing override of virtual functions" ON)

add_compile_options(
  -Wall
  -Wextra
  -Wcast-qual
  -Wredundant-decls
  -Wshadow
  -Wundef
  -pedantic
  -fsigned-char
)

add_compile_definitions(
  $<$<CONFIG:Debug>:DEBUG>
  $<$<CONFIG:Debug>:DEBUG_RENDERING>
)

if(ENABLE_WOVERRIDE)
  add_compile_options(
    $<$<COMPILE_LANGUAGE:CXX>:-Wsuggest-override>
    $<$<COMPILE_LANGUAGE:CXX>:-Woverloaded-virtual>
  )
endif()

include(GNUInstallDirs)
include(CheckTypeSize)
include(CheckIncludeFile)

check_type_size("int" SIZEOF_INT)
check_type_size("short int" SIZEOF_SHORT_INT)
check_type_size("long int" SIZEOF_LONG_INT)
check_type_size("long long int" SIZEOF_LONG_LONG_INT)
check_type_size("float" SIZEOF_FLOAT)
check_type_size("double" SIZEOF_DOUBLE)
check_type_size("long double" SIZEOF_LONG_DOUBLE)

check_include_file("regex.h" HAVE_REGEX_H)
check_include_file("linux/input.h" HAVE_LINUX_INPUT_H)
check_include_file("sched.h" HAVE_SCHED_H)
if(CMAKE_CXX_BYTE_ORDER STREQUAL "BIG_ENDIAN")
  set(WORDS_BIGENDIAN 1)
endif()

if(ENABLE_UPNP)
  find_library(MINIUPNPC_LIBRARY NAMES miniupnpc)
  find_path(MINIUPNPC_INCLUDE_DIR NAMES miniupnpc/miniupnpc.h)
  if(MINIUPNPC_LIBRARY AND MINIUPNPC_INCLUDE_DIR)
    set(HAVE_MINIUPNPC_MINIUPNPC_H 1)
    set(LIBMINIUPNPC 1)
  endif()
endif()

find_package(Threads REQUIRED)
find_package(ZLIB REQUIRED)
find_package(CURL 7.85.0 REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(CARES REQUIRED IMPORTED_TARGET libcares>=1.26.0)

if(BUILD_CLIENT OR BUILD_TOOLS)
  find_package(PNG REQUIRED)
endif()

if(BUILD_CLIENT)
  find_package(OpenGL REQUIRED)
  find_package(GLEW REQUIRED)
  find_package(SDL3 3.2.12 CONFIG REQUIRED)
endif()

if(BUILD_TOOLS)
  find_package(Freetype REQUIRED)
endif()

if(BUILD_BZADMIN)
  find_package(Curses)
  if(CURSES_FOUND)
    set(HAVE_NCURSES_H 1)
  endif()
endif()

string(TIMESTAMP BUILD_DATE "%Y-%m-%d" UTC)
set(BZFLAG_DATA "${CMAKE_INSTALL_FULL_DATADIR}/bzflag")
set(INSTALL_DATA_DIR "${BZFLAG_DATA}")
set(PACKAGE_STRING "BZFlag ${PROJECT_VERSION}")
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  set(BZ_BUILD_OS "linux-gnu")
else()
  string(TOLOWER "${CMAKE_SYSTEM_NAME}" BZ_BUILD_OS)
endif()

configure_file(
  ${CMAKE_SOURCE_DIR}/cmake/config.h.in
  ${CMAKE_BINARY_DIR}/include/config.h
  @ONLY
)

include_directories(
  ${CMAKE_BINARY_DIR}/include
  ${CMAKE_SOURCE_DIR}/include
)
if(MINIUPNPC_INCLUDE_DIR)
  include_directories(${MINIUPNPC_INCLUDE_DIR})
endif()
if(CURSES_FOUND)
  include_directories(${CURSES_INCLUDE_DIRS})
endif()

add_compile_definitions(
  HAVE_CONFIG_H
  INSTALL_DATA_DIR="${BZFLAG_DATA}"
)

# Internal static libraries
add_library(libDate STATIC
  src/date/buildDate.cxx
)

add_library(libCommon STATIC
  src/common/AccessList.cxx
  src/common/AutoCompleter.cxx
  src/common/BZDBCache.cxx
  src/common/BZDBLocal.cxx
  src/common/Bundle.cxx
  src/common/BundleMgr.cxx
  src/common/CommandManager.cxx
  src/common/ConfigFileManager.cxx
  src/common/Country.cxx
  src/common/ErrorHandler.cxx
  src/common/FileManager.cxx
  src/common/Flag.cxx
  src/common/GameTime.cxx
  src/common/KeyManager.cxx
  src/common/Language.cxx
  src/common/OSFile.cxx
  src/common/ParseColor.cxx
  src/common/PlayerState.cxx
  src/common/ShotUpdate.cxx
  src/common/StateDatabase.cxx
  src/common/Team.cxx
  src/common/TextChunkManager.cxx
  src/common/TextUtils.cxx
  src/common/TimeKeeper.cxx
  src/common/VotingBooth.cxx
  src/common/WordFilter.cxx
  src/common/bz_Locale.cxx
  src/common/bzfio.cxx
  src/common/bzglob.cxx
  src/common/bzsignal.cxx
  src/common/cURLManager.cxx
  src/common/global.cxx
  src/common/mathRoutine.cxx
  src/common/md5.cxx
  src/common/messages.cxx
)
target_link_libraries(libCommon PUBLIC CURL::libcurl ZLIB::ZLIB m)

add_library(libNet STATIC
  src/net/Address.cxx
  src/net/AresHandler.cxx
  src/net/Pack.cxx
  src/net/Ping.cxx
  src/net/multicast.cxx
  src/net/network.cxx
)
target_link_libraries(libNet PUBLIC PkgConfig::CARES)
if(MINIUPNPC_LIBRARY)
  target_link_libraries(libNet PUBLIC ${MINIUPNPC_LIBRARY})
endif()

add_library(libGame STATIC
  src/game/BzMaterial.cxx
  src/game/CacheManager.cxx
  src/game/CollisionManager.cxx
  src/game/CommandsStandard.cxx
  src/game/DirectoryNames.cxx
  src/game/DynamicColor.cxx
  src/game/Frustum.cxx
  src/game/Intersect.cxx
  src/game/LagInfo.cxx
  src/game/LinkManager.cxx
  src/game/MsgStrings.cxx
  src/game/MeshTransform.cxx
  src/game/NetHandler.cxx
  src/game/PhysicsDriver.cxx
  src/game/PlayerInfo.cxx
  src/game/Ray.cxx
  src/game/ServerItem.cxx
  src/game/ServerAuth.cxx
  src/game/ServerList.cxx
  src/game/ServerListCache.cxx
  src/game/StartupInfo.cxx
  src/game/TextureMatrix.cxx
)
target_link_libraries(libGame PUBLIC CURL::libcurl ZLIB::ZLIB)

add_library(libObstacle STATIC
  src/obstacle/BaseSceneNodeGenerator.cxx
  src/obstacle/BoxSceneNodeGenerator.cxx
  src/obstacle/EmptySceneNodeGenerator.cxx
  src/obstacle/ObstacleSceneNodeGenerator.cxx
  src/obstacle/PyramidSceneNodeGenerator.cxx
  src/obstacle/TeleporterSceneNodeGenerator.cxx
  src/obstacle/WallSceneNodeGenerator.cxx
  src/obstacle/MeshSceneNodeGenerator.cxx
  src/obstacle/ArcObstacle.cxx
  src/obstacle/ConeObstacle.cxx
  src/obstacle/SphereObstacle.cxx
  src/obstacle/BaseBuilding.cxx
  src/obstacle/BoxBuilding.cxx
  src/obstacle/MeshFace.cxx
  src/obstacle/MeshObstacle.cxx
  src/obstacle/Triangulate.cxx
  src/obstacle/MeshDrawInfo.cxx
  src/obstacle/PyramidBuilding.cxx
  src/obstacle/Teleporter.cxx
  src/obstacle/TetraBuilding.cxx
  src/obstacle/WallObstacle.cxx
  src/obstacle/Obstacle.cxx
  src/obstacle/ObstacleModifier.cxx
  src/obstacle/ObstacleList.cxx
  src/obstacle/ObstacleMgr.cxx
)

if(BUILD_CLIENT)
add_library(libPlatform STATIC
  src/platform/PlatformFactory.cxx
  src/platform/BzfDisplay.cxx
  src/platform/BzfJoystick.cxx
  src/platform/BzfVisual.cxx
  src/platform/BzfWindow.cxx
  src/platform/BzfMedia.cxx
  src/platform/SDLJoystick.cxx
  src/platform/SDLPlatformFactory.cxx
  src/platform/SDLMedia.cxx
  src/platform/SDL2Display.cxx
  src/platform/SDL2Visual.cxx
  src/platform/SDL2Window.cxx
)
target_link_libraries(libPlatform PUBLIC SDL3::SDL3)
if(APPLE)
  target_sources(libPlatform PRIVATE src/platform/MacDataPath.cxx)
  target_link_libraries(libPlatform PUBLIC "-framework CoreFoundation")
endif()

add_library(libGLKit STATIC
  src/ogl/OpenGLCommon.cxx
  src/ogl/OpenGLFramebuffer.cxx
  src/ogl/OpenGLGState.cxx
  src/ogl/OpenGLLight.cxx
  src/ogl/OpenGLMaterial.cxx
  src/ogl/OpenGLTexture.cxx
  src/ogl/OpenGLUtils.cxx
  src/ogl/RenderNode.cxx
)
target_link_libraries(libGLKit PUBLIC OpenGL::GL OpenGL::GLU GLEW::GLEW SDL3::SDL3)

add_library(lib3D STATIC
  src/3D/FontManager.cxx
  src/3D/ImageFont.cxx
  src/3D/TextureFont.cxx
  src/3D/TextureManager.cxx
)

add_library(libSceneDB STATIC
  src/scene/Occluder.cxx
  src/scene/Octree.cxx
  src/scene/SceneDatabase.cxx
  src/scene/ZSceneDatabase.cxx
)

add_library(libGeometry STATIC
  src/geometry/AnimatedTreads.cxx
  src/geometry/BillboardSceneNode.cxx
  src/geometry/BoltSceneNode.cxx
  src/geometry/EighthDBaseSceneNode.cxx
  src/geometry/EighthDBoxSceneNode.cxx
  src/geometry/EighthDimSceneNode.cxx
  src/geometry/EighthDimShellNode.cxx
  src/geometry/EighthDPyrSceneNode.cxx
  src/geometry/FlagSceneNode.cxx
  src/geometry/FlagWarpSceneNode.cxx
  src/geometry/LaserSceneNode.cxx
  src/geometry/MeshDrawMgr.cxx
  src/geometry/MeshFragSceneNode.cxx
  src/geometry/MeshPolySceneNode.cxx
  src/geometry/MeshRenderNode.cxx
  src/geometry/MeshSceneNode.cxx
  src/geometry/ModelRender.cxx
  src/geometry/OccluderSceneNode.cxx
  src/geometry/QuadWallSceneNode.cxx
  src/geometry/SceneNode.cxx
  src/geometry/SphereSceneNode.cxx
  src/geometry/TankGeometryMgr.cxx
  src/geometry/TankSceneNode.cxx
  src/geometry/TriWallSceneNode.cxx
  src/geometry/ViewFrustum.cxx
  src/geometry/WavefrontOBJ.cxx
  src/geometry/WallSceneNode.cxx
  src/geometry/models/tank/high_barrel.cxx
  src/geometry/models/tank/high_body.cxx
  src/geometry/models/tank/high_ltread.cxx
  src/geometry/models/tank/high_rtread.cxx
  src/geometry/models/tank/high_turret.cxx
  src/geometry/models/tank/low_barrel.cxx
  src/geometry/models/tank/low_body.cxx
  src/geometry/models/tank/low_ltread.cxx
  src/geometry/models/tank/low_rtread.cxx
  src/geometry/models/tank/low_turret.cxx
  src/geometry/models/tank/medium_body.cxx
  src/geometry/models/tank/medium_ltread.cxx
  src/geometry/models/tank/medium_rtread.cxx
  src/geometry/models/tank/medium_turret.cxx
)

add_library(libMediaFile STATIC
  src/mediafile/ImageFile.cxx
  src/mediafile/MediaFile.cxx
  src/mediafile/PNGImageFile.cxx
  src/mediafile/AudioFile.cxx
  src/mediafile/WaveAudioFile.cxx
)
target_link_libraries(libMediaFile PUBLIC PNG::PNG ZLIB::ZLIB)

# Executables
add_executable(bzflag
  src/bzflag/ActionBinding.cxx
  src/bzflag/AutoPilot.cxx
  src/bzflag/BackgroundRenderer.cxx
  src/bzflag/BaseLocalPlayer.cxx
  src/bzflag/CacheMenu.cxx
  src/bzflag/clientConfig.cxx
  src/bzflag/CommandsImplementation.cxx
  src/bzflag/ComposeDefaultKey.cxx
  src/bzflag/ControlPanel.cxx
  src/bzflag/defaultBZDB.cxx
  src/bzflag/DisplayMenu.cxx
  src/bzflag/Downloads.cxx
  src/bzflag/EffectsMenu.cxx
  src/bzflag/effectsRenderer.cxx
  src/bzflag/EntryZone.cxx
  src/bzflag/FlashClock.cxx
  src/bzflag/ForceFeedback.cxx
  src/bzflag/FormatMenu.cxx
  src/bzflag/GuidedMissleStrategy.cxx
  src/bzflag/GUIOptionsMenu.cxx
  src/bzflag/HelpMenu.cxx
  src/bzflag/HUDDialog.cxx
  src/bzflag/HUDDialogStack.cxx
  src/bzflag/HUDRenderer.cxx
  src/bzflag/HUDui.cxx
  src/bzflag/HUDuiControl.cxx
  src/bzflag/HUDuiDefaultKey.cxx
  src/bzflag/HUDuiJSTestLabel.cxx
  src/bzflag/HUDuiLabel.cxx
  src/bzflag/HUDuiList.cxx
  src/bzflag/HUDuiTextureLabel.cxx
  src/bzflag/HUDuiTypeIn.cxx
  src/bzflag/JoystickTestMenu.cxx
  src/bzflag/JoinMenu.cxx
  src/bzflag/InputMenu.cxx
  src/bzflag/KeyboardMapMenu.cxx
  src/bzflag/LocalCommand.cxx
  src/bzflag/LocalPlayer.cxx
  src/bzflag/MainMenu.cxx
  src/bzflag/MainWindow.cxx
  src/bzflag/MenuDefaultKey.cxx
  src/bzflag/motd.cxx
  src/bzflag/NewVersionMenu.cxx
  src/bzflag/OptionsMenu.cxx
  src/bzflag/Player.cxx
  src/bzflag/Plan.cxx
  src/bzflag/QuickKeysMenu.cxx
  src/bzflag/QuitMenu.cxx
  src/bzflag/RadarRenderer.cxx
  src/bzflag/Region.cxx
  src/bzflag/RegionPriorityQueue.cxx
  src/bzflag/RemotePlayer.cxx
  src/bzflag/Roaming.cxx
  src/bzflag/RobotPlayer.cxx
  src/bzflag/RoofTops.cxx
  src/bzflag/Roster.cxx
  src/bzflag/SaveWorldMenu.cxx
  src/bzflag/SceneBuilder.cxx
  src/bzflag/SceneRenderer.cxx
  src/bzflag/ScoreboardRenderer.cxx
  src/bzflag/SegmentedShotStrategy.cxx
  src/bzflag/ServerCommandKey.cxx
  src/bzflag/ServerLink.cxx
  src/bzflag/ServerListFilter.cxx
  src/bzflag/ServerListFilterHelpMenu.cxx
  src/bzflag/ServerListFilterMenu.cxx
  src/bzflag/ServerMenu.cxx
  src/bzflag/ServerStartMenu.cxx
  src/bzflag/ShockWaveStrategy.cxx
  src/bzflag/ShotList.cxx
  src/bzflag/ShotPath.cxx
  src/bzflag/ShotPathSegment.cxx
  src/bzflag/ShotStats.cxx
  src/bzflag/ShotStatsDefaultKey.cxx
  src/bzflag/ShotStatistics.cxx
  src/bzflag/ShotStrategy.cxx
  src/bzflag/SilenceDefaultKey.cxx
  src/bzflag/TargetingUtils.cxx
  src/bzflag/TrackMarks.cxx
  src/bzflag/Weapon.cxx
  src/bzflag/WeatherRenderer.cxx
  src/bzflag/World.cxx
  src/bzflag/WorldBuilder.cxx
  src/bzflag/WorldPlayer.cxx
  src/bzflag/bzflag.cxx
  src/bzflag/callbacks.cxx
  src/bzflag/clientCommands.cxx
  src/bzflag/daylight.cxx
  src/bzflag/playing.cxx
  src/bzflag/sound.cxx
  src/bzflag/stars.cxx
)
set_target_properties(bzflag PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/src/bzflag"
)
target_link_libraries(bzflag PRIVATE
  lib3D
  libGame
  libNet
  libObstacle
  libPlatform
  libGLKit
  libSceneDB
  libGeometry
  libMediaFile
  libCommon
  libDate
  PkgConfig::CARES
  PNG::PNG
  ZLIB::ZLIB
  SDL3::SDL3
  OpenGL::GL
  OpenGL::GLU
  GLEW::GLEW
  CURL::libcurl
  Threads::Threads
  dl
  m
)
endif()

add_executable(bzfs
  src/bzfs/bzfsPlugins.cxx
  src/bzfs/AccessControlList.cxx
  src/bzfs/Authentication.cxx
  src/bzfs/BanCommands.cxx
  src/bzfs/bzfsAPI.cxx
  src/bzfs/bzfsAPIServerSidePlayers.cxx
  src/bzfs/bzfsAPIWorld.cxx
  src/bzfs/BZWError.cxx
  src/bzfs/BZWReader.cxx
  src/bzfs/CmdLineOptions.cxx
  src/bzfs/CustomArc.cxx
  src/bzfs/CustomBase.cxx
  src/bzfs/CustomBox.cxx
  src/bzfs/CustomCone.cxx
  src/bzfs/CustomDynamicColor.cxx
  src/bzfs/CustomGate.cxx
  src/bzfs/CustomGroup.cxx
  src/bzfs/CustomLink.cxx
  src/bzfs/CustomMaterial.cxx
  src/bzfs/CustomMesh.cxx
  src/bzfs/CustomMeshFace.cxx
  src/bzfs/CustomMeshTransform.cxx
  src/bzfs/CustomPhysicsDriver.cxx
  src/bzfs/CustomSphere.cxx
  src/bzfs/CustomTextureMatrix.cxx
  src/bzfs/CustomPyramid.cxx
  src/bzfs/CustomTetra.cxx
  src/bzfs/CustomWaterLevel.cxx
  src/bzfs/CustomWeapon.cxx
  src/bzfs/CustomWorld.cxx
  src/bzfs/CustomZone.cxx
  src/bzfs/DropGeometry.cxx
  src/bzfs/EntryZones.cxx
  src/bzfs/FlagHistory.cxx
  src/bzfs/FlagInfo.cxx
  src/bzfs/GameKeeper.cxx
  src/bzfs/ListServerConnection.cxx
  src/bzfs/MasterBanList.cxx
  src/bzfs/MessageBuffers.cxx
  src/bzfs/ParseMaterial.cxx
  src/bzfs/Permissions.cxx
  src/bzfs/RandomSpawnPolicy.cxx
  src/bzfs/RecordReplay.cxx
  src/bzfs/RejoinList.cxx
  src/bzfs/RobotUtils.cxx
  src/bzfs/Score.cxx
  src/bzfs/ServerCommand.cxx
  src/bzfs/ServerSidePlayer.cxx
  src/bzfs/ShotManager.cxx
  src/bzfs/SpawnPolicy.cxx
  src/bzfs/SpawnPosition.cxx
  src/bzfs/TeamBases.cxx
  src/bzfs/VotingArbiter.cxx
  src/bzfs/WorldFileLocation.cxx
  src/bzfs/WorldFileObject.cxx
  src/bzfs/WorldFileObstacle.cxx
  src/bzfs/WorldGenerators.cxx
  src/bzfs/WorldInfo.cxx
  src/bzfs/WorldWeapons.cxx
  src/bzfs/WorldEventManager.cxx
  src/bzfs/commands.cxx
  src/bzfs/bzfs.cxx
)
target_compile_definitions(bzfs PRIVATE
  INSIDE_BZ
  INSTALL_LIB_DIR="${CMAKE_INSTALL_FULL_LIBDIR}/bzflag/"
)
set_target_properties(bzfs PROPERTIES
  ENABLE_EXPORTS ON
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/src/bzfs"
)
target_link_libraries(bzfs PRIVATE
  libObstacle
  libGame
  libNet
  libCommon
  libDate
  CURL::libcurl
  PkgConfig::CARES
  ZLIB::ZLIB
  Threads::Threads
  dl
  m
)

if(BUILD_BZADMIN)
set(BZADMIN_SOURCES
  src/bzadmin/BZAdminClient.cxx
  src/bzadmin/BZAdminUI.cxx
  src/bzadmin/OptionParser.cxx
  src/bzadmin/ServerLink.cxx
  src/bzadmin/StdBothUI.cxx
  src/bzadmin/StdInUI.cxx
  src/bzadmin/StdOutUI.cxx
  src/bzadmin/UIMap.cxx
  src/bzadmin/bzadmin.cxx
)
if(CURSES_FOUND)
  list(APPEND BZADMIN_SOURCES
    src/bzadmin/CursesUI.cxx
    src/bzadmin/CursesMenu.cxx
  )
endif()
add_executable(bzadmin ${BZADMIN_SOURCES})
target_compile_definitions(bzadmin PRIVATE BUILDING_BZADMIN)
set_target_properties(bzadmin PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/src/bzadmin"
)
target_link_libraries(bzadmin PRIVATE
  libDate
  libGame
  libNet
  libCommon
  CURL::libcurl
  PkgConfig::CARES
  ZLIB::ZLIB
  Threads::Threads
  m
)
if(CURSES_FOUND)
  target_link_libraries(bzadmin PRIVATE ${CURSES_LIBRARIES})
endif()
endif()

# Server plugins
if(BUILD_PLUGINS)
add_library(plugin_utils STATIC
  plugins/plugin_utils/plugin_utils.cpp
  plugins/plugin_utils/plugin_files.cpp
  plugins/plugin_utils/plugin_groups.cpp
  plugins/plugin_utils/plugin_config.cpp
)
target_include_directories(plugin_utils PUBLIC ${CMAKE_SOURCE_DIR}/plugins/plugin_utils)

function(add_bzflag_plugin PLUGIN_NAME)
  cmake_parse_arguments(PLUGIN "USE_UTILS" "" "SOURCES" ${ARGN})
  add_library(${PLUGIN_NAME} MODULE ${PLUGIN_SOURCES})
  set_target_properties(${PLUGIN_NAME} PROPERTIES
    PREFIX ""
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins/${PLUGIN_NAME}"
  )
  if(PLUGIN_USE_UTILS)
    target_link_libraries(${PLUGIN_NAME} PRIVATE plugin_utils)
  endif()
  install(TARGETS ${PLUGIN_NAME} LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}/bzflag")
endfunction()

add_bzflag_plugin(airspawn SOURCES plugins/airspawn/airspawn.cpp)
add_bzflag_plugin(autoFlagReset SOURCES plugins/autoFlagReset/autoFlagReset.cpp)
add_bzflag_plugin(bzfscron USE_UTILS SOURCES plugins/bzfscron/bzfscron.cpp plugins/bzfscron/CronJob.cpp)
add_bzflag_plugin(changeTeam USE_UTILS SOURCES plugins/changeTeam/changeTeam.cpp)
add_bzflag_plugin(chathistory SOURCES plugins/chathistory/chathistory.cpp)
add_bzflag_plugin(customflagsample USE_UTILS SOURCES plugins/customflagsample/customflagsample.cpp)
add_bzflag_plugin(customPollTypeSample USE_UTILS SOURCES plugins/customPollTypeSample/customPollTypeSample.cpp)
add_bzflag_plugin(CustomZoneSample USE_UTILS SOURCES plugins/CustomZoneSample/CustomZoneSample.cpp)
add_bzflag_plugin(fairCTF SOURCES plugins/fairCTF/fairCTF.cpp)
add_bzflag_plugin(flagStay SOURCES plugins/flagStay/flagStay.cpp)
add_bzflag_plugin(genocide USE_UTILS SOURCES plugins/genocide/genocide.cpp)
add_bzflag_plugin(HoldTheFlag SOURCES plugins/HoldTheFlag/HoldTheFlag.cpp)
add_bzflag_plugin(keepaway SOURCES plugins/keepaway/keepaway.cpp)
add_bzflag_plugin(killall SOURCES plugins/killall/killall.cpp)
add_bzflag_plugin(koth SOURCES plugins/koth/koth.cpp)
add_bzflag_plugin(logDetail SOURCES plugins/logDetail/logDetail.cpp)
add_bzflag_plugin(nagware SOURCES plugins/nagware/nagware.cpp)
add_bzflag_plugin(Phoenix SOURCES plugins/Phoenix/Phoenix.cpp)
add_bzflag_plugin(playHistoryTracker SOURCES plugins/playHistoryTracker/playHistoryTracker.cpp)
add_bzflag_plugin(pushstats USE_UTILS SOURCES plugins/pushstats/pushstats.cpp)
add_bzflag_plugin(R2 USE_UTILS SOURCES plugins/R2/R2.cpp plugins/R2/playerHandler.cpp)
add_bzflag_plugin(rabbitTimer SOURCES plugins/rabbitTimer/rabbitTimer.cpp)
add_bzflag_plugin(rabidRabbit SOURCES plugins/rabidRabbit/rabidRabbit.cpp)
add_bzflag_plugin(recordmatch SOURCES plugins/recordmatch/recordmatch.cpp)
add_bzflag_plugin(regFlag SOURCES plugins/regFlag/regFlag.cpp)
add_bzflag_plugin(RogueGenocide SOURCES plugins/RogueGenocide/RogueGenocide.cpp)
add_bzflag_plugin(SAMPLE_PLUGIN USE_UTILS SOURCES plugins/SAMPLE_PLUGIN/SAMPLE_PLUGIN.cpp)
add_bzflag_plugin(serverControl USE_UTILS SOURCES plugins/serverControl/serverControl.cpp)
add_bzflag_plugin(serverSidePlayerSample USE_UTILS SOURCES plugins/serverSidePlayerSample/serverSidePlayerSample.cpp plugins/serverSidePlayerSample/playerHandler.cpp)
add_bzflag_plugin(shockwaveDeath SOURCES plugins/shockwaveDeath/shockwaveDeath.cpp)
add_bzflag_plugin(superUser USE_UTILS SOURCES plugins/superUser/superUser.cpp)
add_bzflag_plugin(teamflagreset SOURCES plugins/teamflagreset/teamflagreset.cpp)
add_bzflag_plugin(thiefControl SOURCES plugins/thiefControl/thiefControl.cpp)
add_bzflag_plugin(timedctf SOURCES plugins/timedctf/timedctf.cpp)
add_bzflag_plugin(TimeLimit SOURCES plugins/TimeLimit/TimeLimit.cpp)
add_bzflag_plugin(wwzones SOURCES plugins/wwzones/wwzones.cpp)
endif()

# Tools
if(BUILD_TOOLS)
add_executable(bzwtransform
  tools/bzwtransform/bzwTransform.cxx
)
set_target_properties(bzwtransform PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tools/bzwtransform"
)

add_executable(modeltool
  tools/modeltool/modeltool.cxx
  tools/modeltool/Q3BSP.cxx
  tools/modeltool/SimpleTextUtils.cxx
  tools/modeltool/wavefrontOBJ.cxx
)
set_target_properties(modeltool PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tools/modeltool"
)

add_executable(TextTool2
  tools/TextTool2/TextTool2.cpp
)
set_target_properties(TextTool2 PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tools/TextTool2"
)
target_link_libraries(TextTool2 PRIVATE Freetype::Freetype PNG::PNG)

add_executable(rrlog
  misc/rrlog.cxx
)
target_include_directories(rrlog PRIVATE ${CMAKE_SOURCE_DIR}/src/bzfs)
set_target_properties(rrlog PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/misc"
)
target_link_libraries(rrlog PRIVATE
  libGame
  libNet
  libCommon
  libDate
  ZLIB::ZLIB
)
endif()

# Install rules
install(TARGETS bzfs RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
if(BUILD_CLIENT)
  install(TARGETS bzflag RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
  install(FILES data/bzflag.desktop DESTINATION "${CMAKE_INSTALL_DATADIR}/applications")
endif()
if(BUILD_BZADMIN)
  install(TARGETS bzadmin RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
endif()
install(DIRECTORY data/ DESTINATION "${CMAKE_INSTALL_DATADIR}/bzflag"
  PATTERN "bzflag.desktop" EXCLUDE
)

configure_file(${CMAKE_SOURCE_DIR}/man/bzfs.6.in ${CMAKE_BINARY_DIR}/man/bzfs.6 @ONLY)
configure_file(${CMAKE_SOURCE_DIR}/man/bzw.5.in ${CMAKE_BINARY_DIR}/man/bzw.5 @ONLY)
install(FILES ${CMAKE_BINARY_DIR}/man/bzfs.6 DESTINATION "${CMAKE_INSTALL_MANDIR}/man6")
install(FILES ${CMAKE_BINARY_DIR}/man/bzw.5 DESTINATION "${CMAKE_INSTALL_MANDIR}/man5")
if(BUILD_CLIENT)
  configure_file(${CMAKE_SOURCE_DIR}/man/bzflag.6.in ${CMAKE_BINARY_DIR}/man/bzflag.6 @ONLY)
  install(FILES ${CMAKE_BINARY_DIR}/man/bzflag.6 DESTINATION "${CMAKE_INSTALL_MANDIR}/man6")
endif()
if(BUILD_BZADMIN)
  configure_file(${CMAKE_SOURCE_DIR}/man/bzadmin.6.in ${CMAKE_BINARY_DIR}/man/bzadmin.6 @ONLY)
  install(FILES ${CMAKE_BINARY_DIR}/man/bzadmin.6 DESTINATION "${CMAKE_INSTALL_MANDIR}/man6")
endif()
