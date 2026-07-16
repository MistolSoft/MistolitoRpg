set(SPRITE_PLAYER_SRCS
    ${CMAKE_SOURCE_DIR}/assets/spirites/player/p1_walk01.c
    ${CMAKE_SOURCE_DIR}/assets/spirites/player/p1_walk02.c
    ${CMAKE_SOURCE_DIR}/assets/spirites/player/p1_walk03.c
    ${CMAKE_SOURCE_DIR}/assets/spirites/player/p1_walk04.c
    ${CMAKE_SOURCE_DIR}/assets/spirites/player/p1_walk05.c
    ${CMAKE_SOURCE_DIR}/assets/spirites/player/p1_walk06.c
    ${CMAKE_SOURCE_DIR}/assets/spirites/player/p1_walk07.c
    ${CMAKE_SOURCE_DIR}/assets/spirites/player/p1_walk08.c
    ${CMAKE_SOURCE_DIR}/assets/spirites/player/p1_walk09.c
    ${CMAKE_SOURCE_DIR}/assets/spirites/player/p1_walk10.c
)

set(SPRITE_ENEMY_SRCS
    ${CMAKE_SOURCE_DIR}/assets/spirites/enemys/slimeWalk1.c
    ${CMAKE_SOURCE_DIR}/assets/spirites/enemys/slimeWalk2.c
    ${CMAKE_SOURCE_DIR}/assets/spirites/enemys/slimeDead.c
    ${CMAKE_SOURCE_DIR}/assets/spirites/enemys/fishSwim1.c
    ${CMAKE_SOURCE_DIR}/assets/spirites/enemys/fishSwim2.c
    ${CMAKE_SOURCE_DIR}/assets/spirites/enemys/fishDead.c
    ${CMAKE_SOURCE_DIR}/assets/spirites/enemys/flyFly1.c
    ${CMAKE_SOURCE_DIR}/assets/spirites/enemys/flyFly2.c
    ${CMAKE_SOURCE_DIR}/assets/spirites/enemys/flyDead.c
    ${CMAKE_SOURCE_DIR}/assets/spirites/enemys/blockerBody.c
    ${CMAKE_SOURCE_DIR}/assets/spirites/enemys/blockerMad.c
    ${CMAKE_SOURCE_DIR}/assets/spirites/enemys/blockerSad.c
    ${CMAKE_SOURCE_DIR}/assets/spirites/enemys/pokerMad.c
    ${CMAKE_SOURCE_DIR}/assets/spirites/enemys/pokerSad.c
)

set(TILE_SRCS
    ${CMAKE_SOURCE_DIR}/assets/Tiles/c/grassMid.c
)

idf_component_register(
SRCS
"events.c"
"game_coordinator.c"
"search_engine.c"
"workers.c"
"storage_task.c"
"display_task.c"
"screens.c"
"training_mode.c"
"usb_init.c"
"lcd_init.c"
"rules.c"
"sprites.c"
"spi_bus.c"
"combat_engine.c"
"rest_engine.c"
"animation_system.c"
"animation_loops.c"
"dna_engine.c"
"profession_engine.c"
"skills_perks_engine.c"
"resources_engine.c"
"features_engine.c"
"spell_engine.c"
"reward_calculator.c"
"experience_logger.c"
"ppo_trainer.c"
"exploration_manager.c"
"skill_resolver.c"
"critic_features.c"
"critic_model.c"
"policy_head.c"
"value_head.c"
"brain_registry.c"
${SPRITE_PLAYER_SRCS}
${SPRITE_ENEMY_SRCS}
${TILE_SRCS}
INCLUDE_DIRS "."
REQUIRES mbedtls
PRIV_REQUIRES esp_lcd spi_flash sdmmc fatfs freertos driver esp_timer cjson lvgl esp_driver_usb_serial_jtag inference_engine
)

target_compile_definitions(${COMPONENT_LIB} PRIVATE LV_LVGL_H_INCLUDE_SIMPLE=1)
