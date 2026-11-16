idf_component_register(SRCS "recognition.c" "word_service.c" "mock_provider.c"
                       INCLUDE_DIRS "include"
                       REQUIRES word_model camera
                       PRIV_REQUIRES json esp_timer
                       EMBED_TXTFILES "../../../shared/mock/recognition_cup.json"
                                      "../../../shared/mock/words.json")
